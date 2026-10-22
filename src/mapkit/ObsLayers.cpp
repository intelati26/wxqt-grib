// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ObsLayers.h"
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <ctime>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include "misc/TextViewerStatic.h"
#include "obs/SurfaceData.h"
#include "obs/SurfaceHistoryViewer.h"
#include "obs/SurfaceStationText.h"
#include "settings/UIPreferences.h"
#include "ui/WindBarb.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

namespace {
    // the display choices both station layers share (kept in the settings)
    struct Style {
        bool barbs{true};
        bool values{true};
        int color{0};      // 0 flight category / network, 1 temperature
        int units{0};      // 0 F, 1 C
        int network{0};
        Style() {
            barbs = Utility::readPref("MM_OBS_BARBS", "true") == "true";
            values = Utility::readPref("MM_OBS_VALUES", "true") == "true";
            color = Utility::readPrefInt("MM_OBS_COLOR", 0);
            units = Utility::readPrefInt("MM_OBS_UNITS", UIPreferences::unitsF ? 0 : 1);
            network = Utility::readPrefInt("MM_OBS_NETWORK", 0);
        }
    };

    Style& style() {
        static Style s;
        return s;
    }

    int rankOf(const SurfaceStation& s) {
        if (s.airport) return 0;
        if (s.network == "APRSWXNET") return 3;
        if (s.network == "MesoWest" || s.network == "NonFedAWOS") return 2;
        return 1;
    }

    QString number(double v) {
        return QString::number(std::lround(v));
    }
}

string StationLayer::tip() const {
    return airports ? "The newest METAR of each airport weather station (ASOS, AWOS and the others), from the Aviation Weather Center"
                    : "State mesonets, RAWS, road weather, hydrological and citizen stations from NOAA MADIS' public mesonet files (about 30 MB each, two read)";
}

string StationLayer::summary() const {
    if (loading) {
        return airports ? "reading airport reports..." : "reading the MADIS mesonet files (about 60 MB)...";
    }
    if (!error.empty()) {
        return error;
    }
    if (!stations) {
        return {};
    }
    return std::to_string(stations->size()) + (airports ? " airport reports" : " mesonet stations");
}

// "MADIS mesonet file 18Z (2026-10-22), last written 18:43Z (3 min ago); the hour before: 17Z, written 18:39Z"
string StationLayer::timeText() const {
    if (airports || !stations) {
        return {};
    }
    const auto files = SurfaceData::mesonetFiles();
    if (files.empty()) {
        return {};
    }
    const auto describe = [] (const SurfaceData::MadisFile& f) {
        const auto hour = QDateTime::fromString(QString::fromStdString(f.name.substr(0, 13)), "yyyyMMdd_HHmm");
        QString text = hour.isValid() ? hour.toString("HH") + "Z (" + hour.toString("yyyy-MM-dd") + ")" : QString::fromStdString(f.name);
        if (f.modified > 0) {
            const auto written = QDateTime::fromSecsSinceEpoch(f.modified, QTimeZone::utc());
            const long long minutes = std::max(0LL, (QDateTime::currentSecsSinceEpoch() - f.modified) / 60);
            text += ", last written " + written.toString("HH:mm") + "Z (" + (minutes < 90 ? QString::number(minutes) + " min ago" : QString::number(minutes / 60) + " h ago") + ")";
        }
        return text.toStdString();
    };
    string text = "MADIS mesonet file " + describe(files.back());
    if (files.size() > 1) {
        text += "; the hour before: " + describe(files[files.size() - 2]);
    }
    return text;
}

void StationLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<std::vector<SurfaceStation>>();
    auto message = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    const bool wantAirports = airports;
    host.background(
        [fresh, message, ok, wantAirports] {
            if (wantAirports) {
                *ok = SurfaceData::loadMetars(*fresh, *message);
            } else {
                std::vector<SurfaceStation> metars;
                string ignored;
                SurfaceData::loadMetars(metars, ignored);
                *ok = SurfaceData::loadMesonet(*fresh, *message, metars);
            }
            std::stable_sort(fresh->begin(), fresh->end(), [] (const SurfaceStation& a, const SurfaceStation& b) {
                if (rankOf(a) != rankOf(b)) return rankOf(a) < rankOf(b);
                const bool ta = SurfaceStation::has(a.temperature);
                const bool tb = SurfaceStation::has(b.temperature);
                if (ta != tb) return ta;
                return a.seconds > b.seconds;
            });
        },
        [this, &host, fresh, message, ok] {
            loading = false;
            if (*ok) {
                stations = fresh;
                error.clear();
            } else {
                error = *message;
            }
            host.redraw();
        });
}

bool StationLayer::networkShown(const SurfaceStation& s) const {
    if (s.airport) {
        return true;
    }
    const auto& n = s.network;
    switch (style().network) {
        case 1: return n != "APRSWXNET";
        case 2: return n == "RAWS";
        case 3: return n == "HADS";
        case 4: return n == "MesoWest";
        case 5: return n != "APRSWXNET" && n != "RAWS" && n != "HADS" && n != "MesoWest" && n != "NonFedAWOS";
        case 6: return n == "APRSWXNET";
        default: return true;
    }
}

QColor StationLayer::dotColor(const SurfaceStation& s) const {
    if (style().color == 1 || !s.airport) {
        if (style().color == 0) {
            return QColor{110, 150, 215};
        }
        if (!SurfaceStation::has(s.temperature)) {
            return QColor{130, 130, 140};
        }
        const double f = s.temperature * 1.8 + 32.0;
        return f < 0 ? QColor{170, 110, 240} : f < 32 ? QColor{70, 110, 240} : f < 50 ? QColor{50, 190, 230} : f < 70 ? QColor{70, 200, 100} :
            f < 85 ? QColor{240, 215, 50} : f < 95 ? QColor{255, 150, 30} : QColor{235, 60, 50};
    }
    if (s.flight == "VFR") return QColor{60, 200, 80};
    if (s.flight == "MVFR") return QColor{70, 140, 255};
    if (s.flight == "IFR") return QColor{240, 60, 60};
    if (s.flight == "LIFR") return QColor{235, 70, 220};
    return QColor{140, 140, 150};
}

void StationLayer::paint(QPainter& painter, MapHost& host) {
    drawn.clear();
    if (!stations) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    const int widthPx = std::max(1, view.pixelWidth());
    const double spacing = (style().barbs || style().values) ? 46.0 : 14.0;
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(11.0 * perPixel))));
    painter.setFont(font);
    (void)widthPx;
    const long now = static_cast<long>(std::time(nullptr));
    for (size_t i = 0; i < stations->size(); i++) {
        const auto& s = (*stations)[i];
        if (!networkShown(s)) {
            continue;
        }
        const QPointF at = t(s.lat, s.lon);
        if (at.x() < -500.0 || at.x() > 500.0 || at.y() < -250.0 || at.y() > -250.0 + 1000.0 * host.view().pixelHeight() / std::max(1, host.view().pixelWidth())) {
            continue;
        }
        const QPointF pixels{(at.x() + 500.0) / perPixel, (at.y() + 250.0) / perPixel};
        if (!host.claimCell(pixels, spacing)) {
            continue;
        }
        drawn.push_back({i, pixels});
        const bool old = now - s.seconds > (s.airport ? 7200 : 10800);
        const QColor color = old ? QColor{110, 110, 118} : dotColor(s);
        const auto st = style();   // (the mark may be painted later, when the options have changed)
        const SurfaceStation station = s;
        // the mark, painted at the origin with `unit` the size of a pixel of it: here at once, or later on a world picture where a polar projection would turn it
        const auto mark = [st, station, old, color, font] (QPainter& p, double unit, double north) {
            QFont f{font};
            f.setPixelSize(std::max(6, static_cast<int>(std::lround(11.0 * unit))));
            p.setFont(f);
            const auto& s = station;
            if (st.barbs && SurfaceStation::has(s.windSpeed) && !old) {
                const QColor barb{225, 228, 235};
                p.save();
                p.rotate(north);
                p.setPen(QPen{barb, 1.2 * unit});
                p.setBrush(barb);
                WindBarb::draw(p, QPointF{0.0, 0.0}, SurfaceStation::has(s.windDirection) ? s.windDirection : 0.0, s.windSpeed, 24.0 * unit, s.lat < 0.0);
                p.restore();
            }
            p.setPen(QPen{QColor{255, 255, 255, 215}, 1.0 * unit});
            p.setBrush(color);
            const double r = 3.6 * unit;
            if (s.airport) {
                p.drawEllipse(QPointF{0.0, 0.0}, r, r);
            } else {
                p.drawRect(QRectF{-r * 0.85, -r * 0.85, r * 1.7, r * 1.7});
            }
            if (st.values && !old) {
                const QFontMetricsF metrics{f};
                const auto degrees = [&st] (double c) { return st.units == 0 ? c * 1.8 + 32.0 : c; };
                if (SurfaceStation::has(s.temperature)) {
                    const QString text = number(degrees(s.temperature));
                    p.setPen(QColor{255, 110, 100});
                    p.drawText(QPointF{-r - 2.0 * unit - metrics.horizontalAdvance(text), -2.0 * unit}, text);
                }
                if (SurfaceStation::has(s.dewPoint)) {
                    const QString text = number(degrees(s.dewPoint));
                    p.setPen(QColor{90, 220, 110});
                    p.drawText(QPointF{-r - 2.0 * unit - metrics.horizontalAdvance(text), metrics.ascent() * 0.9}, text);
                }
            }
        };
        if (host.deferMarks()) {
            host.deferMark(s.lat, s.lon, mark);
        } else {
            painter.save();
            painter.translate(at);
            mark(painter, perPixel, 0.0);
            painter.restore();
        }
    }
}

MapHit StationLayer::pick(const QPointF& pixels, MapHost&) const {
    MapHit hit;
    hit.reach = 13.0;
    hit.priority = airports ? 20 : 10;
    for (const auto& d : drawn) {
        const double distance = std::hypot(d.at.x() - pixels.x(), d.at.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            const auto& s = (*stations)[d.index];
            hit.text = SurfaceStationText::summary(s, style().units == 0);
            hit.open = [s] (Window * parent) {
                if (s.airport) {
                    new SurfaceHistoryViewer{parent, s};
                } else {
                    new TextViewerStatic{parent, SurfaceStationText::details(s).toStdString(), s.id + (s.name.empty() ? "" : "  " + s.name), 640, 520};
                }
            };
        }
    }
    return hit;
}

vector<MapLegendRow> StationLayer::legend() const {
    MapLegendRow row;
    if (airports) {
        row.title = "Airports:";
        if (style().color == 1) {
            for (const auto& [name, color] : {std::pair<const char *, QColor>{"<0 F", QColor{170, 110, 240}}, {"0-32", QColor{70, 110, 240}}, {"32-50", QColor{50, 190, 230}}, {"50-70", QColor{70, 200, 100}},
                                              {"70-85", QColor{240, 215, 50}}, {"85-95", QColor{255, 150, 30}}, {"95+", QColor{235, 60, 50}}}) {
                row.entries.push_back({MapLegendEntry::Circle, color, name});
            }
        } else {
            row.entries = {{MapLegendEntry::Circle, QColor{60, 200, 80}, "VFR"}, {MapLegendEntry::Circle, QColor{70, 140, 255}, "MVFR"}, {MapLegendEntry::Circle, QColor{240, 60, 60}, "IFR"},
                           {MapLegendEntry::Circle, QColor{235, 70, 220}, "LIFR"}, {MapLegendEntry::Circle, QColor{110, 110, 118}, "old"}};
        }
    } else {
        row.title = "Mesonets:";
        row.entries = {{MapLegendEntry::Square, QColor{110, 150, 215}, style().color == 1 ? "coloured by temperature" : "network station"}};
    }
    return {row};
}

QWidget * StationLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    const auto check = [&] (const char * label, bool value, const char * pref, bool Style::* member) {
        auto * c = new QCheckBox{label, widget};
        c->setChecked(value);
        const string key{pref};
        QObject::connect(c, &QCheckBox::toggled, [key, member, changed] (bool on) { style().*member = on; Utility::writePref(key, on ? "true" : "false"); changed(); });
        layout->addWidget(c);
    };
    check("Wind barbs (knots)", style().barbs, "MM_OBS_BARBS", &Style::barbs);
    check("Temperature (red) and dew point (green)", style().values, "MM_OBS_VALUES", &Style::values);
    const auto combo = [&] (const char * title, std::vector<const char *> entries, int current, const char * pref, int Style::* member) {
        layout->addWidget(new QLabel{title, widget});
        auto * c = UiStandards::combo(widget);
        for (const auto * e : entries) {
            c->addItem(e);
        }
        c->setCurrentIndex(current);
        const string key{pref};
        QObject::connect(c, &QComboBox::currentIndexChanged, [key, member, changed] (int index) { style().*member = index; Utility::writePrefInt(key, index); changed(); });
        layout->addWidget(c);
    };
    combo("Dots:", {"Flight category / network", "Temperature"}, style().color, "MM_OBS_COLOR", &Style::color);
    combo("Temperatures in:", {"Fahrenheit", "Celsius"}, style().units, "MM_OBS_UNITS", &Style::units);
    if (!airports) {
        combo("Networks:", {"All mesonet networks", "Without citizen stations (APRS)", "RAWS fire weather only", "HADS (hydrologic) only", "MesoWest partners only", "State and road networks only", "Citizen stations (APRS) only"},
              style().network, "MM_OBS_NETWORK", &Style::network);
    }
    return widget;
}
