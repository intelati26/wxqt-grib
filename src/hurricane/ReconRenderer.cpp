// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/ReconRenderer.h"
#include <algorithm>
#include <cmath>
#include <QFontMetricsF>
#include "hurricane/UtilityAtcf.h"
#include "ui/WindBarb.h"
#include "util/Utility.h"

namespace ReconRenderer {
    long newest(const Data& data) {
        long latest = 0;
        if (data.flights) {
            for (const auto& message : *data.flights) {
                for (const auto& ob : message.obs) {
                    latest = std::max(latest, ob.seconds);
                }
            }
        }
        if (data.fixes) {
            for (const auto& fix : *data.fixes) {
                latest = std::max(latest, fix.seconds);
            }
        }
        if (data.drops) {
            for (const auto& drop : *data.drops) {
                latest = std::max(latest, drop.seconds);
            }
        }
        return latest;
    }

    long cutoff(const Data& data, int hours) {
        if (hours <= 0) {
            return 0;
        }
        const long latest = newest(data);
        return latest == 0 ? 0 : latest - static_cast<long>(hours) * 3600;
    }

    bool recent(long seconds, long cut) {
        return cut == 0 || seconds == 0 || seconds >= cut;
    }

    QColor windColor(double knots) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 250, 244}, QColor{255, 255, 204}, QColor{255, 231, 117}, QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
        if (!UtilityHdob::has(knots)) {
            return QColor{150, 150, 150};
        }
        return colors[std::clamp(UtilityAtcf::categoryOf(static_cast<int>(knots)), 0, 6)];
    }

    void paintFlights(QPainter& painter, const Project& project, double px, const Data& data, const Settings& settings, const Accept& accept) {
        if (!data.flights) {
            return;
        }
        const long cut = cutoff(data, settings.hours);
        const auto scale = [&] (double knots) { return settings.color ? (UtilityHdob::has(knots) ? settings.color(knots) : QColor{190, 190, 190}) : windColor(knots); };
        const auto colorOf = [&] (const UtilityHdob::Ob& ob) { return scale(settings.sfmr ? ob.sfmrWind : ob.windSpeed); };
        const auto usable = [&] (const UtilityHdob::Ob& ob) { return recent(ob.seconds, cut) && (!accept || accept(ob.lat, ob.lon)); };
        if (settings.page) {   // the one-flight page: a segment at a time, coloured by the stronger wind of its ends; a gap of a quarter hour or more means the aircraft was away
            for (const auto& message : *data.flights) {
                const auto& obs = message.obs;
                const auto wind = [&] (const UtilityHdob::Ob& ob) { return settings.sfmr ? ob.sfmrWind : ob.windSpeed; };
                for (size_t i = 1; i < obs.size(); i++) {
                    if (!usable(obs[i - 1]) || obs[i].seconds - obs[i - 1].seconds > 900) {
                        continue;
                    }
                    const double a = wind(obs[i - 1]), b = wind(obs[i]);
                    const double kt = UtilityHdob::has(a) && UtilityHdob::has(b) ? std::max(a, b) : UtilityHdob::has(b) ? b : UtilityHdob::has(a) ? a : UtilityHdob::missing;
                    painter.setPen(QPen{scale(kt), 3.0 * px, Qt::SolidLine, Qt::RoundCap});
                    painter.drawLine(project(obs[i - 1].lat, obs[i - 1].lon), project(obs[i].lat, obs[i].lon));
                }
                if (settings.barbs) {
                    painter.setPen(QPen{QColor{15, 15, 15}, 1.2 * px});
                    painter.setBrush(QColor{15, 15, 15});
                    long lastBarb = 0;
                    for (const auto& ob : obs) {
                        if (!UtilityHdob::has(ob.windSpeed) || !UtilityHdob::has(ob.windDirection) || ob.seconds - lastBarb < 300 || !usable(ob)) {   // one barb every five minutes
                            continue;
                        }
                        lastBarb = ob.seconds;
                        WindBarb::draw(painter, project(ob.lat, ob.lon), ob.windDirection, ob.windSpeed, 18.0 * px, ob.lat < 0.0);
                    }
                }
            }
            return;
        }
        for (const auto& message : *data.flights) {
            const UtilityHdob::Ob * previous = nullptr;
            for (const auto& ob : message.obs) {
                if (!usable(ob)) {
                    previous = nullptr;
                    continue;
                }
                const auto p = project(ob.lat, ob.lon);
                const auto color = colorOf(ob);
                if (previous != nullptr) {
                    painter.setPen(QPen{QColor{color.red(), color.green(), color.blue(), 200}, 2.4 * px});
                    painter.drawLine(project(previous->lat, previous->lon), p);
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(p, 2.6 * px, 2.6 * px);
                previous = &ob;
            }
        }
        if (!settings.barbs) {
            return;
        }
        // flight-level wind barbs, thinned to one every ~34 pixels along each flight
        for (const auto& message : *data.flights) {
            bool haveLast = false;
            QPointF last;
            for (const auto& ob : message.obs) {
                if (!usable(ob) || !UtilityHdob::has(ob.windSpeed) || !UtilityHdob::has(ob.windDirection)) {
                    continue;
                }
                const auto p = project(ob.lat, ob.lon);
                if (haveLast && std::hypot(p.x() - last.x(), p.y() - last.y()) < 34.0 * px) {
                    continue;
                }
                haveLast = true;
                last = p;
                const auto color = colorOf(ob).darker(125);
                painter.setPen(QPen{color, 1.4 * px});
                painter.setBrush(color);
                WindBarb::draw(painter, p, ob.windDirection, ob.windSpeed, 26.0 * px, ob.lat < 0.0);
            }
        }
    }

    void paintFixes(QPainter& painter, const Project& project, double px, const Data& data, const Settings& settings, const Accept& accept) {
        if (!data.fixes) {
            return;
        }
        const long cut = cutoff(data, settings.hours);
        for (const auto& m : *data.fixes) {
            if (!UtilityVdm::has(m.lat) || !UtilityVdm::has(m.lon) || !recent(m.seconds, cut) || (accept && !accept(m.lat, m.lon))) {
                continue;
            }
            const auto at = project(m.lat, m.lon);
            if (settings.page) {   // a diamond with the pressure beside it
                QPolygonF diamond;
                diamond << at + QPointF{0, -7 * px} << at + QPointF{7 * px, 0} << at + QPointF{0, 7 * px} << at + QPointF{-7 * px, 0};
                painter.setPen(QPen{QColor{20, 20, 20}, 1.2 * px});
                painter.setBrush(QColor{255, 210, 60});
                painter.drawPolygon(diamond);
                if (UtilityVdm::has(m.pressure)) {
                    QFont small = painter.font();
                    small.setPixelSize(static_cast<int>(11 * px));
                    small.setBold(true);
                    painter.setFont(small);
                    painter.setPen(QColor{255, 232, 140});
                    painter.drawText(at + QPointF{9 * px, -4 * px}, QString::number(static_cast<int>(std::lround(m.pressure))) + " mb");
                }
                continue;
            }
            painter.setPen(QPen{QColor{255, 255, 255}, 1.6 * px});
            painter.setBrush(QColor{220, 40, 40, 200});
            painter.drawEllipse(at, 5.5 * px, 5.5 * px);
            painter.drawLine(at + QPointF{-8 * px, 0}, at + QPointF{8 * px, 0});
            painter.drawLine(at + QPointF{0, -8 * px}, at + QPointF{0, 8 * px});
        }
    }

    void paintDrops(QPainter& painter, const Project& project, double px, const Data& data, const Settings& settings, const Accept& accept) {
        if (!data.drops) {
            return;
        }
        const long cut = cutoff(data, settings.hours);
        if (settings.page) {   // where each sonde was released, with the lowest pressure and the strongest wind it measured beside it
            QFont small = painter.font();
            small.setPixelSize(static_cast<int>(11 * px));
            small.setBold(true);
            painter.setFont(small);
            for (const auto& drop : *data.drops) {
                const double lat = UtilityDropsonde::has(drop.releaseLat) ? drop.releaseLat : drop.lat, lon = UtilityDropsonde::has(drop.releaseLon) ? drop.releaseLon : drop.lon;
                if (!UtilityDropsonde::has(lat) || !UtilityDropsonde::has(lon) || !recent(drop.seconds, cut) || (accept && !accept(lat, lon))) {
                    continue;
                }
                const auto at = project(lat, lon);
                QPolygonF triangle;
                triangle << at + QPointF{0, -7 * px} << at + QPointF{7 * px, 6 * px} << at + QPointF{-7 * px, 6 * px};
                painter.setPen(QPen{QColor{20, 20, 20}, 1.2 * px});
                painter.setBrush(QColor{120, 220, 255});
                painter.drawPolygon(triangle);
                if (!settings.labels) {
                    continue;
                }
                const double pressure = UtilityDropsonde::minimumPressure(drop), wind = UtilityDropsonde::maxWind(drop);
                QString label;
                if (UtilityDropsonde::has(pressure)) {
                    label += QString::number(static_cast<int>(std::lround(pressure)));
                }
                if (UtilityDropsonde::has(wind)) {
                    label += (label.isEmpty() ? "" : " / ") + QString::number(static_cast<int>(std::lround(wind))) + " kt";
                }
                if (!label.isEmpty()) {
                    painter.setPen(QColor{190, 235, 255});
                    painter.drawText(at + QPointF{9 * px, 14 * px}, label);
                }
            }
            return;
        }
        std::vector<const UtilityDropsonde::Drop *> shown;
        for (const auto& d : *data.drops) {
            const double lat = UtilityDropsonde::has(d.splashLat) ? d.splashLat : d.lat;
            const double lon = UtilityDropsonde::has(d.splashLon) ? d.splashLon : d.lon;
            if (!UtilityDropsonde::has(lat) || !recent(d.seconds, cut) || (accept && !accept(lat, lon))) {
                continue;
            }
            const auto at = project(lat, lon);
            QPolygonF triangle;
            triangle << at + QPointF{0, 7 * px} << at + QPointF{-6 * px, -5 * px} << at + QPointF{6 * px, -5 * px};   // a downward triangle: a sonde falling
            painter.setPen(QPen{QColor{255, 255, 255}, 1.4 * px});
            painter.setBrush(QColor{60, 140, 255, 230});
            painter.drawPolygon(triangle);
            shown.push_back(&d);
        }
        if (!settings.labels) {
            return;
        }
        // the lowest pressure (the surface) and the strongest wind of each sonde beside its marker, where there is room for the text: the lowest pressure first, those in the eye are the ones to read
        QFont font{painter.font()};
        font.setPixelSize(static_cast<int>(10 * px));
        font.setBold(true);
        painter.setFont(font);
        const QFontMetricsF metrics{font};
        std::vector<QRectF> taken;
        std::stable_sort(shown.begin(), shown.end(), [] (const auto * a, const auto * b) {
            const double pa = UtilityDropsonde::minimumPressure(*a), pb = UtilityDropsonde::minimumPressure(*b);
            return (UtilityDropsonde::has(pa) ? pa : 9999.0) < (UtilityDropsonde::has(pb) ? pb : 9999.0);
        });
        for (const auto * d : shown) {
            const double lat = UtilityDropsonde::has(d->splashLat) ? d->splashLat : d->lat;
            const double lon = UtilityDropsonde::has(d->splashLon) ? d->splashLon : d->lon;
            const double pressure = UtilityDropsonde::minimumPressure(*d), wind = UtilityDropsonde::maxWind(*d);
            if (!UtilityDropsonde::has(pressure) && !UtilityDropsonde::has(wind)) {
                continue;
            }
            const QString text = (UtilityDropsonde::has(pressure) ? QString::number(static_cast<int>(std::lround(pressure))) + " mb" : QString{}) +
                (UtilityDropsonde::has(pressure) && UtilityDropsonde::has(wind) ? "  " : "") + (UtilityDropsonde::has(wind) ? QString::number(static_cast<int>(std::lround(wind))) + " kt" : QString{});
            const auto at = project(lat, lon);
            const QRectF box{at.x() + 9 * px, at.y() - 7 * px, metrics.horizontalAdvance(text) + 6 * px, 13 * px};
            if (std::any_of(taken.begin(), taken.end(), [&] (const QRectF& other) { return other.intersects(box); })) {
                continue;
            }
            taken.push_back(box);
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor{10, 20, 40, 190});
            painter.drawRoundedRect(box, 3 * px, 3 * px);
            painter.setPen(QColor{150, 205, 255});
            painter.drawText(box, Qt::AlignCenter, text);
        }
    }

    const std::vector<std::pair<const char *, int>>& hourChoices() {
        static const std::vector<std::pair<const char *, int>> choices{{"Recon: the last 3 hours", 3}, {"Recon: the last 6 hours", 6}, {"Recon: the last 12 hours", 12}, {"Recon: the last 24 hours", 24},
                                                                       {"Recon: the last 48 hours", 48}, {"Recon: all of it", 0}};
        return choices;
    }

    int savedHours(const std::string& pref, int standard) {
        return Utility::readPrefInt(pref, standard);
    }

    QComboBox * hoursCombo(QWidget * parent, const std::string& pref, const std::function<void(int)>& changed, int standard) {
        auto * combo = new QComboBox{parent};
        const int saved = savedHours(pref, standard);
        int at = 1;
        for (size_t i = 0; i < hourChoices().size(); i++) {
            combo->addItem(hourChoices()[i].first, hourChoices()[i].second);
            if (hourChoices()[i].second == saved) {
                at = static_cast<int>(i);
            }
        }
        combo->setCurrentIndex(at);
        combo->setToolTip("Only the reconnaissance of the last hours before the newest observation is drawn, so that it does not bury the track and the guidance");
        QObject::connect(combo, &QComboBox::currentIndexChanged, combo, [combo, pref, changed] {
            const int hours = combo->currentData().toInt();
            Utility::writePref(pref, std::to_string(hours));
            changed(hours);
        });
        return combo;
    }

    int bulletinsFor(int hours) {
        return hours <= 0 ? 400 : std::max(36, hours * 6);
    }
}
