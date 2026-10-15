// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "sounding/UtilityBufkit.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTime>
#include <QTimeZone>
#include "common/GlobalVariables.h"
#include "util/UtilityIO.h"
#include "util/WfoSites.h"

namespace {
    std::string lower(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // the value after "KEY =" on a line of "KEY = value KEY2 = value2" pairs
    std::string valueOf(const std::string& text, const std::string& key) {
        const auto at = text.find(key + " =");
        if (at == std::string::npos) {
            return {};
        }
        std::istringstream words{text.substr(at + key.size() + 2)};
        std::string word;
        words >> word;
        return word;
    }

    bool isNumber(const std::string& token) {
        char * end = nullptr;
        std::strtod(token.c_str(), &end);
        return end != token.c_str() && *end == '\0';
    }
}

namespace UtilityBufkit {

const std::set<std::string>& officeStations() {
    static const std::set<std::string> list = [] {
        std::set<std::string> out;
        if (WfoSites::sites == nullptr) {
            return out;
        }
        const auto& all = stations();
        for (const auto& wfo : WfoSites::sites->sites) {
            const double lat = wfo.latLon.lat(), lon = wfo.latLon.lon();
            const std::string code = wfo.codeName;
            const Station * best = nullptr;
            double bestDistance = 0.3;
            for (const auto& s : all) {
                if (s.id == "K" + code) {
                    best = &s;
                    bestDistance = -1.0;
                    break;
                }
                if (s.id.size() != 4 || s.id[0] != 'K') {
                    continue;
                }
                const double d = std::hypot(s.latitude - lat, (s.longitude - lon) * std::cos(lat * 3.14159265358979 / 180.0));
                if (d < bestDistance) {
                    bestDistance = d;
                    best = &s;
                }
            }
            if (best != nullptr) {
                out.insert(best->id);
            }
        }
        return out;
    }();
    return list;
}

const std::vector<Station>& stations() {
    static const std::vector<Station> list = [] {
        std::vector<Station> out;
        for (const auto& line : UtilityIO::rawFileToStringArray(GlobalVariables::resDir + "bufkit_stations.txt")) {
            std::istringstream parts{line};
            std::string id, lat, lon;
            if (std::getline(parts, id, ',') && std::getline(parts, lat, ',') && std::getline(parts, lon, ',') && !id.empty()) {
                out.push_back({id, std::atof(lat.c_str()), std::atof(lon.c_str())});
            }
        }
        return out;
    }();
    return list;
}

bool parse(const std::string& text, Run& out, std::string& error) {
    out.hours.clear();
    out.validTimes.clear();
    out.profiles.clear();
    // each forecast time starts at "STID = "; what comes before is the parameter list
    size_t at = text.find("STID =");
    if (at == std::string::npos) {
        error = "not a BUFKIT file";
        return false;
    }
    while (at != std::string::npos) {
        const auto next = text.find("STID =", at + 6);
        const std::string block = text.substr(at, next == std::string::npos ? std::string::npos : next - at);
        at = next;
        const auto time = valueOf(block, "TIME");
        const auto stim = valueOf(block, "STIM");
        std::vector<std::string> names;
        std::vector<double> numbers;
        std::istringstream lines{block};
        std::string line;
        bool inColumns = false;
        while (std::getline(lines, line)) {
            if (line.empty()) {
                continue;
            }
            std::istringstream words{line};
            std::vector<std::string> tokens;
            std::string token;
            while (words >> token) {
                tokens.push_back(token);
            }
            if (tokens.empty()) {
                continue;
            }
            if (!inColumns) {
                // the column names: the line that starts with PRES, and the line or two of names after it
                if (tokens[0] == "PRES") {
                    inColumns = true;
                    names = tokens;
                }
                continue;
            }
            if (std::all_of(tokens.begin(), tokens.end(), [] (const std::string& t) { return !isNumber(t); }) && numbers.empty()) {
                names.insert(names.end(), tokens.begin(), tokens.end());   // CFRL HGHT on a second line
                continue;
            }
            for (const auto& t : tokens) {
                if (isNumber(t)) {
                    numbers.push_back(std::strtod(t.c_str(), nullptr));
                }
            }
        }
        const auto column = [&names] (const char * name) {
            const auto found = std::find(names.begin(), names.end(), name);
            return found == names.end() ? -1 : static_cast<int>(found - names.begin());
        };
        const int ip = column("PRES"), it = column("TMPC"), id = column("DWPC"), idir = column("DRCT"), ispd = column("SKNT"), ih = column("HGHT");
        if (ip < 0 || it < 0 || id < 0 || idir < 0 || ispd < 0 || ih < 0 || names.empty()) {
            continue;
        }
        const size_t width = names.size();
        SoundingProfile profile;
        for (size_t i = 0; i + width <= numbers.size(); i += width) {
            const double pres = numbers[i + static_cast<size_t>(ip)];
            if (pres <= 0.0) {
                continue;
            }
            profile.pres.push_back(pres);
            profile.hght.push_back(numbers[i + static_cast<size_t>(ih)]);
            profile.tmpc.push_back(numbers[i + static_cast<size_t>(it)]);
            profile.dwpc.push_back(numbers[i + static_cast<size_t>(id)]);
            profile.wdir.push_back(numbers[i + static_cast<size_t>(idir)]);
            profile.wspd.push_back(numbers[i + static_cast<size_t>(ispd)]);
        }
        if (profile.pres.size() < 8) {
            continue;
        }
        if (out.profiles.empty()) {
            out.station = valueOf(block, "STID");
            out.latitude = std::strtod(valueOf(block, "SLAT").c_str(), nullptr);
            out.longitude = std::strtod(valueOf(block, "SLON").c_str(), nullptr);
            out.elevation = std::strtod(valueOf(block, "SELV").c_str(), nullptr);
            out.runTime = time;
        }
        profile.station = out.station;
        profile.latitude = out.latitude;
        profile.validTime = time;
        profile.finalize();
        out.hours.push_back(std::atoi(stim.c_str()));
        out.validTimes.push_back(time);
        out.profiles.push_back(std::move(profile));
    }
    if (out.profiles.empty()) {
        error = "no profiles in the BUFKIT file";
        return false;
    }
    // the run's own time is the one at hour 0 (the TIME of each block is its valid time)
    out.runTime = out.validTimes.front();
    out.cycle = out.runTime.size() >= 9 ? out.runTime.substr(7, 2) : std::string{};
    return true;
}

namespace {
    // the compressed copies of the files read: one per station and model, in the folder of the GRIB cache, named bk_<station>_<run yymmdd-hhmm>_<model>.z
    QString cacheFolder() {
        const auto path = QDir::tempPath() + "/wxqt_grib";
        QDir{}.mkpath(path);
        return path;
    }

    QDateTime runDateTime(const std::string& runTime) {   // "261015/0600" -> UTC
        if (runTime.size() < 11) {
            return {};
        }
        return QDateTime{QDate{2000 + std::atoi(runTime.substr(0, 2).c_str()), std::atoi(runTime.substr(2, 2).c_str()), std::atoi(runTime.substr(4, 2).c_str())},
                         QTime{std::atoi(runTime.substr(7, 2).c_str()), std::atoi(runTime.substr(9, 2).c_str())}, QTimeZone::utc()};
    }
}

bool fetchLatest(const std::string& model, const std::string& station, Run& out, std::string& error) {
    // a read file is kept (compressed) until the next cycle of the model can be out: the run's time plus its 6 hours plus the 3 hours and a half the feed takes to publish
    const QString folder = cacheFolder();
    const QString upper = QString::fromStdString(station).toUpper(), modelName = QString::fromStdString(model).toUpper();
    {
        QDir dir{folder};
        auto names = dir.entryList({"bk_" + upper + "_*_" + modelName + ".z"}, QDir::Files, QDir::Name);
        if (!names.isEmpty()) {
            const QString name = names.last();
            const QString run = name.mid(4 + upper.size() + 1, 11).replace('-', '/');
            const auto runAt = runDateTime(run.toStdString());
            QFile file{dir.filePath(name)};
            if (runAt.isValid() && QDateTime::currentDateTimeUtc() < runAt.addSecs(static_cast<qint64>(9.5 * 3600.0)) && file.open(QIODevice::ReadOnly)) {
                const auto text = qUncompress(file.readAll());
                std::string e;
                Run cached;
                if (!text.isEmpty() && parse(text.toStdString(), cached, e)) {
                    cached.model = model;
                    out = cached;
                    return true;
                }
            }
        }
    }
    // the cycles the feed carries (RRFS: 00 06 12 18). A cycle's folder holds its last run, so one not yet published still holds yesterday's:
    // the two most recent cycles are read and the later run kept.
    const int latest = (QDateTime::currentDateTimeUtc().addSecs(-2 * 3600).time().hour() / 6) * 6;   // runs appear a few hours after their time
    bool found = false;
    std::string foundText;
    for (int back = 0; back < 2; back++) {
        const int hour = ((latest - 6 * back) % 24 + 24) % 24;
        char cycle[3];
        std::snprintf(cycle, sizeof(cycle), "%02d", hour);
        const std::string url = "https://www.meteo.psu.edu/bufkit/data/" + model + "/" + cycle + "/" + lower(model) + "_" + lower(station) + ".buf";
        const auto text = UtilityIO::getHtml(url);
        Run candidate;
        std::string e;
        if (text.empty() || !parse(text, candidate, e)) {
            continue;
        }
        candidate.model = model;
        if (!found || candidate.runTime > out.runTime) {
            out = candidate;
            foundText = text;
            found = true;
        }
    }
    if (!found) {
        error = "no BUFKIT " + model + " file for " + station + " (stations are airports and sounding sites, e.g. KSGF)";
        return false;
    }
    // keep it compressed (about a third of the text), and let go of what is from an older run: this station's earlier file, and any other station's of the model
    // from a run before this one, as the GRIB cache drops the slices of old runs
    QString runName = QString::fromStdString(out.runTime);
    runName.replace('/', '-');
    const QString mine = "bk_" + upper + "_" + runName + "_" + modelName + ".z";
    QDir dir{folder};
    for (const auto& name : dir.entryList({"bk_*_" + modelName + ".z"}, QDir::Files)) {
        const auto parts = name.split('_');   // bk, station, run, model.z
        if (name != mine && (parts.size() >= 4 && (parts[1] == upper || parts[2] < runName))) {
            dir.remove(name);
        }
    }
    QFile part{dir.filePath(mine + ".part")};
    if (part.open(QIODevice::WriteOnly)) {
        part.write(qCompress(QByteArray::fromRawData(foundText.data(), static_cast<qsizetype>(foundText.size())), 6));
        part.close();
        QFile::remove(dir.filePath(mine));
        part.rename(dir.filePath(mine));
    }
    return true;
}

}  // namespace UtilityBufkit
