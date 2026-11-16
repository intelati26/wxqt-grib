// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/ServerStatus.h"
#include <mutex>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>

namespace {
    std::mutex lock;

    QJsonObject load() {
        QFile f{QString::fromStdString(ServerStatus::file())};
        return f.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(f.readAll()).object() : QJsonObject{};
    }
    QDateTime time(const QJsonObject& o, const char * key) {
        const auto t = QDateTime::fromString(o.value(key).toString(), Qt::ISODate);
        return t.isValid() ? t.toUTC() : QDateTime{};
    }
    QString text(const QDateTime& t) {
        return t.isValid() ? t.toUTC().toString(Qt::ISODate) : QString{};
    }
}

std::string ServerStatus::file() {
    return (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/servers/servers-status.json").toStdString();
}

std::string ServerStatus::name(State state) {
    switch (state) {
        case State::Ok: return "OK";
        case State::Late: return "Late";
        case State::Failing: return "Failing";
        case State::Down: return "Down";
        case State::Retired: return "Retired";
        default: return "";
    }
}

ServerStatus::State ServerStatus::stateOf(const Entry& e, const QDateTime& now) {
    if (!e.lastCheck.isValid()) {
        return State::Unknown;
    }
    if (!e.lastWasOk) {
        if (e.lastOk.isValid() && e.lastOk.secsTo(now) > 7 * 24 * 3600 && e.failsInRow >= 2) {
            return State::Retired;
        }
        if (e.failsInRow >= 2 && e.firstFail.isValid() && e.firstFail.secsTo(e.lastCheck) >= 600) {
            return State::Down;
        }
        return State::Failing;
    }
    if (e.expectRecent && e.newestRun.size() == 10) {
        auto run = QDateTime::fromString(QString::fromStdString(e.newestRun), "yyyyMMddHH");
        run.setTimeZone(QTimeZone::utc());
        if (run.isValid() && run.secsTo(now) > static_cast<qint64>(e.cycleHours + e.lagHours + 3) * 3600) {
            return State::Late;
        }
    }
    return State::Ok;
}

void ServerStatus::record(const std::string& model, bool ok, bool skipped, const std::string& run, const std::string& detail, int cycleHours, int lagHours, bool expectRecent, const QDateTime& now) {
    if (skipped || model.empty()) {
        return;
    }
    const std::lock_guard<std::mutex> guard{lock};
    auto root = load();
    const auto key = QString::fromStdString(model);
    auto o = root.value(key).toObject();
    o["last_check"] = text(now);
    o["detail"] = QString::fromStdString(detail).left(300);
    o["last_was_ok"] = ok;
    o["cycle_hours"] = cycleHours;
    o["lag_hours"] = lagHours;
    o["expect_recent"] = expectRecent;
    if (ok) {
        o["last_ok"] = text(now);
        o["fails_in_row"] = 0;
        o.remove("first_fail");
        if (!run.empty()) {
            o["newest_run"] = QString::fromStdString(run);
        }
    } else {
        const int fails = o.value("fails_in_row").toInt() + 1;
        o["fails_in_row"] = fails;
        if (fails == 1 || !o.contains("first_fail")) {
            o["first_fail"] = text(now);
        }
    }
    root[key] = o;
    QDir{}.mkpath(QString::fromStdString(file()).section('/', 0, -2));
    QSaveFile out{QString::fromStdString(file())};
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument{root}.toJson(QJsonDocument::Compact));
        out.commit();
    }
}

ServerStatus::Entry ServerStatus::entry(const std::string& model) {
    const std::lock_guard<std::mutex> guard{lock};
    const auto o = load().value(QString::fromStdString(model)).toObject();
    Entry e;
    e.model = model;
    if (o.isEmpty()) {
        return e;
    }
    e.newestRun = o.value("newest_run").toString().toStdString();
    e.detail = o.value("detail").toString().toStdString();
    e.lastCheck = time(o, "last_check");
    e.lastOk = time(o, "last_ok");
    e.firstFail = time(o, "first_fail");
    e.failsInRow = o.value("fails_in_row").toInt();
    e.lastWasOk = o.value("last_was_ok").toBool();
    e.cycleHours = o.value("cycle_hours").toInt(6);
    e.lagHours = o.value("lag_hours").toInt(0);
    e.expectRecent = o.value("expect_recent").toBool(true);
    return e;
}

std::string ServerStatus::ago(const QDateTime& when, const QDateTime& now) {
    if (!when.isValid()) {
        return "never";
    }
    const auto seconds = when.secsTo(now);
    if (seconds < 120) {
        return "just now";
    }
    if (seconds < 7200) {
        return std::to_string(seconds / 60) + " min ago";
    }
    if (seconds < 2 * 86400) {
        return std::to_string(seconds / 3600) + " h ago";
    }
    return std::to_string(seconds / 86400) + " days ago";
}
