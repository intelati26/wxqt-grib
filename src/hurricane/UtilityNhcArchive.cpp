// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityNhcArchive.h"
#include <algorithm>
#include <set>
#include <cmath>
#include <atomic>
#include <mutex>
#include <thread>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimeZone>
#include "objects/NetManager.h"
#include "objects/URL.h"
#include "util/UtilityGzip.h"

namespace {
    std::mutex progressMutex;
    string stageText;
    std::atomic<int> zipsRead{0};
    std::atomic<int> zipsAsked{0};

    const string base = "https://www.nhc.noaa.gov/gis/forecast/archive/";

    QString folder() {
        const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/archive/nhc";
        QDir{}.mkpath(path);
        return path;
    }

    bool isZip(const QByteArray& bytes) { return bytes.size() > 100 && bytes.startsWith("PK"); }

    // one advisory's zip: from the folder, else downloaded and kept; empty when NHC has none
    QByteArray advisoryZip(const string& id, const string& number) {
        const QString path = folder() + "/" + QString::fromStdString(id + "_5day_" + number) + ".zip";
        QFile kept{path};
        if (kept.open(QIODevice::ReadOnly)) {
            zipsRead++;
            return kept.readAll();
        }
        zipsAsked++;
        int status = 0;
        const auto bytes = URL::getBytesWithStatus(base + id + "_5day_" + number + ".zip", status);
        if (status == 200 && isZip(bytes)) {
            zipsRead++;
            QFile out{path};
            if (out.open(QIODevice::WriteOnly)) {
                out.write(bytes);
            }
            return bytes;
        }
        return {};
    }

    // the first of the names that exists, for each of the candidates, asked for on eight threads (a miss costs a round trip, and there are many)
    vector<QByteArray> zipsFor(const vector<std::pair<string, vector<string>>>& wanted) {
        vector<QByteArray> out(wanted.size());
        std::atomic<size_t> next{0};
        const auto work = [&] {
            const NetManager::Scope background{NetManager::Priority::Ahead};   // behind what is on screen, through the program's one network client
            for (;;) {
                const size_t i = next++;
                if (i >= wanted.size()) {
                    return;
                }
                for (const auto& number : wanted[i].second) {
                    out[i] = advisoryZip(wanted[i].first, number);
                    if (!out[i].isEmpty()) {
                        break;
                    }
                }
            }
        };
        vector<std::thread> pool;
        for (size_t k = 0; k < std::min<size_t>(8, wanted.size()); k++) {
            pool.emplace_back(work);
        }
        for (auto& thread : pool) {
            thread.join();
        }
        return out;
    }

    string pad(int n) {
        char text[8];
        std::snprintf(text, sizeof(text), "%03d", n);
        return text;
    }

    // the best track as ATCF text: btk/ for the storms of the recent past, the year's archive for the older ones
    string bestText(const string& id, int year) {
        const string atcf = "https://ftp.nhc.noaa.gov/atcf/";
        const QString path = folder() + "/b" + QString::fromStdString(id) + ".dat";
        const bool old = year < QDate::currentDate().year();
        QFile kept{path};
        if (old && kept.open(QIODevice::ReadOnly)) {
            return kept.readAll().toStdString();
        }
        string text;
        if (!old) {
            const auto bytes = URL::getBytes(atcf + "btk/b" + id + ".dat");
            text = bytes.toStdString();
        }
        if (text.size() < 100 || text.find(',') == string::npos) {
            const auto zipped = URL::getBytes(atcf + "archive/" + std::to_string(year) + "/b" + id + ".dat.gz").toStdString();
            text.clear();
            if (!zipped.empty() && !UtilityGzip::gunzip(zipped, text)) {
                text.clear();
            }
        }
        if (text.size() < 100 || text.find(',') == string::npos) {
            return {};
        }
        if (old) {   // a finished season's file does not change
            QFile out{path};
            if (out.open(QIODevice::WriteOnly)) {
                out.write(QByteArray::fromStdString(text));
            }
        }
        return text;
    }

    double zoneHours(const QString& zone) {
        static const QMap<QString, double> hours{{"AST", -4}, {"ADT", -3}, {"EST", -5}, {"EDT", -4}, {"CST", -6}, {"CDT", -5}, {"MST", -7}, {"MDT", -6}, {"PST", -8}, {"PDT", -7},
                                                 {"AKST", -9}, {"AKDT", -8}, {"HST", -10}, {"HDT", -9}, {"CVT", -1}, {"UTC", 0}, {"GMT", 0}, {"Z", 0}};
        return hours.value(zone, 99.0);
    }
}

QDateTime UtilityNhcArchive::parseIssued(const string& text) {
    static const QRegularExpression re{R"((\d{1,4})\s*(AM|PM)\s+([A-Z]{1,4})\s+\w{3}\s+(\w{3})\s+(\d{1,2})\s+(\d{4}))"};
    const auto match = re.match(QString::fromStdString(text).toUpper());
    if (!match.hasMatch()) {
        return {};
    }
    const int clock = match.captured(1).toInt();
    int hour = clock >= 100 ? clock / 100 : clock;
    const int minute = clock >= 100 ? clock % 100 : 0;
    hour = hour % 12 + (match.captured(2) == "PM" ? 12 : 0);
    static const QStringList months{"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    const int month = static_cast<int>(months.indexOf(match.captured(4))) + 1;
    const double offset = zoneHours(match.captured(3));
    if (month < 1 || offset > 50.0) {
        return {};
    }
    const QDateTime local{QDate{match.captured(6).toInt(), month, match.captured(5).toInt()}, QTime{hour, minute}, QTimeZone::utc()};
    return local.addSecs(static_cast<qint64>(-offset * 3600.0));
}

vector<UtilityNhcArchive::StormRef> UtilityNhcArchive::stormsOfYear(int year) {
    const QString path = folder() + "/year" + QString::number(year) + ".txt";
    const QFileInfo info{path};
    const bool current = year >= QDate::currentDate().year();
    vector<StormRef> found;
    if (info.exists() && (!current || info.lastModified().secsTo(QDateTime::currentDateTime()) < 3 * 3600)) {
        QFile file{path};
        if (file.open(QIODevice::ReadOnly)) {
            for (const auto& line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
                const auto parts = line.split('|');
                if (parts.size() == 3) {
                    found.push_back({parts[0].toStdString(), parts[1].toStdString(), QDateTime::fromString(parts[2], Qt::ISODate)});
                }
            }
            return found;
        }
    }
    for (const char * basin : {"al", "ep", "cp"}) {
        setStage("listing the " + std::to_string(year) + " storms (" + basin + ")");
        int misses = 0;
        for (int first = 1; first <= 40 && misses < 3; first += 8) {
            vector<std::pair<string, vector<string>>> wanted;
            for (int number = first; number < first + 8 && number <= 40; number++) {
                wanted.push_back({string{basin} + (number < 10 ? "0" : "") + std::to_string(number) + std::to_string(year), {"001", "001A", "001a", "002"}});
            }
            const auto zips = zipsFor(wanted);
            for (size_t k = 0; k < zips.size() && misses < 3; k++) {
                if (zips[k].isEmpty()) {
                    misses++;
                    continue;
                }
                misses = 0;
                const auto cone = UtilityNhcGis::parseCone(zips[k].toStdString());
                found.push_back({wanted[k].first, cone.stormName, parseIssued(cone.advisoryDate)});
            }
        }
    }
    QFile out{path};
    if (out.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream{&out};
        for (const auto& s : found) {
            stream << QString::fromStdString(s.id) << '|' << QString::fromStdString(s.name) << '|' << s.first.toString(Qt::ISODate) << '\n';
        }
    }
    return found;
}

UtilityNhcArchive::Storm UtilityNhcArchive::load(const string& id, bool withBest) {
    Storm storm;
    storm.id = id;
    setStage("reading the advisories of " + id);
    const int year = id.size() >= 8 ? std::stoi(id.substr(4, 4)) : 0;
    // the advisories found before (the manifest) are read from the folder; the next numbers are asked for, unless the season is over
    const QString manifestPath = folder() + "/" + QString::fromStdString(id) + ".idx";
    QStringList known;
    {
        QFile file{manifestPath};
        if (file.open(QIODevice::ReadOnly)) {
            known = QString::fromUtf8(file.readAll()).split(',', Qt::SkipEmptyParts);
        }
    }
    const auto take = [&] (const string& number) {
        const auto zip = advisoryZip(id, number);
        if (zip.isEmpty()) {
            return false;
        }
        Advisory advisory;
        advisory.number = number;
        advisory.cone = UtilityNhcGis::parseCone(zip.toStdString());
        if (!advisory.cone.ok) {
            return true;   // it is there, but unreadable
        }
        advisory.issued = parseIssued(advisory.cone.advisoryDate);
        if (storm.name.empty()) {
            storm.name = advisory.cone.stormName;
        }
        storm.advisories.push_back(std::move(advisory));
        return true;
    };
    int highest = 0;
    QStringList found;
    for (const auto& number : known) {
        if (take(number.toStdString())) {
            found << number;
            highest = std::max(highest, number.left(3).toInt());
        }
    }
    bool settled = !known.isEmpty() && year < QDate::currentDate().year();
    if (!settled && !storm.advisories.empty()) {   // a storm whose last advisory is days old is over
        QDateTime newest;
        for (const auto& a : storm.advisories) {
            const auto t = parseIssued(a.cone.advisoryDate);
            if (t.isValid() && (!newest.isValid() || t > newest)) {
                newest = t;
            }
        }
        settled = newest.isValid() && newest.secsTo(QDateTime::currentDateTimeUtc()) > 5 * 86400;
    }
    if (!settled) {
        char letters = 0;   // the case of the letter of an intermediate advisory differs between the years: once seen, only that one is asked for
        for (const auto& n : known) {
            letters = n.endsWith('A') || n.endsWith('B') ? 'A' : n.endsWith('a') || n.endsWith('b') ? 'a' : letters;
        }
        int emptyNumbers = 0;
        for (int first = std::max(1, highest); first <= 400 && emptyNumbers < 3; first += 8) {
            vector<std::pair<string, vector<string>>> wanted;
            vector<string> names;
            for (int number = first; number < first + 8; number++) {
                for (const string suffix : {"", "A", "a", "B", "b"}) {
                    const bool lower = suffix == "a" || suffix == "b";
                    const bool upper = suffix == "A" || suffix == "B";
                    if ((lower && letters == 'A') || (upper && letters == 'a')) {
                        continue;
                    }
                    const string name = pad(number) + suffix;
                    if (!known.contains(QString::fromStdString(name))) {
                        wanted.push_back({id, {name}});
                        names.push_back(name);
                    }
                }
            }
            const auto zips = zipsFor(wanted);
            std::set<int> hitNumbers;
            for (size_t k = 0; k < zips.size(); k++) {
                if (zips[k].isEmpty()) {
                    continue;
                }
                const auto& name = names[k];
                Advisory advisory;
                advisory.number = name;
                advisory.cone = UtilityNhcGis::parseCone(zips[k].toStdString());
                found << QString::fromStdString(name);
                hitNumbers.insert(std::stoi(name.substr(0, 3)));
                if (name.size() > 3) {
                    letters = name[3] >= 'a' ? 'a' : 'A';
                }
                if (advisory.cone.ok) {
                    advisory.issued = parseIssued(advisory.cone.advisoryDate);
                    if (storm.name.empty()) {
                        storm.name = advisory.cone.stormName;
                    }
                    storm.advisories.push_back(std::move(advisory));
                }
            }
            for (int number = first; number < first + 8; number++) {
                emptyNumbers = hitNumbers.count(number) > 0 || known.contains(QString::fromStdString(pad(number))) ? 0 : emptyNumbers + 1;
                if (emptyNumbers >= 3) {
                    break;
                }
            }
        }
        QFile out{manifestPath};
        if (!found.isEmpty() && out.open(QIODevice::WriteOnly)) {
            out.write(found.join(',').toUtf8());
        }
    }
    // an advisory whose time could not be read takes the one before it plus six hours
    for (size_t i = 0; i < storm.advisories.size(); i++) {
        if (!storm.advisories[i].issued.isValid()) {
            storm.advisories[i].issued = i > 0 ? storm.advisories[i - 1].issued.addSecs(6 * 3600) : QDateTime{};
        }
    }
    storm.advisories.erase(std::remove_if(storm.advisories.begin(), storm.advisories.end(), [] (const Advisory& a) { return !a.issued.isValid(); }), storm.advisories.end());
    std::stable_sort(storm.advisories.begin(), storm.advisories.end(), [] (const Advisory& a, const Advisory& b) { return a.issued < b.issued; });
    if (withBest && year > 0) {
        storm.best = UtilityAtcf::bestTrack(bestText(id, year));
    }
    return storm;
}

bool UtilityNhcArchive::observedAt(const vector<UtilityAtcf::Fix>& best, const QDateTime& utc, double& lat, double& lon, int& wind) {
    if (best.size() < 2) {
        return false;
    }
    for (size_t i = 0; i + 1 < best.size(); i++) {
        auto a = QDateTime::fromString(QString::fromStdString(best[i].time), "yyyyMMddHH");
        auto b = QDateTime::fromString(QString::fromStdString(best[i + 1].time), "yyyyMMddHH");
        a.setTimeZone(QTimeZone::utc());
        b.setTimeZone(QTimeZone::utc());
        if (utc >= a && utc <= b) {
            const double span = static_cast<double>(a.secsTo(b));
            const double f = span > 0.0 ? static_cast<double>(a.secsTo(utc)) / span : 0.0;
            lat = best[i].lat + (best[i + 1].lat - best[i].lat) * f;
            double lonB = best[i + 1].lon;
            if (std::abs(lonB - best[i].lon) > 180.0) {
                lonB += lonB < best[i].lon ? 360.0 : -360.0;
            }
            lon = best[i].lon + (lonB - best[i].lon) * f;
            wind = f < 0.5 ? best[i].wind : best[i + 1].wind;
            return true;
        }
    }
    return false;
}

double UtilityNhcArchive::distanceNm(double lat1, double lon1, double lat2, double lon2) {
    const double r = M_PI / 180.0;
    const double a = std::sin((lat2 - lat1) * r / 2.0), b = std::sin((lon2 - lon1) * r / 2.0);
    const double h = a * a + std::cos(lat1 * r) * std::cos(lat2 * r) * b * b;
    return 2.0 * 3440.065 * std::asin(std::min(1.0, std::sqrt(h)));
}

string UtilityNhcArchive::progressText() {
    std::lock_guard<std::mutex> lock{progressMutex};
    return stageText + " (" + std::to_string(zipsRead.load()) + " files read" + (zipsAsked.load() > 0 ? ", " + std::to_string(zipsAsked.load()) + " downloaded" : string{}) + ")";
}

void UtilityNhcArchive::setStage(const string& stage) {
    std::lock_guard<std::mutex> lock{progressMutex};
    if (stage != stageText && stage.find("storm ") == 0) {
        // a new storm of the same piece of work keeps the counts
    }
    stageText = stage;
}

vector<UtilityNhcArchive::Outlook> UtilityNhcArchive::outlooks(const QDateTime& from, const QDateTime& to) {
    // the directory page, as the list of issuance times (kept six hours)
    const QString listPath = folder() + "/gtwo-index.txt";
    QStringList names;
    {
        const QFileInfo info{listPath};
        if (info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < 6 * 3600) {
            QFile file{listPath};
            if (file.open(QIODevice::ReadOnly)) {
                names = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
            }
        }
        if (names.isEmpty()) {
            setStage("reading the outlook archive's list");
            const auto page = URL::getBytes("https://www.nhc.noaa.gov/gis/gtwo/archive/");
            static const QRegularExpression re{R"(href="(\d{12})_gtwo\.zip")"};
            auto it = re.globalMatch(QString::fromUtf8(page));
            while (it.hasNext()) {
                names << it.next().captured(1);
            }
            names.removeDuplicates();
            if (!names.isEmpty()) {
                QFile out{listPath};
                if (out.open(QIODevice::WriteOnly)) {
                    out.write(names.join('\n').toUtf8());
                }
            } else {
                QFile file{listPath};   // an old list is better than none
                if (file.open(QIODevice::ReadOnly)) {
                    names = QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts);
                }
            }
        }
    }
    vector<std::pair<QDateTime, QString>> wanted;
    for (const auto& name : names) {
        auto time = QDateTime::fromString(name, "yyyyMMddHHmm");
        time.setTimeZone(QTimeZone::utc());
        if (time.isValid() && time >= from && time <= to) {
            wanted.push_back({time, name});
        }
    }
    std::sort(wanted.begin(), wanted.end());
    QDir{}.mkpath(folder() + "/gtwo");
    vector<Outlook> out(wanted.size());
    std::atomic<size_t> next{0}, done{0};
    const auto work = [&] {
        const NetManager::Scope background{NetManager::Priority::Ahead};
        for (;;) {
            const size_t i = next++;
            if (i >= wanted.size()) {
                return;
            }
            const QString path = folder() + "/gtwo/" + wanted[i].second + ".zip";
            QByteArray bytes;
            QFile kept{path};
            if (kept.open(QIODevice::ReadOnly)) {
                bytes = kept.readAll();
            } else {
                int status = 0;
                bytes = URL::getBytesWithStatus("https://www.nhc.noaa.gov/gis/gtwo/archive/" + wanted[i].second.toStdString() + "_gtwo.zip", status);
                if (status == 200 && isZip(bytes)) {
                    QFile file{path};
                    if (file.open(QIODevice::WriteOnly)) {
                        file.write(bytes);
                    }
                } else {
                    bytes.clear();
                }
                zipsAsked++;
            }
            if (!bytes.isEmpty()) {
                out[i].issued = wanted[i].first;
                out[i].areas = UtilityNhcGis::parseOutlook(bytes.toStdString());
            }
            zipsRead++;
            done++;
            setStage("reading the outlooks (" + std::to_string(done.load()) + " of " + std::to_string(wanted.size()) + ")");
        }
    };
    setStage("reading the outlooks (0 of " + std::to_string(wanted.size()) + ")");
    vector<std::thread> pool;
    for (size_t k = 0; k < std::min<size_t>(6, wanted.size()); k++) {
        pool.emplace_back(work);
    }
    for (auto& thread : pool) {
        thread.join();
    }
    out.erase(std::remove_if(out.begin(), out.end(), [] (const Outlook& o) { return !o.issued.isValid(); }), out.end());
    return out;
}
