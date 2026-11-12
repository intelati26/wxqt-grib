// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/NbmText.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <mutex>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <regex>
#include "objects/URL.h"
#include "util/UtilityIO.h"

namespace {
    constexpr long long window = 4096;
    const char * const bucket = "https://noaa-nbm-grib2-pds.s3.amazonaws.com";

    std::string trim(const std::string& s) {
        const auto a = s.find_first_not_of(" \t\r");
        return a == std::string::npos ? std::string{} : s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
    }

    // every header of a piece of the file: where it starts (the space after the line break) and its station
    struct Header {
        size_t at;
        std::string station;
    };
    std::vector<Header> headers(const std::string& piece) {
        static const std::regex pattern{"(^|\n) ([A-Za-z0-9]+) +NBM +NB[HSEXP] +GUIDANCE"};
        std::vector<Header> out;
        for (std::sregex_iterator it{piece.begin(), piece.end(), pattern}, end; it != end; ++it) {
            out.push_back({static_cast<size_t>(it->position(0)) + static_cast<size_t>(it->length(1)), (*it)[2].str()});
        }
        return out;
    }

    std::string piece(const NbmText::File& file, long long first, long long last, const NbmText::Range& range) {
        last = std::min(last, file.size - 1);
        if (first > last) {
            return {};
        }
        std::string text = range(file.url, first, last).toStdString();
        if (first == 0) {
            text = "\n" + text;   // (the first header has no line break before it)
        }
        if (last >= file.size - 1) {
            text += "\n ~END~ NBM  " + file.kind + " GUIDANCE\n";   // (the last station has an end too)
        }
        return text;
    }

    // the station's block cut out of a piece: from its header to the next one (which the piece has to hold); "" when it is not there or has no end in the piece
    std::string cut(const std::string& text, const std::string& station) {
        const auto list = headers(text);
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].station == station && i + 1 < list.size()) {
                return text.substr(list[i].at, list[i + 1].at - list[i].at);
            }
        }
        return {};
    }

    struct Sample {
        long long offset;
        std::string station;
    };

    // The index of a file, kept for the session: the first header of 32 windows spread over it
    std::vector<Sample> indexOf(const NbmText::File& file, const NbmText::Range& range) {
        static std::mutex mutex;
        static std::map<std::string, std::vector<Sample>> kept;
        {
            const std::lock_guard lock{mutex};
            if (const auto f = kept.find(file.url); f != kept.end()) {
                return f->second;
            }
        }
        std::vector<Sample> out;
        const long long step = file.size / 32;
        for (int n = 0; n < 32; n++) {
            const long long start = n * step;
            const auto text = piece(file, start, start + window - 1, range);
            const auto list = headers(text);
            if (!list.empty()) {
                out.push_back({start + static_cast<long long>(list.front().at) - (start == 0 ? 1 : 0), list.front().station});
            }
        }
        if (out.size() >= 16) {
            const std::lock_guard lock{mutex};
            kept[file.url] = out;
        }
        return out;
    }

    // between two offsets where the ids rise: bisect on the first header of a window at the middle, then cut the bracket out
    std::string bisect(const NbmText::File& file, const std::string& station, long long lo, long long hi, const NbmText::Range& range) {
        while (hi - lo > 3 * window) {
            const long long mid = lo + (hi - lo) / 2;
            const auto list = headers(piece(file, mid, mid + window - 1, range));
            if (list.empty()) {
                hi = mid;
                continue;
            }
            const long long at = mid + static_cast<long long>(list.front().at);
            if (list.front().station == station) {
                lo = at;
                break;
            }
            (list.front().station < station ? lo : hi) = at;
        }
        return cut(piece(file, lo, hi + 2 * window, range), station);
    }

    // an interval where the order breaks (a new sorted run starts in it): read through it in 96 KB chunks that overlap by 2 windows, to one window past its end (a station just before the next
    // run's first header has no end otherwise)
    std::string scan(const NbmText::File& file, const std::string& station, long long from, long long to, const NbmText::Range& range) {
        constexpr long long chunk = 96 * 1024;
        for (long long at = from; at <= to + window && at < file.size; at += chunk - 2 * window) {
            if (const auto block = cut(piece(file, at, at + chunk - 1, range), station); !block.empty()) {
                return block;
            }
        }
        return {};
    }
}

namespace NbmText {
    bool firstHeader(const std::string& text, size_t from, size_t& at, std::string& station) {
        const auto list = headers(from == 0 ? text : text.substr(from));
        if (list.empty()) {
            return false;
        }
        at = from + list.front().at;
        station = list.front().station;
        return true;
    }

    Block parse(const std::string& text, const std::string& source) {
        Block block;
        static const std::regex header{"^\\s*(\\S+)\\s+NBM\\s+(NB[HSEXP])\\s+GUIDANCE\\s+(\\d+)/(\\d+)/(\\d+)\\s+(\\d\\d)(\\d\\d)\\s+UTC"};
        std::vector<std::string> lines;
        for (size_t from = 0; from <= text.size();) {
            auto end = text.find('\n', from);
            end = end == std::string::npos ? text.size() : end;
            lines.push_back(text.substr(from, end - from));
            from = end + 1;
        }
        size_t first = 0;
        while (first < lines.size() && trim(lines[first]).empty()) {
            first++;
        }
        std::smatch m;
        if (first >= lines.size() || !std::regex_search(lines[first], m, header)) {
            return block;
        }
        block.station = m[1];
        block.kind = m[2];
        const QDateTime cycle{QDate{std::stoi(m[5]), std::stoi(m[3]), std::stoi(m[4])}, QTime{std::stoi(m[6]), std::stoi(m[7])}, QTimeZone::utc()};
        block.cycle = cycle.toSecsSinceEpoch();
        // the FHR line gives the columns: the last digit of each number is where the 3 characters of a value end
        std::vector<std::pair<size_t, int>> where;
        size_t row = first + 1;
        for (; row < lines.size(); row++) {
            if (trim(lines[row]).compare(0, 3, "FHR") == 0) {
                const auto& line = lines[row];
                for (size_t i = 4; i < line.size();) {
                    if (std::isdigit(static_cast<unsigned char>(line[i]))) {
                        size_t j = i;
                        while (j < line.size() && std::isdigit(static_cast<unsigned char>(line[j]))) {
                            j++;
                        }
                        where.emplace_back(j - 1, std::stoi(line.substr(i, j - i)));
                        i = j;
                    } else {
                        i++;
                    }
                }
                break;
            }
        }
        if (where.empty()) {
            return block;
        }
        const std::string tag = source.empty() ? block.kind : source;
        block.columns.resize(where.size());
        for (size_t c = 0; c < where.size(); c++) {
            block.columns[c].time = block.cycle + static_cast<qint64>(where[c].second) * 3600;
            block.columns[c].source = tag;
        }
        for (row++; row < lines.size(); row++) {
            const auto& line = lines[row];
            if (line.size() < 5) {
                continue;
            }
            const auto code = trim(line.substr(0, 4));
            if (code.size() != 3 || !std::all_of(code.begin(), code.end(), [] (unsigned char ch) { return std::isupper(ch) || std::isdigit(ch); })) {
                continue;
            }
            for (size_t c = 0; c < where.size(); c++) {
                const size_t last = where[c].first;
                if (last >= line.size() || last < 2) {
                    continue;
                }
                const auto cell = trim(line.substr(last - 2, 3));
                char * end = nullptr;
                const long v = std::strtol(cell.c_str(), &end, 10);
                if (cell.empty() || end == cell.c_str() || *end != '\0' || v == -99) {
                    continue;
                }
                block.columns[c].value[code] = static_cast<int>(v);
            }
        }
        return block;
    }

    std::string findBlock(const File& file, const std::string& station, const Range& range) {
        if (file.size < 32 * window) {
            return cut(piece(file, 0, file.size - 1, range), station);   // (a small file: all of it)
        }
        const auto samples = indexOf(file, range);
        if (samples.size() < 16) {
            return {};
        }
        const size_t n = samples.size();
        std::vector<size_t> breaks, small;
        for (size_t i = 0; i < n; i++) {
            const bool last = i + 1 == n;
            const long long lo = samples[i].offset, hi = last ? file.size : samples[i + 1].offset;
            const bool rising = !last && samples[i].station <= samples[i + 1].station;
            if ((last || rising) && samples[i].station <= station && (last || station <= samples[i + 1].station)) {
                if (const auto block = bisect(file, station, lo, hi, range); !block.empty()) {
                    return block;
                }
            }
            if (!rising) {
                breaks.push_back(i);
            } else if (hi - lo <= 1024 * 1024) {
                small.push_back(i);
            }
        }
        for (const auto& list : {breaks, small}) {
            for (const auto i : list) {
                const long long lo = samples[i].offset, hi = i + 1 == n ? file.size : samples[i + 1].offset;
                if (const auto block = scan(file, station, lo, hi, range); !block.empty()) {
                    return block;
                }
            }
        }
        return {};
    }

    bool newestFile(const std::string& kind, qint64 now, File& out) {
        static const std::regex entry{"<Key>([^<]+)</Key>.*?<Size>(\\d+)</Size>"};
        for (int back = 0; back <= 12; back++) {
            const auto t = QDateTime::fromSecsSinceEpoch(now - back * 3600LL, QTimeZone::utc());
            const auto day = t.toString("yyyyMMdd").toStdString(), hour = t.toString("HH").toStdString();
            const auto prefix = "blend." + day + "/" + hour + "/text/blend_" + kind + ".t" + hour + "z";
            const auto listing = URL::getText(std::string{bucket} + "/?list-type=2&max-keys=1&prefix=" + prefix);
            std::smatch m;
            if (std::regex_search(listing, m, entry) && m[1].str() == prefix) {
                out.url = std::string{bucket} + "/" + prefix;
                out.kind = kind.substr(0, 3);
                std::transform(out.kind.begin(), out.kind.end(), out.kind.begin(), [] (unsigned char c) { return std::toupper(c); });
                out.size = std::stoll(m[2]);
                return true;
            }
        }
        return false;
    }

    std::vector<Column> gapColumns(const UtilityForecastPoint::Data& grid, qint64 from, qint64 to) {
        const auto at = [&grid] (const char * key, qint64 t, double& v) {
            const auto f = grid.hourly.find(key);
            if (f == grid.hourly.end()) {
                return false;
            }
            for (const auto& [time, value] : f->second) {
                if (time == t) {
                    v = value;
                    return true;
                }
            }
            return false;
        };
        std::vector<Column> out;
        for (qint64 t = (from + 3 * 3600 - 1) / (3 * 3600) * (3 * 3600); t < to; t += 3 * 3600) {
            Column c;
            c.time = t;
            c.source = "API";
            double v = 0;
            if (at("temperature", t, v)) {
                c.value["TMP"] = static_cast<int>(std::lround(v));
            }
            if (at("dewpoint", t, v)) {
                c.value["DPT"] = static_cast<int>(std::lround(v));
            }
            if (at("skyCover", t, v)) {
                c.value["SKY"] = static_cast<int>(std::lround(v));
            }
            if (at("windSpeed", t, v)) {
                c.value["WSP"] = static_cast<int>(std::lround(v * 0.868976));   // (the page's mph to knots, the bulletin's unit)
            }
            if (at("windGust", t, v)) {
                c.value["GST"] = static_cast<int>(std::lround(v * 0.868976));
            }
            if (at("windDirection", t, v)) {
                const int tens = static_cast<int>(std::lround(v / 10.0)) % 36;
                c.value["WDR"] = tens == 0 ? 36 : tens;
            }
            if (c.has("TMP")) {
                out.push_back(std::move(c));
            }
        }
        return out;
    }

    std::vector<Column> merge(const std::vector<Column>& api, const std::vector<Column>& nbs, const std::vector<Column>& nbe, const std::vector<Column>& nbx, qint64 fromEpoch) {
        std::vector<Column> out;
        qint64 last = 0;
        const auto add = [&] (const std::vector<Column>& list) {
            qint64 newest = last;
            for (const auto& c : list) {
                if (c.time <= last || c.time < fromEpoch) {
                    continue;
                }
                if (c.source != "API" && !c.has("TMP") && !c.has("SKY") && !c.has("WSP") && !c.has("P12")) {
                    continue;   // (the 13Z run's NBX lists days with nothing in them)
                }
                out.push_back(c);
                newest = std::max(newest, c.time);
            }
            last = std::max(last, newest);
        };
        add(api);
        add(nbs);
        add(nbe);
        add(nbx);
        return out;
    }

    namespace {
        std::string truncated(double v) {
            return QString::number(std::trunc(v * 10000.0) / 10000.0, 'f', 4).toStdString();
        }
    }

    Table fetch(const UtilityForecastPoint::Data& grid) {
        Table table;
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        const auto point = QJsonDocument::fromJson(QByteArray::fromStdString(UtilityIO::getHtml("https://api.weather.gov/points/" + truncated(grid.lat) + "," + truncated(grid.lon)))).object().value("properties").toObject();
        const auto stationsUrl = point.value("observationStations").toString().toStdString();
        if (stationsUrl.empty()) {
            table.error = "The NWS has no observing stations for this point.";
            return table;
        }
        struct Station {
            std::string id, name;
        };
        std::vector<Station> stations;
        for (const auto& f : QJsonDocument::fromJson(QByteArray::fromStdString(UtilityIO::getHtml(stationsUrl))).object().value("features").toArray()) {
            const auto p = f.toObject().value("properties").toObject();
            stations.push_back({p.value("stationIdentifier").toString().toStdString(), p.value("name").toString().toStdString()});
            if (stations.size() == 6) {
                break;
            }
        }
        const auto range = [] (const std::string& url, long long a, long long b) { return URL::getBytesRangeManaged(url, a, b); };
        std::vector<std::future<bool>> found;
        File files[3];
        const char * kinds[3] = {"nbstx", "nbetx", "nbxtx"};
        for (int i = 0; i < 3; i++) {
            found.push_back(std::async(std::launch::async, [&files, &kinds, i, now] { return newestFile(kinds[i], now, files[i]); }));
        }
        bool have[3];
        for (int i = 0; i < 3; i++) {
            have[i] = found[static_cast<size_t>(i)].get();
        }
        if (!have[0]) {
            table.error = "No National Blend of Models bulletin was found in the last 12 hours.";
            return table;
        }
        // the station: the first of the nearest few that the short range file has
        Block blocks[3];
        for (const auto& s : stations) {
            if (const auto text = findBlock(files[0], s.id, range); !text.empty()) {
                blocks[0] = parse(text);
                if (!blocks[0].columns.empty()) {
                    table.station = s.id;
                    table.name = s.name;
                    break;
                }
            }
        }
        if (table.station.empty()) {
            table.error = "None of the nearest observing stations is in the National Blend of Models bulletins.";
            return table;
        }
        std::vector<std::future<void>> more;
        for (int i = 1; i < 3; i++) {
            if (have[i]) {
                more.push_back(std::async(std::launch::async, [&, i] {
                    if (const auto text = findBlock(files[i], table.station, range); !text.empty()) {
                        blocks[i] = parse(text);
                    }
                }));
            }
        }
        for (auto& f : more) {
            f.get();
        }
        const qint64 from = now - 3 * 3600;
        const qint64 firstNbs = blocks[0].columns.empty() ? now : blocks[0].columns.front().time;
        table.columns = merge(gapColumns(grid, from, firstNbs), blocks[0].columns, blocks[1].columns, blocks[2].columns, from);
        const char * names[3] = {"NBS", "NBE", "NBX"};
        for (int i = 0; i < 3; i++) {
            if (!blocks[i].columns.empty()) {
                table.cycles[names[i]] = blocks[i].cycle;
            }
        }
        table.gridUpdated = grid.updated.isValid() ? grid.updated.toSecsSinceEpoch() : 0;
        table.ok = !table.columns.empty();
        if (!table.ok) {
            table.error = "The bulletins have no columns for this station.";
        }
        return table;
    }
}
