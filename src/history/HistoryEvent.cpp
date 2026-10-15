// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/HistoryEvent.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

std::string HistoryEvent::defaultName() const {
    const QString when = start.isValid() ? start.toUTC().toString("yyyy-MM-dd HH:mm") + "Z" : QString{"no date"};
    return (when + QString{"  %1%2 %3%4"}.arg(std::abs(lat), 0, 'f', 2).arg(lat >= 0.0 ? "N" : "S").arg(std::abs(lon), 0, 'f', 2).arg(lon >= 0.0 ? "E" : "W")).toStdString();
}

namespace {
    QString file() {
        const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/history";
        QDir{}.mkpath(folder);
        return folder + "/events.json";
    }
}

std::vector<HistoryEvent> HistoryEvents::load() {
    std::vector<HistoryEvent> out;
    QFile f{file()};
    if (!f.open(QIODevice::ReadOnly)) {
        return out;
    }
    const auto list = QJsonDocument::fromJson(f.readAll()).array();
    for (const auto& item : list) {
        const auto o = item.toObject();
        HistoryEvent e;
        e.id = o.value("id").toString().toStdString();
        e.name = o.value("name").toString().toStdString();
        e.source = o.value("source").toString("user").toStdString();
        e.catalogId = o.value("catalogId").toString().toStdString();
        e.start = QDateTime::fromString(o.value("start").toString(), Qt::ISODate);
        e.end = QDateTime::fromString(o.value("end").toString(), Qt::ISODate);
        e.lat = o.value("lat").toDouble();
        e.lon = o.value("lon").toDouble();
        e.radiusKm = o.value("radiusKm").toDouble(100.0);
        e.note = o.value("note").toString().toStdString();
        for (const auto& m : o.value("moments").toArray()) {
            const auto mo = m.toObject();
            e.moments.push_back({QDateTime::fromString(mo.value("time").toString(), Qt::ISODate), mo.value("label").toString().toStdString()});
        }
        out.push_back(std::move(e));
    }
    return out;
}

void HistoryEvents::save(const std::vector<HistoryEvent>& events) {
    QJsonArray list;
    for (const auto& e : events) {
        QJsonObject o;
        o["id"] = QString::fromStdString(e.id);
        o["name"] = QString::fromStdString(e.name);
        o["source"] = QString::fromStdString(e.source);
        o["catalogId"] = QString::fromStdString(e.catalogId);
        o["start"] = e.start.toUTC().toString(Qt::ISODate);
        o["end"] = e.end.toUTC().toString(Qt::ISODate);
        o["lat"] = e.lat;
        o["lon"] = e.lon;
        o["radiusKm"] = e.radiusKm;
        o["note"] = QString::fromStdString(e.note);
        QJsonArray moments;
        for (const auto& m : e.moments) {
            moments.append(QJsonObject{{"time", m.time.toUTC().toString(Qt::ISODate)}, {"label", QString::fromStdString(m.label)}});
        }
        o["moments"] = moments;
        list.append(o);
    }
    QFile f{file() + ".part"};
    if (f.open(QIODevice::WriteOnly)) {
        f.write(QJsonDocument{list}.toJson());
        f.close();
        QFile::remove(file());
        f.rename(file());
    }
}
