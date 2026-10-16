// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/StormReportsLayer.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QTimeZone>
#include "objects/WString.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "spc/UtilitySpcStormReports.h"
#include "util/UtilityIO.h"

namespace {
    const QColor colors[] = {QColor{235, 40, 40}, QColor{60, 130, 255}, QColor{40, 190, 70}};
    const char * names[] = {"Tornado", "Wind", "Hail"};
}

QDate StormReportsLayer::day() const {
    const auto now = asOf.isValid() ? asOf.toUTC() : QDateTime::currentDateTimeUtc();
    return now.addSecs(-12 * 3600).date();
}

long StormReportsLayer::start() const {
    return QDateTime{day(), QTime{12, 0}, QTimeZone::utc()}.toSecsSinceEpoch();
}

vector<long> StormReportsLayer::times() const {
    vector<long> out;
    const long first = start();
    const long limit = asOf.isValid() ? first + 24 * 3600 : std::min<long>(first + 24 * 3600, static_cast<long>(QDateTime::currentSecsSinceEpoch() / 3600 * 3600));
    for (long s = first + 3600; s <= limit; s += 3600) {
        out.push_back(s);
    }
    return out;
}

string StormReportsLayer::timeText() const {
    const auto t = times();
    const long s = shown != 0 ? shown : (t.empty() ? start() : t.back());
    return "Reports by " + QDateTime::fromSecsSinceEpoch(s, QTimeZone::utc()).toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  SPC";
}

string StormReportsLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && reports.empty()) {
        return "reading the storm reports...";
    }
    size_t count[3] = {0, 0, 0};
    const auto t = times();
    const long upTo = shown != 0 ? shown : (t.empty() ? start() + 24 * 3600 : t.back());
    for (const auto& r : reports) {
        if (r.seconds <= upTo) {
            count[r.kind]++;
        }
    }
    return day().toString("yyyy-MM-dd").toStdString() + ": " + std::to_string(count[0]) + " tornado, " + std::to_string(count[1]) + " wind, " + std::to_string(count[2]) + " hail";
}

void StormReportsLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

void StormReportsLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    shown = asOf.toSecsSinceEpoch() / 3600 * 3600;
    reports.clear();
    loadedDay = QDate{};
    error.clear();
    refresh(host);
}

void StormReportsLayer::refresh(MapHost& host) {
    if (loading || (asOf.isValid() && loadedDay == day())) {
        return;
    }
    loading = true;
    const QDate wanted = day();
    const string url = "https://www.spc.noaa.gov/climo/reports/" + wanted.toString("yyMMdd").toStdString() + "_rpts.csv";
    auto text = std::make_shared<string>();
    host.background([text, url] { *text = UtilityIO::downloadAsByteArray(url).toStdString(); },
                    [this, text, wanted, &host] {
                        loading = false;
                        if (wanted != day()) {   // the time was set while it was reading: read the day now wanted
                            refresh(host);
                            return;
                        }
                        if (text->empty()) {
                            error = "no storm reports for " + wanted.toString("yyyy-MM-dd").toStdString();
                        } else {
                            error.clear();
                            reports.clear();
                            int kind = 0;
                            long base = QDateTime{wanted, QTime{12, 0}, QTimeZone::utc()}.toSecsSinceEpoch();
                            for (const auto& r : UtilitySpcStormReports::process(WString::split(*text, "\n"))) {
                                if (!r.damageHeader.empty()) {
                                    kind = r.damageHeader[0] == 'T' ? 0 : (r.damageHeader[0] == 'W' ? 1 : 2);
                                    continue;
                                }
                                if (r.time.size() < 4 || r.lat.empty() || r.lon.empty()) {
                                    continue;
                                }
                                const int hour = std::atoi(r.time.substr(0, 2).c_str());
                                const int minute = std::atoi(r.time.substr(2, 2).c_str());
                                long seconds = base - 12 * 3600 + hour * 3600 + minute * 60;   // from 00 UTC of the day the convective day begins
                                if (hour < 12) {
                                    seconds += 24 * 3600;   // after midnight UTC: the next calendar day
                                }
                                const QString magnitude = QString::fromStdString(r.magnitude);
                                const QString what = kind == 0 ? (magnitude == "UNK" ? "" : "F" + magnitude) : (kind == 1 ? magnitude + (magnitude == "UNK" ? "" : " mph") : magnitude + (magnitude.size() > 0 ? " (1/100 in)" : ""));
                                reports.push_back({kind, seconds, std::atof(r.lat.c_str()), std::atof(r.lon.c_str()),
                                                   QString{"%1  %2Z  %3\n%4, %5\n%6"}.arg(names[kind], QString::fromStdString(r.time.substr(0, 4)), what, QString::fromStdString(r.city), QString::fromStdString(r.state), QString::fromStdString(r.damageReport))});
                            }
                            loadedDay = wanted;
                        }
                        host.timesChanged();
                        host.redraw();
                    });
}

void StormReportsLayer::paint(QPainter& painter, MapHost& host) {
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    const auto list = times();
    const long upTo = shown != 0 ? shown : (list.empty() ? start() + 24 * 3600 : list.back());
    for (int kind = 2; kind >= 0; kind--) {   // tornadoes on top
        painter.setPen(QPen{QColor{20, 20, 20, 200}, 0.8 * px});
        painter.setBrush(colors[kind]);
        const double radius = (kind == 0 ? 4.5 : 3.2) * px;
        for (const auto& r : reports) {
            if (r.kind == kind && r.seconds <= upTo) {
                painter.drawEllipse(t(r.lat, r.lon), radius, radius);
            }
        }
    }
}

MapHit StormReportsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 9.0;
    hit.priority = 14;
    const auto list = times();
    const long upTo = shown != 0 ? shown : (list.empty() ? start() + 24 * 3600 : list.back());
    for (const auto& r : reports) {
        if (r.seconds > upTo) {
            continue;
        }
        const auto p = host.view().toPixels(r.lat, r.lon);
        const double d = std::hypot(p.x() - pixels.x(), p.y() - pixels.y());
        if (d < hit.distance) {
            hit.distance = d;
            hit.text = r.text;
        }
    }
    return hit;
}

vector<MapLegendRow> StormReportsLayer::legend() const {
    MapLegendRow row;
    row.title = "Storm reports:";
    for (int k = 0; k < 3; k++) {
        row.entries.push_back({MapLegendEntry::Circle, colors[k], names[k]});
    }
    return {row};
}
