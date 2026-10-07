// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HurricaneData.h"
#include <algorithm>
#include <ctime>
#include <mutex>
#include <regex>
#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

bool HurricaneData::loadStormList(vector<StormEntry>& entries, string& error) {
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
        if (entry.id.rfind("al", 0) != 0) {
            continue;   // the Atlantic only
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
        entry.graphicsUrl = text(o.value("forecastGraphics").toObject(), "url");
        entry.label = entry.name + " (" + entry.classification + ", " + (entry.wind >= 0 ? std::to_string(entry.wind) + " kt" : string{"-"}) + ") - " + idLabel(entry.id);
        activeIds.push_back(entry.id);
        entries.push_back(entry);
    }
    // the rest of the season from the btk/ directory: invests (numbers 90-99) touched in the last two days, and the finished storms
    const auto listing = download(atcf + "btk/");
    const std::regex row{R"re(href="(bal(\d\d)(\d{4})\.dat)"[^\n]*?(\d{4}-\d\d-\d\d \d\d:\d\d))re"};
    const auto now = QDateTime::currentDateTimeUtc();
    vector<StormEntry> rest;
    for (std::sregex_iterator it{listing.begin(), listing.end(), row}, end; it != end; ++it) {
        const string number = (*it)[2];
        const string fileYear = (*it)[3];
        if (std::stoi(fileYear) != year) {
            continue;
        }
        StormEntry entry;
        entry.id = "al" + number + fileYear;
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
            entry.label = "Invest " + number + "L (" + idLabel(entry.id) + ")";
        } else {
            entry.label = "AL" + number + " " + fileYear + (recent ? " (recent)" : " (finished)");
        }
        rest.push_back(entry);
    }
    std::stable_sort(rest.begin(), rest.end(), [] (const auto& a, const auto& b) {
        return a.active != b.active ? a.active : a.id > b.id;   // invests first, then the newest storm
    });
    entries.insert(entries.end(), rest.begin(), rest.end());
    if (entries.empty()) {
        error = "No Atlantic storms in the NHC lists.";
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
    if (data.best.empty() && data.guidance.empty() && data.error.empty()) {
        data.error = "NHC has no track data for this storm yet.";
    }
}

void HurricaneData::loadRecon(ReconData& data, int bulletins) {
    data = ReconData{};
    const auto year = QDateTime::currentDateTimeUtc().date().year();
    const auto folder = "https://www.nhc.noaa.gov/archive/recon/" + std::to_string(year) + "/AHONT1/";
    const auto listing = download(folder);
    const std::regex file{R"re(href="(AHONT1-[A-Z]+\.(\d{12})\.txt)")re"};
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
