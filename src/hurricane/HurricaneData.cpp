// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HurricaneData.h"
#include <algorithm>
#include <cstdio>
#include <cctype>
#include <ctime>
#include <mutex>
#include <regex>
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include "hurricane/UtilityEnsembleStats.h"
#include "hurricane/UtilityNhcText.h"
#include "hurricane/UtilityVdm.h"
#include "util/UtilityGzip.h"
#include "util/UtilityIO.h"

namespace {
    const string atcf = "https://ftp.nhc.noaa.gov/atcf/";

    string download(const string& url) {
        return UtilityIO::downloadAsByteArray(url).toStdString();
    }

    string text(const QJsonObject& object, const char * key) {
        return object.value(key).toString().toStdString();
    }

    int toInt(const QJsonObject& object, const char * key) {
        const auto value = object.value(key);
        return value.isString() ? value.toString().toInt() : value.toInt(-1);
    }
}

string HurricaneData::idLabel(const string& id) {
    if (id.size() < 8) {
        return id;
    }
    string label = id.substr(0, 4);
    std::transform(label.begin(), label.end(), label.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return label;
}

bool HurricaneData::loadStormList(vector<StormEntry>& entries, string& error, const string& basin) {
    entries.clear();
    const auto json = download("https://www.nhc.noaa.gov/CurrentStorms.json");
    if (json.empty()) {
        error = "Could not read the NHC storm list.";
        return false;
    }
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    std::vector<string> activeIds;
    const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    for (const auto& value : document.object().value("activeStorms").toArray()) {
        const auto o = value.toObject();
        StormEntry entry;
        entry.id = text(o, "id");
        if (entry.id.rfind(basin, 0) != 0) {
            continue;   // the chosen basin only
        }
        entry.active = true;
        entry.name = text(o, "name");
        entry.classification = text(o, "classification");
        entry.wind = toInt(o, "intensity");
        entry.pressure = toInt(o, "pressure");
        entry.lat = o.value("latitudeNumeric").toDouble();
        entry.lon = o.value("longitudeNumeric").toDouble();
        entry.movementDir = toInt(o, "movementDir");
        entry.movementSpeed = toInt(o, "movementSpeed");
        entry.lastUpdate = text(o, "lastUpdate");
        entry.discussionUrl = text(o.value("forecastDiscussion").toObject(), "url");
        entry.advisoryUrl = text(o.value("publicAdvisory").toObject(), "url");
        entry.forecastAdvisoryUrl = text(o.value("forecastAdvisory").toObject(), "url");
        entry.probabilitiesUrl = text(o.value("windSpeedProbabilities").toObject(), "url");
        entry.graphicsUrl = text(o.value("forecastGraphics").toObject(), "url");
        const auto cone = o.value("trackCone").toObject();
        entry.advNum = text(cone, "advNum");
        entry.coneZip = text(cone, "zipFile");
        entry.radiiZip = text(o.value("forecastWindRadiiGIS").toObject(), "zipFile");
        const auto watches = o.value("windWatchesWarnings").toObject();
        entry.watchKmz = text(watches, "kmzFile");
        if (entry.watchKmz.empty() && !entry.advNum.empty()) {
            // NHC names it like the cone file: AL092026_003adv_WW.kmz (the list names it only while watches are in effect)
            string upper = entry.id;
            std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
            entry.watchKmz = "https://www.nhc.noaa.gov/storm_graphics/api/" + upper + "_" + entry.advNum + "adv_WW.kmz";
        }
        entry.label = entry.name + " (" + entry.classification + ", " + (entry.wind >= 0 ? std::to_string(entry.wind) + " kt" : string{"-"}) + ") - " + idLabel(entry.id);
        activeIds.push_back(entry.id);
        entries.push_back(entry);
    }
    // the rest of the season from the btk/ directory: invests (numbers 90-99) touched in the last two days, and the finished storms
    const auto listing = download(atcf + "btk/");
    const std::regex row{"href=\"(b" + basin + R"re((\d\d)(\d{4})\.dat)"[^\n]*?(\d{4}-\d\d-\d\d \d\d:\d\d))re"};
    const auto now = QDateTime::currentDateTimeUtc();
    vector<StormEntry> rest;
    for (std::sregex_iterator it{listing.begin(), listing.end(), row}, end; it != end; ++it) {
        const string number = (*it)[2];
        const string fileYear = (*it)[3];
        if (std::stoi(fileYear) != year) {
            continue;
        }
        StormEntry entry;
        entry.id = basin + number + fileYear;
        if (std::find(activeIds.begin(), activeIds.end(), entry.id) != activeIds.end()) {
            continue;
        }
        const auto modified = QDateTime::fromString(QString::fromStdString((*it)[4]), "yyyy-MM-dd HH:mm");
        const bool recent = modified.isValid() && modified.secsTo(now) < 2 * 86400;
        if (std::stoi(number) >= 90) {
            if (!recent) {
                continue;   // an old invest
            }
            entry.active = true;
            entry.label = "Invest " + number + (basin == "al" ? "L" : basin == "ep" ? "E" : "C") + " (" + idLabel(entry.id) + ")";
        } else {
            string up = basin;
            std::transform(up.begin(), up.end(), up.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
            entry.label = up + number + " " + fileYear + (recent ? " (recent)" : " (finished)");
        }
        rest.push_back(entry);
    }
    std::stable_sort(rest.begin(), rest.end(), [] (const auto& a, const auto& b) {
        return a.active != b.active ? a.active : a.id > b.id;   // invests first, then the newest storm
    });
    entries.insert(entries.end(), rest.begin(), rest.end());
    if (entries.empty()) {
        error = "No storms in the NHC lists for this basin.";
        return false;
    }
    return true;
}

void HurricaneData::loadStorm(const string& id, StormData& data) {
    data = StormData{};
    data.id = id;
    const auto best = download(atcf + "btk/b" + id + ".dat");
    data.best = UtilityAtcf::bestTrack(best);
    if (!data.best.empty()) {
        for (auto it = data.best.rbegin(); it != data.best.rend(); ++it) {
            if (!it->name.empty()) {
                data.name = it->name;
                break;
            }
        }
    }
    const auto forecast = download(atcf + "fst/" + id + ".fst");
    for (auto& track : UtilityAtcf::latestTracks(forecast)) {
        if (track.tech == "OFCL") {
            data.official = track;
        }
    }
    std::string guidance;
    const auto zipped = download(atcf + "aid_public/a" + id + ".dat.gz");
    if (!zipped.empty() && !UtilityGzip::gunzip(zipped, guidance)) {
        data.error = "The model guidance file could not be unpacked.";
    }
    for (auto& track : UtilityAtcf::latestTracks(guidance)) {
        if (track.tech == "OFCL") {
            if (data.official.fixes.empty()) {
                data.official = track;
            }
        } else {
            data.guidance.push_back(track);
        }
    }
    static std::mutex mutex;
    static std::map<string, string> names;
    {
        std::lock_guard lock{mutex};
        if (names.empty()) {
            names = UtilityAtcf::parseTechList(download(atcf + "docs/nhc_techlist.dat"));
        }
        data.longNames = names;
    }
    // codes that NHC's own list does not name yet
    static const std::pair<const char *, const char *> extra[] = {
        {"HFSA", "HAFS-A (NOAA Hurricane Analysis and Forecast System)"}, {"HFSB", "HAFS-B (NOAA Hurricane Analysis and Forecast System)"},
        {"GDMN", "Google DeepMind WeatherNext cyclone model (experimental)"}, {"GDMI", "Google DeepMind WeatherNext cyclone model, interpolated (experimental)"},
        {"AIFS", "ECMWF AIFS (AI forecasting system)"}, {"AIFI", "ECMWF AIFS, interpolated"}};
    for (const auto& [code, name] : extra) {
        data.longNames.emplace(code, name);
    }
    if (data.best.empty() && data.guidance.empty() && data.error.empty()) {
        data.error = "NHC has no track data for this storm yet.";
    }
}

void HurricaneData::loadRecon(ReconData& data, int bulletins, const string& basin) {
    data = ReconData{};
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    // the high density observations: AHONT1 Atlantic, AHOPN1 Eastern Pacific, AHOPA1 Central Pacific (the bulletin codes of the recon archive)
    const string code = basin == "ep" ? "AHOPN1" : basin == "cp" ? "AHOPA1" : "AHONT1";
    const auto folder = "https://www.nhc.noaa.gov/archive/recon/" + std::to_string(year) + "/" + code + "/";
    const auto listing = download(folder);
    const std::regex file{"href=\"(" + code + R"re(-[A-Z]+\.(\d{12})\.txt)")re"};
    vector<std::pair<string, string>> files;   // time, name
    for (std::sregex_iterator it{listing.begin(), listing.end(), file}, end; it != end; ++it) {
        files.emplace_back((*it)[2], (*it)[1]);
    }
    if (files.empty()) {
        data.error = "Could not read the NHC reconnaissance archive.";
        return;
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    const size_t first = files.size() > static_cast<size_t>(bulletins) ? files.size() - static_cast<size_t>(bulletins) : 0;
    for (size_t i = first; i < files.size(); i++) {
        auto parsed = UtilityHdob::parse(download(folder + files[i].second));
        data.messages.insert(data.messages.end(), parsed.begin(), parsed.end());
    }
}

string HurricaneData::ecmwfId(const string& id) {
    if (id.size() < 8) {
        return id;
    }
    static const std::map<string, char> letters{{"al", 'L'}, {"ep", 'E'}, {"cp", 'C'}};
    const auto found = letters.find(id.substr(0, 2));
    return id.substr(2, 2) + string(1, found != letters.end() ? found->second : '?');
}

namespace {
    // a download kept for 20 minutes (the ensembles are a megabyte or more each, and the screen asks again on every storm pick)
    string cachedDownload(const string& url) {
        static std::mutex mutex;
        static std::map<string, std::pair<qint64, string>> cache;
        const auto now = QDateTime::currentSecsSinceEpoch();
        {
            std::lock_guard lock{mutex};
            const auto found = cache.find(url);
            if (found != cache.end() && now - found->second.first < 1200) {
                return found->second.second;
            }
        }
        auto bytes = download(url);
        if (bytes.size() > 1000 && bytes.compare(0, 4, "BUFR") == 0) {
            std::lock_guard lock{mutex};
            cache[url] = {now, bytes};
            return bytes;
        }
        return {};
    }
}

void HurricaneData::loadEnsembles(const string& nhcId, EnsembleData& data) {
    data = EnsembleData{};
    struct Model {
        const char * label;
        const char * model;
        const char * stream;
    };
    static const Model models[] = {{"AIFS ENS", "aifs-ens", "enfo"}, {"IFS ENS", "ifs", "enfo"}, {"AIFS", "aifs-single", "oper"}, {"IFS HRES", "ifs", "oper"}};
    const auto id = ecmwfId(nhcId);
    const auto now = QDateTime::currentDateTimeUtc();
    // the newest of the last five cycles that has a track file (the run is posted about seven hours after its time)
    for (const auto& model : models) {
        string bytes;
        string cycle;
        for (int back = 0; back < 5 && bytes.empty(); back++) {
            const auto time = now.addSecs(-static_cast<qint64>(back) * 6 * 3600);
            const int hour = time.time().hour() / 6 * 6;
            char day[16];
            std::snprintf(day, sizeof day, "%04d%02d%02d", time.date().year(), time.date().month(), time.date().day());
            char hh[8];
            std::snprintf(hh, sizeof hh, "%02d", hour);
            for (const int steps : {360, 144}) {
                const string url = string{"https://data.ecmwf.int/forecasts/"} + day + "/" + hh + "z/" + model.model + "/0p25/" + model.stream + "/" + day + hh +
                    "0000-" + std::to_string(steps) + "h-" + model.stream + "-tf.bufr";
                bytes = cachedDownload(url);
                if (!bytes.empty()) {
                    cycle = string{day} + hh;
                    break;
                }
            }
        }
        if (bytes.empty()) {
            continue;
        }
        vector<UtilityEcmwfTracks::Storm> storms;
        string error;
        if (!UtilityEcmwfTracks::parse(bytes, storms, error)) {
            data.error = string{model.label} + ": " + error;
            continue;
        }
        for (auto& storm : storms) {
            if (storm.id == id) {
                data.sets.push_back({model.label, cycle, std::move(storm)});
                break;
            }
        }
    }
    if (data.sets.empty() && data.error.empty()) {
        data.error = "ECMWF has no ensemble tracks for this storm (yet).";
    }
}

bool HurricaneData::gefsFromGuidance(const string& stormId, const vector<UtilityAtcf::Track>& guidance, EnsembleSet& set) {
    set = EnsembleSet{};
    set.label = "GEFS";
    if (!UtilityEnsembleStats::fromGefs(guidance, set.storm)) {
        return false;
    }
    set.storm.id = ecmwfId(stormId);
    set.cycle = set.storm.cycle;
    return true;
}

void HurricaneData::loadShips(const string& nhcId, ShipsData& data) {
    data = ShipsData{};
    const auto file = UtilityShips::newestFile(download(atcf + "stext/"), nhcId);
    if (file.empty()) {
        data.error = "NHC has no SHIPS forecast for this storm (it is made for named storms and some invests).";
        return;
    }
    data.ships = UtilityShips::parse(download(atcf + "stext/" + file));
    if (!data.ships.ok) {
        data.error = "Could not read the SHIPS file " + file + ".";
    }
}

void HurricaneData::loadPod(PodData& data) {
    data = PodData{};
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    const auto folder = "https://www.nhc.noaa.gov/archive/recon/" + std::to_string(year) + "/REPRPD/";
    const auto listing = download(folder);
    const std::regex file{R"re(href="(REPRPD\.(\d{12})\.txt)")re"};
    string newest;
    string stamp;
    for (std::sregex_iterator it{listing.begin(), listing.end(), file}, end; it != end; ++it) {
        if (string{(*it)[2]} > stamp) {
            stamp = (*it)[2];
            newest = (*it)[1];
        }
    }
    if (newest.empty()) {
        data.error = "Could not find the Plan of the Day in the NHC recon archive.";
        return;
    }
    data.file = newest;
    data.issued = stamp;
    data.pod = UtilityPod::parse(download(folder + newest));
    if (!data.pod.ok) {
        data.error = "Could not read the Plan of the Day (" + newest + ").";
    }
}

void HurricaneData::loadVdm(const string& nhcId, VdmData& data) {
    data = VdmData{};
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    // the vortex data messages: REPNT2 Atlantic, REPPN2 Eastern Pacific, REPPA2 Central Pacific
    const string code = nhcId.rfind("ep", 0) == 0 ? "REPPN2" : nhcId.rfind("cp", 0) == 0 ? "REPPA2" : "REPNT2";
    const auto folder = "https://www.nhc.noaa.gov/archive/recon/" + std::to_string(year) + "/" + code + "/";
    const auto listing = download(folder);
    const std::regex file{"href=\"(" + code + R"re(-[A-Z]+\.(\d{12})\.txt)")re"};
    vector<std::pair<string, string>> files;   // time, name
    for (std::sregex_iterator it{listing.begin(), listing.end(), file}, end; it != end; ++it) {
        files.emplace_back((*it)[2], (*it)[1]);
    }
    if (files.empty()) {
        data.error = "Could not read the NHC reconnaissance archive.";
        return;
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    string wanted = nhcId;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    // each file is one small message; read the newest 80 (parsed ones are remembered, so a refresh costs little)
    static std::mutex mutex;
    static std::map<string, std::pair<bool, UtilityVdm::Vdm>> parsed;
    const size_t first = files.size() > 80 ? files.size() - 80 : 0;
    for (size_t i = first; i < files.size(); i++) {
        std::pair<bool, UtilityVdm::Vdm> entry;
        bool known = false;
        {
            std::lock_guard lock{mutex};
            const auto found = parsed.find(files[i].second);
            if (found != parsed.end()) {
                entry = found->second;
                known = true;
            }
        }
        if (!known) {
            entry.first = UtilityVdm::parse(download(folder + files[i].second), files[i].first, entry.second);
            std::lock_guard lock{mutex};
            parsed[files[i].second] = entry;
        }
        data.filesRead++;
        if (entry.first && !entry.second.test && entry.second.stormId == wanted) {
            data.messages.push_back(entry.second);
        }
    }
    std::sort(data.messages.begin(), data.messages.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds || (a.seconds == b.seconds && a.aircraft < b.aircraft); });
    // the same message can be filed twice (a retransmission): one per fix time and aircraft
    data.messages.erase(std::unique(data.messages.begin(), data.messages.end(), [] (const auto& a, const auto& b) { return a.seconds == b.seconds && a.aircraft == b.aircraft; }),
                        data.messages.end());
}

void HurricaneData::loadSeason(SeasonData& data, const string& basin) {
    data = SeasonData{};
    data.basin = basin;
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    data.currentYear = year;
    // the database: the newest file in NHC's directory; read once, then kept on disk as a compact list of storms
    const auto listing = download("https://www.nhc.noaa.gov/data/hurdat/");
    data.hurdatFile = UtilitySeason::newestHurdatFile(listing, basin == "al" ? "hurdat2-1851" : "hurdat2-nepac-1949");
    if (data.hurdatFile.empty()) {
        data.error = "Could not find the HURDAT2 file on the NHC site.";
    } else {
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/hurricane";
        QDir{}.mkpath(folder);
        const auto cachePath = folder + "/" + QString::fromStdString(data.hurdatFile) + ".csv";
        QFile cache{cachePath};
        if (cache.open(QIODevice::ReadOnly)) {
            data.history = UtilitySeason::fromCsv(cache.readAll().toStdString());
            cache.close();
        }
        if (data.history.empty()) {
            data.history = UtilitySeason::parseHurdat2(download("https://www.nhc.noaa.gov/data/hurdat/" + data.hurdatFile));
            if (data.history.empty()) {
                data.error = "Could not read the HURDAT2 file " + data.hurdatFile + ".";
            } else if (cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                cache.write(QByteArray::fromStdString(UtilitySeason::csv(data.history)));
                cache.close();
                // older cached versions are no longer needed
                for (const auto& old : QDir{folder}.entryList({"hurdat2-*.csv"}, QDir::Files)) {
                    if (old != QString::fromStdString(data.hurdatFile) + ".csv") {
                        QFile::remove(folder + "/" + old);
                    }
                }
            }
        }
    }
    int through = 0;
    for (const auto& s : data.history) {
        through = std::max(through, s.year);
    }
    // this season: the storms of the ATCF btk/ directory (numbers 90 and up are invests that have not become anything)
    if (year > through) {
        const auto btk = download(atcf + "btk/");
        // the Atlantic, or the Eastern and Central Pacific together (HURDAT2's northeast Pacific file holds both)
        const std::regex row{basin == "al" ? R"re(href="(b(al)(\d\d)(\d{4})\.dat)"[^\n]*?(\d{4}-\d\d-\d\d \d\d:\d\d))re"
                                           : R"re(href="(b(ep|cp)(\d\d)(\d{4})\.dat)"[^\n]*?(\d{4}-\d\d-\d\d \d\d:\d\d))re"};
        const auto now = QDateTime::currentDateTimeUtc();
        for (std::sregex_iterator it{btk.begin(), btk.end(), row}, end; it != end; ++it) {
            if (std::stoi((*it)[4]) != year || std::stoi((*it)[3]) >= 90) {
                continue;
            }
            const string id = string{(*it)[2]} + string{(*it)[3]} + string{(*it)[4]};
            auto storm = UtilitySeason::fromBestTrack(UtilityAtcf::bestTrack(download(atcf + "btk/" + string{(*it)[1]})), id);
            if (storm.first.empty()) {
                continue;
            }
            data.current.push_back(storm);
            const auto modified = QDateTime::fromString(QString::fromStdString((*it)[5]), "yyyy-MM-dd HH:mm");
            if (modified.isValid() && modified.secsTo(now) < 2 * 86400) {
                data.active.push_back(storm.id);
            }
        }
    }
}

void HurricaneData::loadGis(const StormEntry& entry, GisData& data) {
    data = GisData{};
    if (entry.coneZip.empty()) {
        data.error = "NHC publishes no cone for this storm.";
        return;
    }
    data.cone = UtilityNhcGis::parseCone(download(entry.coneZip));
    if (!data.cone.ok) {
        data.error = "Could not read NHC's cone file.";
    }
    if (!entry.radiiZip.empty()) {
        data.radii = UtilityNhcGis::parseRadii(download(entry.radiiZip));
    }
    if (!entry.watchKmz.empty()) {
        const auto kmz = download(entry.watchKmz);
        // no watches or warnings: the address answers with a page, not a file
        if (kmz.compare(0, 2, "PK") == 0 || kmz.compare(0, 5, "<?xml") == 0) {
            data.watchWarnings = UtilityNhcGis::parseWatchWarnings(kmz);
        }
    }
}

string HurricaneData::loadBulletin(const string& url) {
    return UtilityNhcText::bulletin(download(url));
}
