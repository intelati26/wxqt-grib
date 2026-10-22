// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MarkText.h"
#include "mapkit/OutlookHistoryLayer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainterPath>
#include <QTimeZone>
#include <QTimer>
#include <QVBoxLayout>
#include "hurricane/UtilityWindProbability.h"
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    QColor riskColor(const UtilityNhcGis::OutlookArea& area) {
        const auto& risk = area.risk7.empty() ? area.risk2 : area.risk7;
        return risk == "High" ? QColor{255, 60, 60} : risk == "Medium" ? QColor{255, 150, 30} : QColor{255, 225, 60};
    }

    QPainterPath ringsPath(const MapView::Transform& t, const UtilityNhcGis::OutlookArea& area) {
        QPainterPath path;
        for (const auto& ring : area.rings) {
            QPolygonF polygon;
            for (const auto& [lon, lat] : ring) {
                polygon << t(lat, lon);
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        return path;
    }

    QDateTime utcOf(long seconds) { return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()); }
}

OutlookHistoryLayer::OutlookHistoryLayer() {
    windowDays = std::clamp(Utility::readPrefInt("MM_OUTLOOKHIST_DAYS", 6), 2, 30);
    ghosts = Utility::readPref("MM_OUTLOOKHIST_GHOSTS", "true") == "true";
}

QDateTime OutlookHistoryLayer::when() const {
    if (shown != 0) {
        return utcOf(shown);
    }
    return asOf.isValid() ? asOf : QDateTime::currentDateTimeUtc();
}

void OutlookHistoryLayer::refresh(MapHost& host) {
    if (loading) {
        relist = true;
        return;
    }
    loading = true;
    relist = false;
    UtilityNhcArchive::setStage("looking for the outlooks");
    {
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
    // live: the last days; at a past time: the days around it (the weeks of an area's life run both ways from the moment looked at)
    const QDateTime centre = asOf.isValid() ? asOf : QDateTime::currentDateTimeUtc();
    const QDateTime from = centre.addDays(-windowDays);
    const QDateTime to = asOf.isValid() ? centre.addDays(windowDays) : centre.addSecs(3600);
    auto result = std::make_shared<vector<UtilityNhcArchive::Outlook>>();
    host.background([from, to, result] { *result = UtilityNhcArchive::outlooks(from, to); },
                    [this, &host, result] {
                        loading = false;
                        issuances = std::move(*result);
                        error = issuances.empty() ? "no Tropical Weather Outlook issuances were found for those days" : string{};
                        host.timesChanged();
                        host.redraw();
                        if (relist) {
                            refresh(host);
                        }
                    });
}

vector<long> OutlookHistoryLayer::times() const {
    vector<long> out;
    for (const auto& o : issuances) {
        out.push_back(static_cast<long>(o.issued.toSecsSinceEpoch()));
    }
    return out;
}

void OutlookHistoryLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

void OutlookHistoryLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    shown = static_cast<long>(asOf.toSecsSinceEpoch());
    issuances.clear();
    refresh(host);
}

int OutlookHistoryLayer::current() const {
    const long at = static_cast<long>(when().toSecsSinceEpoch());
    int found = -1;
    for (size_t i = 0; i < issuances.size(); i++) {
        if (issuances[i].issued.toSecsSinceEpoch() <= at) {
            found = static_cast<int>(i);
        }
    }
    return found;
}

string OutlookHistoryLayer::timeText() const {
    const int now = current();
    return now < 0 ? string{} : "Outlook issued " + issuances[static_cast<size_t>(now)].issued.toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
}

string OutlookHistoryLayer::summary() const {
    if (loading) {
        return "reading the outlooks: " + UtilityNhcArchive::progressText();
    }
    if (!error.empty()) {
        return error;
    }
    const int now = current();
    if (now < 0) {
        return {};
    }
    const auto& o = issuances[static_cast<size_t>(now)];
    return std::to_string(o.areas.size()) + (o.areas.size() == 1 ? " development area" : " development areas") + ", outlook of " + o.issued.toString("MMM d HH:mm").toStdString() + "Z";
}

void OutlookHistoryLayer::paint(QPainter& painter, MapHost& host) {
    const int now = current();
    if (now < 0) {
        return;
    }
    const auto t = host.view().transform();
    const double px = host.view().unitsPerPixel();
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    if (ghosts) {   // the earlier issuances: where the areas were, older fainter
        for (int i = std::max(0, now - 16); i < now; i++) {
            const int alpha = static_cast<int>(std::clamp(110.0 - (now - i) * 6.0, 24.0, 110.0));
            for (const auto& area : issuances[static_cast<size_t>(i)].areas) {
                QColor c = riskColor(area);
                c.setAlpha(alpha);
                painter.setBrush(Qt::NoBrush);
                painter.setPen(QPen{c, 1.0 * px, Qt::DotLine});
                painter.drawPath(ringsPath(t, area));
            }
        }
    }
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(12 * px));
    font.setBold(true);
    painter.setFont(font);
    for (const auto& area : issuances[static_cast<size_t>(now)].areas) {
        const QColor color = riskColor(area);
        QColor fill = color;
        fill.setAlpha(55);
        painter.setPen(QPen{color, 2.2 * px, Qt::DashLine});
        painter.setBrush(fill);
        painter.drawPath(ringsPath(t, area));
        painter.setPen(color.lighter(130));
        MarkText::at(host, painter, px, area.centerLat, area.centerLon, QPointF{-30.0, 4.0}, QString::number(area.prob2) + "% / " + QString::number(area.prob7) + "%");
    }
    painter.restore();
}

MapHit OutlookHistoryLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = 1;
    const int now = current();
    if (now < 0) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const auto& issuance = issuances[static_cast<size_t>(now)];
    for (const auto& area : issuance.areas) {
        for (const auto& ring : area.rings) {
            if (ring.size() >= 3 && UtilityWindProbability::inside(ring, lat, lon)) {
                hit.distance = 0.0;
                hit.text = "Tropical Weather Outlook area " + QString::fromStdString(area.area) + ", issued " + issuance.issued.toString("MMM d HH:mm") + "Z\nChance of formation: " + QString::number(area.prob2) +
                    " % in 2 days (" + QString::fromStdString(area.risk2) + "), " + QString::number(area.prob7) + " % in 7 days (" + QString::fromStdString(area.risk7) + ")";
                // how it changed from the issuance before
                if (now > 0) {
                    for (const auto& before : issuances[static_cast<size_t>(now - 1)].areas) {
                        if (before.area == area.area && before.basin == area.basin) {
                            hit.text += "\nissuance before: " + QString::number(before.prob2) + " % / " + QString::number(before.prob7) + " %";
                            break;
                        }
                    }
                }
                return hit;
            }
        }
    }
    return hit;
}

vector<MapLegendRow> OutlookHistoryLayer::legend() const {
    if (current() < 0) {
        return {};
    }
    return {MapLegendRow{"Development chance (7 days):", {{MapLegendEntry::Square, QColor{255, 225, 60}, "low"}, {MapLegendEntry::Square, QColor{255, 150, 30}, "medium"}, {MapLegendEntry::Square, QColor{255, 60, 60}, "high"}}}};
}

QWidget * OutlookHistoryLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Issuances to step through (days either side of the time):", widget});
    auto * days = new QComboBox{widget};
    int current = 0;
    for (const int d : {3, 6, 10, 14, 21}) {
        days->addItem(QString::number(d) + " days", d);
        if (d <= windowDays) {
            current = days->count() - 1;
        }
    }
    days->setCurrentIndex(current);
    layout->addWidget(days);
    auto * faded = new QCheckBox{"Earlier issuances, faded", widget};
    faded->setChecked(ghosts);
    layout->addWidget(faded);
    layout->addWidget(new QLabel{"Each issuance is downloaded once (about 17 KB) and kept.", widget});
    QObject::connect(days, &QComboBox::currentIndexChanged, [this, days, changed] (int) {
        windowDays = days->currentData().toInt();
        Utility::writePrefInt("MM_OUTLOOKHIST_DAYS", windowDays);
        issuances.clear();
        changed();
    });
    QObject::connect(faded, &QCheckBox::toggled, [this, changed] (bool on) {
        ghosts = on;
        Utility::writePref("MM_OUTLOOKHIST_GHOSTS", on ? "true" : "false");
        changed();
    });
    return widget;
}
