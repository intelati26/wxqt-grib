// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MarkText.h"
#include "mapkit/ConeHistoryLayer.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QCheckBox>
#include <QLabel>
#include <QPainterPath>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    QColor categoryColor(int wind) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 235, 230}, QColor{255, 255, 204}, QColor{255, 231, 117}, QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
        return colors[std::clamp(wind >= 0 ? UtilityAtcf::categoryOf(wind) : 0, 0, 6)];
    }

    QPainterPath ringsPath(const MapView::Transform& t, const UtilityNhcGis::Cone& cone) {
        QPainterPath path;
        for (const auto& ring : cone.polygons) {
            QPolygonF polygon;
            for (const auto& [lon, lat] : ring) {
                polygon << t(lat, lon);
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        return path;
    }

    QPainterPath linesPath(const MapView::Transform& t, const UtilityNhcGis::Cone& cone) {
        QPainterPath path;
        for (const auto& line : cone.lines) {
            for (size_t i = 0; i < line.size(); i++) {
                const auto p = t(line[i].second, line[i].first);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
        }
        return path;
    }

    QDateTime utcOf(long seconds) { return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()); }

    // where a forecast point of an advisory was observed to be, for the miss: the point's own valid time is the advisory's plus its hour
    bool miss(const UtilityNhcArchive::Storm& storm, const UtilityNhcArchive::Advisory& advisory, const UtilityNhcGis::Point& point, double& nm, double& lat, double& lon) {
        if (point.tau <= 0 || storm.best.size() < 2) {
            return false;
        }
        int wind = 0;
        if (!UtilityNhcArchive::observedAt(storm.best, advisory.issued.addSecs(point.tau * 3600LL), lat, lon, wind)) {
            return false;
        }
        nm = UtilityNhcArchive::distanceNm(point.lat, point.lon, lat, lon);
        return true;
    }
}

ConeHistoryLayer::ConeHistoryLayer() {
    ghosts = Utility::readPref("MM_CONEHIST_GHOSTS", "true") == "true";
    observed = Utility::readPref("MM_CONEHIST_BEST", "true") == "true";
    points = Utility::readPref("MM_CONEHIST_POINTS", "true") == "true";
}

QDateTime ConeHistoryLayer::when() const {
    if (shown != 0) {
        return utcOf(shown);
    }
    return asOf.isValid() ? asOf : QDateTime::currentDateTimeUtc();
}

void ConeHistoryLayer::listYear(MapHost& host) {
    const int year = yearChoice != 0 ? yearChoice : when().date().year();
    if (yearListed == year) {
        return;
    }
    yearListed = year;
    auto result = std::make_shared<vector<UtilityNhcArchive::StormRef>>();
    host.background([year, result] { *result = UtilityNhcArchive::stormsOfYear(year); },
                    [this, result, year] {
                        if (year != yearListed) {
                            return;
                        }
                        yearList = std::move(*result);
                        if (stormBox) {
                            const QString current = QString::fromStdString(stormChoice);
                            stormBox->blockSignals(true);
                            stormBox->clear();
                            stormBox->addItem("Every storm active at the time on the bar", QString{});
                            for (const auto& s : yearList) {
                                stormBox->addItem(QString::fromStdString(s.name + " (" + s.id + ")"), QString::fromStdString(s.id));
                            }
                            const int index = stormBox->findData(current);
                            stormBox->setCurrentIndex(index < 0 ? 0 : index);
                            stormBox->blockSignals(false);
                        }
                    });
}

void ConeHistoryLayer::refresh(MapHost& host) {
    hostPointer = &host;
    if (loading) {
        relist = true;
        return;
    }
    loading = true;
    relist = false;
    UtilityNhcArchive::setStage("looking for storms");
    {   // the progress line, twice a second, until the reading is done
        const auto flag = alive;
        auto tick = std::make_shared<std::function<void()>>();
        *tick = [this, &host, flag, tick] {
            if (*flag && loading) {
                host.status(summary());
                QTimer::singleShot(500, [tick] { (*tick)(); });
            }
        };
        QTimer::singleShot(500, [tick] { (*tick)(); });
    }
    const string chosen = stormChoice;
    const QDateTime at = when();
    const int year = yearChoice != 0 ? yearChoice : at.date().year();
    auto result = std::make_shared<vector<std::shared_ptr<UtilityNhcArchive::Storm>>>();
    host.background(
        [chosen, at, year, result] {
            vector<string> ids;
            if (!chosen.empty()) {
                ids.push_back(chosen);
            } else {
                // the storms that had started by then and could still be going (a storm lasts a few weeks at most)
                for (const auto& ref : UtilityNhcArchive::stormsOfYear(year)) {
                    if (!ref.first.isValid() || (ref.first <= at.addSecs(3600) && ref.first.daysTo(at) <= 18)) {
                        ids.push_back(ref.id);
                    }
                }
                if (at.date().month() <= 1) {   // the early days of January can still belong to December's storm
                    for (const auto& ref : UtilityNhcArchive::stormsOfYear(year - 1)) {
                        if (ref.first.isValid() && ref.first.daysTo(at) <= 28) {
                            ids.push_back(ref.id);
                        }
                    }
                }
            }
            size_t number = 0;
            for (const auto& id : ids) {
                UtilityNhcArchive::setStage("storm " + std::to_string(++number) + " of " + std::to_string(ids.size()) + ", " + id);
                auto storm = std::make_shared<UtilityNhcArchive::Storm>(UtilityNhcArchive::load(id, true));
                if (!storm->advisories.empty()) {
                    result->push_back(std::move(storm));
                }
            }
        },
        [this, &host, result, chosen] {
            loading = false;
            vector<std::shared_ptr<UtilityNhcArchive::Storm>> kept;
            for (auto& storm : *result) {
                // every storm of the "active" mode must have been going at the time: after its last advisory it is no longer shown
                if (!chosen.empty() || storm->last().addSecs(14 * 3600) >= when().addSecs(-1) || storm->last() >= when()) {
                    kept.push_back(storm);
                }
            }
            storms = std::move(kept);
            error = storms.empty() ? (chosen.empty() ? "no storm had an NHC cone at that time" : "no advisories found for that storm") : string{};
            listYear(host);
            host.timesChanged();
            host.redraw();
            if (relist) {
                refresh(host);
            }
        });
}

vector<long> ConeHistoryLayer::times() const {
    std::set<long> all;
    for (const auto& storm : storms) {
        for (const auto& advisory : storm->advisories) {
            all.insert(static_cast<long>(advisory.issued.toSecsSinceEpoch()));
        }
    }
    return {all.begin(), all.end()};
}

void ConeHistoryLayer::showTime(long seconds, MapHost& host) {
    const bool wasActive = stormChoice.empty();
    shown = seconds;
    if (wasActive && seconds != 0) {
        host.redraw();
    } else {
        host.redraw();
    }
}

void ConeHistoryLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    shown = static_cast<long>(asOf.toSecsSinceEpoch());
    storms.clear();
    refresh(host);
}

string ConeHistoryLayer::timeText() const {
    const auto now = shownNow();
    if (now.empty()) {
        return {};
    }
    return "NHC advisories to " + now.front().storm->advisories[now.front().index].issued.toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
}

vector<ConeHistoryLayer::Shown> ConeHistoryLayer::shownNow() const {
    vector<Shown> out;
    const long at = shown != 0 ? shown : (asOf.isValid() ? static_cast<long>(asOf.toSecsSinceEpoch()) : static_cast<long>(QDateTime::currentSecsSinceEpoch()));
    for (const auto& storm : storms) {
        const auto& list = storm->advisories;
        long newest = -1;
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i].issued.toSecsSinceEpoch() <= at) {
                newest = static_cast<long>(i);
            }
        }
        if (newest < 0) {
            continue;
        }
        // a storm that has ended (its last advisory is the one on show and it is more than half a day old at the time) is not shown; a chosen storm always is
        if (stormChoice.empty() && static_cast<size_t>(newest) + 1 == list.size() && list.back().issued.secsTo(utcOf(at)) > 14 * 3600) {
            continue;
        }
        out.push_back({storm.get(), static_cast<size_t>(newest)});
    }
    return out;
}

void ConeHistoryLayer::paint(QPainter& painter, MapHost& host) {
    const auto now = shownNow();
    if (now.empty()) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    painter.save();
    painter.setFont(font);
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& [storm, index] : now) {
        const auto& current = storm->advisories[index];
        if (ghosts) {   // the earlier forecasts, older fainter
            for (size_t i = 0; i < index; i++) {
                const double age = static_cast<double>(index - i);
                const int alpha = static_cast<int>(std::clamp(120.0 - age * 6.0, 28.0, 120.0));
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen{QColor{255, 255, 255, alpha}, 0.9 * px});
                painter.drawPath(ringsPath(t, storm->advisories[i].cone));
            }
        }
        painter.setPen(QPen{QColor{255, 255, 255, 215}, 1.5 * px, Qt::DashLine});
        painter.setBrush(QColor{255, 255, 255, 42});
        painter.drawPath(ringsPath(t, current.cone));
        const auto line = linesPath(t, current.cone);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{QColor{0, 0, 0, 200}, 4.4 * px});
        painter.drawPath(line);
        painter.setPen(QPen{QColor{255, 255, 255}, 2.2 * px});
        painter.drawPath(line);
        for (const auto& warning : current.cone.warnings) {   // the coast under a watch or warning of this advisory
            QPainterPath coast;
            for (const auto& ring : warning.lines) {
                for (size_t i = 0; i < ring.size(); i++) {
                    const auto p = t(ring[i].second, ring[i].first);
                    i == 0 ? coast.moveTo(p) : coast.lineTo(p);
                }
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 7.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            painter.drawPath(coast);
            painter.setPen(QPen{QColor{QString::fromStdString(UtilityNhcGis::colorFor(warning.code))}, 4.4 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            painter.drawPath(coast);
        }
        if (points) {
            for (const auto& p : current.cone.points) {
                const auto at = t(p.lat, p.lon);
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 0.9 * px});
                painter.setBrush(categoryColor(static_cast<int>(p.maxWind)));
                painter.drawEllipse(at, 4.0 * px, 4.0 * px);
                if (p.tau > 0 && p.tau % 24 == 0) {
                    painter.setPen(QColor{255, 255, 255});
                    MarkText::at(host, painter, px, p.lat, p.lon, QPointF{7.0, -5.0}, QString::number(p.tau / 24) + " d");
                }
            }
        }
        // the track the storm then took: all of it, so that each forecast shows against it
        if (observed && storm->best.size() >= 2) {
            QPainterPath path;
            for (size_t i = 0; i < storm->best.size(); i++) {
                const auto p = t(storm->best[i].lat, storm->best[i].lon);
                i == 0 ? path.moveTo(p) : path.lineTo(p);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 210}, 4.0 * px});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{255, 70, 200}, 1.8 * px});
            painter.drawPath(path);
            for (size_t i = 0; i < storm->best.size(); i += 4) {
                painter.setPen(QPen{QColor{0, 0, 0, 220}, 0.8 * px});
                painter.setBrush(categoryColor(storm->best[i].wind));
                painter.drawEllipse(t(storm->best[i].lat, storm->best[i].lon), 2.8 * px, 2.8 * px);
            }
        }
        // the name where the storm was
        painter.setPen(QColor{255, 255, 255});
        if (!current.cone.points.empty()) {
            const auto& first = current.cone.points.front();
            MarkText::at(host, painter, px, first.lat, first.lon, QPointF{8.0, 12.0}, QString::fromStdString(current.cone.stormName.empty() ? storm->name : current.cone.stormName) + "  adv " + QString::fromStdString(current.number));
        }
    }
    painter.restore();
}

MapHit ConeHistoryLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 11.0;
    const auto now = shownNow();
    for (const auto& [storm, index] : now) {
        const auto& advisory = storm->advisories[index];
        for (const auto& p : advisory.cone.points) {
            const auto at = host.view().toPixels(p.lat, p.lon);
            const double d = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (d >= hit.distance) {
                continue;
            }
            double nm = 0.0, lat = 0.0, lon = 0.0;
            QString text = QString::fromStdString(advisory.cone.stormName.empty() ? storm->name : advisory.cone.stormName) + ", advisory " + QString::fromStdString(advisory.number) + " (" + advisory.issued.toString("MMM d HH:mm") + "Z)\n" +
                (p.tau == 0 ? QString{"now"} : QString{"+%1 h"}.arg(p.tau)) + ": " + QString::fromStdString(p.development) + ", " + (p.maxWind >= 0 ? QString::number(static_cast<int>(p.maxWind)) + " kt" : QString{"-"}) + "  " +
                QString::number(p.lat, 'f', 1) + " N " + QString::number(-p.lon, 'f', 1) + " W";
            if (miss(*storm, advisory, p, nm, lat, lon)) {
                text += QString{"\nobserved then: %1 N %2 W, the forecast was %3 nm off"}.arg(lat, 0, 'f', 1).arg(-lon, 0, 'f', 1).arg(static_cast<int>(std::lround(nm)));
            }
            hit.distance = d;
            hit.text = text;
        }
    }
    return hit;
}

vector<MapLegendRow> ConeHistoryLayer::legend() const {
    if (shownNow().empty()) {
        return {};
    }
    MapLegendRow row;
    row.title = "NHC forecast history:";
    row.entries.push_back({MapLegendEntry::Line, QColor{255, 255, 255}, "forecast track and cone"});
    if (ghosts) {
        row.entries.push_back({MapLegendEntry::Line, QColor{255, 255, 255, 90}, "earlier cones"});
    }
    if (observed) {
        row.entries.push_back({MapLegendEntry::Line, QColor{255, 70, 200}, "track the storm took"});
    }
    std::set<string> codes;
    for (const auto& [storm, index] : shownNow()) {
        for (const auto& w : storm->advisories[index].cone.warnings) {
            if (codes.insert(w.code).second) {
                row.entries.push_back({MapLegendEntry::Line, QColor{QString::fromStdString(UtilityNhcGis::colorFor(w.code))}, QString::fromStdString(w.kind)});
            }
        }
    }
    return {row};
}

string ConeHistoryLayer::summary() const {
    if (loading) {
        return "reading the NHC archive: " + UtilityNhcArchive::progressText();
    }
    if (!error.empty()) {
        return error;
    }
    const auto now = shownNow();
    if (now.empty()) {
        return {};
    }
    string text;
    for (const auto& [storm, index] : now) {
        const auto& advisory = storm->advisories[index];
        text += (text.empty() ? "" : "; ") + (advisory.cone.stormName.empty() ? storm->name : advisory.cone.stormName) + " adv " + advisory.number;
        // the miss at each day of this advisory's forecast
        string errors;
        for (const auto& p : advisory.cone.points) {
            double nm = 0.0, lat = 0.0, lon = 0.0;
            if (p.tau > 0 && p.tau % 24 == 0 && miss(*storm, advisory, p, nm, lat, lon)) {
                errors += (errors.empty() ? "" : ", ") + std::to_string(p.tau / 24) + " d " + std::to_string(static_cast<int>(std::lround(nm))) + " nm";
            }
        }
        if (!errors.empty()) {
            text += " (off by " + errors + ")";
        }
    }
    return text;
}

QWidget * ConeHistoryLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Year:", widget});
    auto * year = new QComboBox{widget};
    year->addItem("The year of the time on the bar", 0);
    for (int y = QDate::currentDate().year(); y >= 2008; y--) {
        year->addItem(QString::number(y), y);
    }
    year->setCurrentIndex(std::max(0, year->findData(yearChoice)));
    layout->addWidget(year);
    layout->addWidget(new QLabel{"Storm:", widget});
    stormBox = new QComboBox{widget};
    stormBox->addItem("Every storm active at the time on the bar", QString{});
    for (const auto& s : yearList) {
        stormBox->addItem(QString::fromStdString(s.name + " (" + s.id + ")"), QString::fromStdString(s.id));
    }
    stormBox->setCurrentIndex(std::max(0, stormBox->findData(QString::fromStdString(stormChoice))));
    layout->addWidget(stormBox);
    auto * faded = new QCheckBox{"Earlier cones, faded", widget};
    faded->setChecked(ghosts);
    layout->addWidget(faded);
    auto * best = new QCheckBox{"The track the storm took (best track)", widget};
    best->setChecked(observed);
    layout->addWidget(best);
    auto * dots = new QCheckBox{"Forecast points", widget};
    dots->setChecked(points);
    layout->addWidget(dots);
    layout->addWidget(new QLabel{"Advisories are downloaded once (about 30 KB each) and kept.", widget});
    QObject::connect(year, &QComboBox::currentIndexChanged, [this, year, changed] (int) {
        yearChoice = year->currentData().toInt();
        yearListed = 0;
        stormChoice.clear();
        storms.clear();
        changed();
        if (hostPointer) {
            refresh(*hostPointer);
        }
    });
    QObject::connect(stormBox.data(), &QComboBox::currentIndexChanged, [this, changed] (int) {
        stormChoice = stormBox->currentData().toString().toStdString();
        storms.clear();
        shown = 0;
        changed();
        if (hostPointer) {
            refresh(*hostPointer);
        }
    });
    QObject::connect(faded, &QCheckBox::toggled, [this, changed] (bool on) { ghosts = on; Utility::writePref("MM_CONEHIST_GHOSTS", on ? "true" : "false"); changed(); });
    QObject::connect(best, &QCheckBox::toggled, [this, changed] (bool on) { observed = on; Utility::writePref("MM_CONEHIST_BEST", on ? "true" : "false"); changed(); });
    QObject::connect(dots, &QCheckBox::toggled, [this, changed] (bool on) { points = on; Utility::writePref("MM_CONEHIST_POINTS", on ? "true" : "false"); changed(); });
    if (hostPointer && yearList.empty()) {
        listYear(*hostPointer);
    }
    return widget;
}
