// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Servers.h"
#include <algorithm>
#include <mutex>
#include <thread>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include "objects/URL.h"
#include "util/Utility.h"

namespace {
    const char * const projectAddress = "https://raw.githubusercontent.com/intelati26/wxqt-grib/main/servers/servers.json";
    constexpr int maxBytes = 64 * 1024;

    std::mutex lock;
    std::map<std::string, std::string> promoted;   // name -> the base that answered when the first did not (this run of the program), put first in what table() gives
    std::map<std::string, std::string> demoted;    // name -> the base it was put before
    std::map<std::string, QDateTime> promotedAt;   // name -> when (kept in promoted.json: a fallback is used for a day across restarts, then the first place is tried again)
    bool promotedLoaded{false};

    QString promotedPath() {
        return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/servers/promoted.json";
    }
    void loadPromoted() {   // (the lock is held)
        promotedLoaded = true;
        QFile f{promotedPath()};
        if (!f.open(QIODevice::ReadOnly)) {
            return;
        }
        const auto root = QJsonDocument::fromJson(f.readAll()).object();
        const auto now = QDateTime::currentDateTimeUtc();
        for (auto it = root.begin(); it != root.end(); ++it) {
            const auto o = it.value().toObject();
            const auto at = QDateTime::fromString(o.value("at").toString(), Qt::ISODate);
            if (at.isValid() && at.secsTo(now) < 24 * 3600 && at <= now.addSecs(60)) {   // older than a day: the first place gets its chance again
                promoted[it.key().toStdString()] = o.value("base").toString().toStdString();
                demoted[it.key().toStdString()] = o.value("first").toString().toStdString();
                promotedAt[it.key().toStdString()] = at;
            }
        }
    }
    void savePromoted() {   // (the lock is held)
        QJsonObject root;
        for (const auto& [name, base] : promoted) {
            QJsonObject o;
            o["base"] = QString::fromStdString(base);
            o["first"] = QString::fromStdString(demoted[name]);
            o["at"] = promotedAt[name].toString(Qt::ISODate);
            root[QString::fromStdString(name)] = o;
        }
        QDir{}.mkpath(promotedPath().section('/', 0, -2));
        QSaveFile out{promotedPath()};
        if (out.open(QIODevice::WriteOnly)) {
            out.write(QJsonDocument{root}.toJson(QJsonDocument::Compact));
            out.commit();
        }
    }
    bool loaded{false};
    Servers::Table cached;
    Servers::FetchStatus status;

    bool plainName(const std::string& name) {
        return !name.empty() && name.size() <= 40 && std::all_of(name.begin(), name.end(), [] (unsigned char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'; });
    }

    std::string hostOf(const std::string& url) {
        const auto from = url.find("://");
        if (from == std::string::npos) {
            return {};
        }
        const auto end = url.find_first_of("/?#:", from + 3);
        return url.substr(from + 3, end == std::string::npos ? std::string::npos : end - from - 3);
    }

    QByteArray readFile(const std::string& path) {
        QFile file{QString::fromStdString(path)};
        return file.open(QIODevice::ReadOnly) ? file.read(maxBytes + 1) : QByteArray{};
    }

    void loadLayers() {
        Servers::Table table;
        std::string why;
        const bool any = Utility::readPref("SERVERS_ANY_HOST", "off") == "on";
        Servers::parse(QByteArray::fromStdString(Servers::builtInJson()), "built in", true, table, why);   // (the program's own: any host it names)
        if (const auto bytes = readFile(Servers::projectFile()); !bytes.isEmpty()) {
            Servers::parse(bytes, "project", any, table, why);   // (a file that is not valid is left out whole: the layer below stands)
        }
        if (const auto bytes = readFile(Servers::userFile()); !bytes.isEmpty()) {
            Servers::parse(bytes, "yours", true, table, why);   // (the person's own file: their choice of hosts)
        }
        for (auto& [name, entry] : table) {   // Settings: one base for a name
            const auto set = Utility::readPref("SERVER_" + name, "");
            if (!set.empty() && set.rfind("https://", 0) == 0 && set.find_first_of("\"'\\ <>{}") == std::string::npos) {
                entry.bases.insert(entry.bases.begin(), set.back() == '/' ? set.substr(0, set.size() - 1) : set);
                entry.origin = "settings";
            }
        }
        cached = std::move(table);
        loaded = true;
    }
}

bool Servers::allowedHost(const std::string& host) {
    static const char * const suffixes[] = {"amazonaws.com", "noaa.gov", "googleapis.com", "dwd.de", "weather.gc.ca", "open-meteo.com", "nasa.gov", "recastsystems.com", "ecmwf.int", "ucar.edu", "meteo.fr"};
    for (const auto * suffix : suffixes) {
        const std::string s{suffix};
        if (host == s || (host.size() > s.size() && host.compare(host.size() - s.size() - 1, s.size() + 1, "." + s) == 0)) {
            return true;
        }
    }
    return false;
}

bool Servers::parse(const QByteArray& json, const std::string& origin, bool anyHost, Table& out, std::string& why) {
    if (json.isEmpty() || json.size() > maxBytes) {
        why = json.isEmpty() ? "the file is empty" : "the file is over 64 KB";
        return false;
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json, &error);
    if (document.isNull() || !document.isObject()) {
        why = "not JSON: " + error.errorString().toStdString();
        return false;
    }
    const auto root = document.object();
    if (root.value("version").toInt() != 1) {
        why = "version is not 1";
        return false;
    }
    const auto servers = root.value("servers").toObject();
    if (servers.isEmpty()) {
        why = "no servers";
        return false;
    }
    Table made;
    for (auto it = servers.begin(); it != servers.end(); ++it) {
        const auto name = it.key().toStdString();
        if (!plainName(name)) {
            why = "the name \"" + name + "\" is not a plain name (a to z, 0 to 9 and -)";
            return false;
        }
        const auto entry = it.value().toObject();
        const auto bases = entry.value("bases").toArray();
        if (bases.isEmpty() || bases.size() > 4) {
            why = name + ": one to four bases";
            return false;
        }
        Entry e;
        e.name = name;
        e.origin = origin;
        e.note = entry.value("note").toString().left(200).toStdString();
        for (const auto& value : bases) {
            auto base = value.toString().toStdString();
            while (!base.empty() && base.back() == '/') {
                base.pop_back();
            }
            if (base.rfind("https://", 0) != 0) {
                why = name + ": a base begins with https://";
                return false;
            }
            if (base.find_first_of("\"'\\ <>{}`?#@") != std::string::npos || base.find("..") != std::string::npos || base.find("/vsi") != std::string::npos) {
                why = name + ": a base has a character that is not allowed";
                return false;
            }
            const auto host = hostOf(base);
            if (host.empty() || host.find('.') == std::string::npos) {
                why = name + ": a base needs a host name";
                return false;
            }
            if (!anyHost && !allowedHost(host)) {
                why = name + ": the host " + host + " is not one the program reads from (Settings can allow other hosts)";
                return false;
            }
            e.bases.push_back(base);
        }
        made[name] = std::move(e);
    }
    for (auto& [name, entry] : made) {   // (only now: a file with one bad entry changes nothing)
        out[name] = std::move(entry);
    }
    return true;
}

Servers::Table Servers::table() {
    const std::lock_guard<std::mutex> guard{lock};
    if (!loaded) {
        loadLayers();
    }
    if (!promotedLoaded) {
        loadPromoted();
    }
    auto out = cached;
    for (const auto& [name, base] : promoted) {   // a fallback that answered while the first did not stays first until the program restarts (or the table is read again: reload)
        const auto found = out.find(name);
        if (found == out.end()) {
            continue;
        }
        auto& bases = found->second.bases;
        const auto at = std::find(bases.begin(), bases.end(), base);
        if (at != bases.end() && at != bases.begin()) {
            std::rotate(bases.begin(), at, at + 1);
        }
    }
    return out;
}

bool Servers::promote(const std::string& url) {
    const auto name = nameOfUrl(url);
    if (name.empty()) {
        return false;
    }
    const auto all = table();
    const auto& bases = all.at(name).bases;
    for (size_t i = 1; i < bases.size(); i++) {
        if (url.rfind(bases[i], 0) == 0) {
            const std::lock_guard<std::mutex> guard{lock};
            if (demoted.find(name) == demoted.end()) {
                demoted[name] = bases.front();
            }
            promoted[name] = bases[i];
            promotedAt[name] = QDateTime::currentDateTimeUtc();
            savePromoted();
            return true;
        }
    }
    return false;
}

std::string Servers::demotedBase(const std::string& name) {
    const std::lock_guard<std::mutex> guard{lock};
    if (!promotedLoaded) {
        loadPromoted();
    }
    const auto found = demoted.find(name);
    return found == demoted.end() ? std::string{} : found->second;
}

void Servers::reload() {
    const std::lock_guard<std::mutex> guard{lock};
    promoted.clear();   // (a table read again starts from what the files say, and what promoted.json keeps)
    demoted.clear();
    promotedAt.clear();
    promotedLoaded = false;
    loadLayers();
}

std::string Servers::nameIn(const std::string& pattern) {
    if (pattern.rfind("{@", 0) != 0) {
        return {};
    }
    const auto end = pattern.find('}');
    if (end == std::string::npos) {
        return {};
    }
    const auto name = pattern.substr(2, end - 2);
    return plainName(name) ? name : std::string{};
}

std::vector<std::string> Servers::places(const std::string& pattern) {
    const auto name = nameIn(pattern);
    if (name.empty()) {
        return {pattern};
    }
    const auto all = table();
    const auto found = all.find(name);
    if (found == all.end()) {
        return {};
    }
    std::vector<std::string> out;
    const auto rest = pattern.substr(name.size() + 3);
    for (const auto& base : found->second.bases) {
        out.push_back(base + rest);
    }
    return out;
}

std::string Servers::projectFile() {
    return (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/servers/project.json").toStdString();
}

std::string Servers::userFile() {
    return (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models/servers.json").toStdString();
}

Servers::FetchStatus Servers::lastFetch() {
    FetchStatus out;
    out.when = Utility::readPref("SERVERS_LAST_FETCH", "");
    out.result = Utility::readPref("SERVERS_LAST_RESULT", "");
    out.ok = Utility::readPref("SERVERS_LAST_OK", "") == "1";
    return out;
}

void Servers::refreshFromProject(const std::function<void(bool, const std::string&)>& done, const std::string& url) {
    const auto record = [&done] (bool ok, const std::string& text) {
        Utility::writePref("SERVERS_LAST_FETCH", QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString());
        Utility::writePref("SERVERS_LAST_RESULT", text);
        Utility::writePref("SERVERS_LAST_OK", ok ? "1" : "0");
        if (done) {
            done(ok, text);
        }
    };
    int code = 0;
    const auto bytes = URL::getBytesWithStatus(url.empty() ? std::string{projectAddress} : url, code);
    if (code != 200 || bytes.isEmpty()) {
        record(false, code == 0 ? "no answer" : "HTTP " + std::to_string(code));   // (the file kept from before, and the built-in table, stand)
        return;
    }
    Table check;
    std::string why;
    if (!parse(bytes, "project", Utility::readPref("SERVERS_ANY_HOST", "off") == "on", check, why)) {
        record(false, "the file was not used: " + why);
        return;
    }
    QDir{}.mkpath(QString::fromStdString(projectFile()).section('/', 0, -2));
    QSaveFile file{QString::fromStdString(projectFile())};
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        record(false, "the file could not be kept");
        return;
    }
    reload();
    record(true, std::to_string(check.size()) + " servers");
}

void Servers::refreshIfDue() {
    if (Utility::readPref("SERVERS_FETCH", "on") != "on") {
        return;
    }
    const auto last = QDateTime::fromString(QString::fromStdString(Utility::readPref("SERVERS_LAST_FETCH", "")), Qt::ISODate);
    if (last.isValid() && last.secsTo(QDateTime::currentDateTimeUtc()) < 20 * 3600) {
        return;
    }
    std::thread{[] { refreshFromProject(nullptr); }}.detach();
}

std::string Servers::nameOfUrl(const std::string& url) {
    for (const auto& [name, entry] : table()) {
        for (const auto& base : entry.bases) {
            if (url.rfind(base, 0) == 0) {
                return name;
            }
        }
    }
    return {};
}
