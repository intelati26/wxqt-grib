// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/CfsPeriodLayer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QVBoxLayout>
#include "gfs/Cfs30Loader.h"
#include "mapkit/PlateCarreeImage.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "util/DisplayUnits.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

namespace {
    // a departure of temperature (degrees C): blues under the normal, warm colors over it, a half degree clear
    const std::vector<std::pair<double, QColor>>& temperatureStops() {
        static const std::vector<std::pair<double, QColor>> stops{{-5, QColor{"#2b4a9a"}}, {-3, QColor{"#3f74b8"}}, {-1.5, QColor{"#86b4dc"}}, {-0.5, QColor{"#d3e5f2"}}, {0.0, QColor{250, 250, 250}},
                                                                   {0.5, QColor{"#f6e0b8"}}, {1.5, QColor{"#f0b070"}}, {3, QColor{"#d9633a"}}, {5, QColor{"#a02a2a"}}};
        return stops;
    }
    // the same for the precipitation (mm/day): browns drier, greens wetter
    const std::vector<std::pair<double, QColor>>& precipitationStops() {
        static const std::vector<std::pair<double, QColor>> stops{{-2.4, QColor{"#7a4a1c"}}, {-1.2, QColor{"#b07a3a"}}, {-0.5, QColor{"#dcc08a"}}, {-0.15, QColor{"#efe3c4"}}, {0.0, QColor{250, 250, 250}},
                                                                   {0.15, QColor{"#c8e6c8"}}, {0.5, QColor{"#8cc99a"}}, {1.2, QColor{"#3fa06a"}}, {2.4, QColor{"#1b6e48"}}};
        return stops;
    }

    QRgb colorOf(const std::vector<std::pair<double, QColor>>& stops, double v) {
        if (v <= stops.front().first) {
            return stops.front().second.rgb();
        }
        for (size_t i = 1; i < stops.size(); i++) {
            if (v <= stops[i].first) {
                const double f = (v - stops[i - 1].first) / (stops[i].first - stops[i - 1].first);
                const auto a = stops[i - 1].second, b = stops[i].second;
                return qRgb(static_cast<int>(a.red() + (b.red() - a.red()) * f), static_cast<int>(a.green() + (b.green() - a.green()) * f), static_cast<int>(a.blue() + (b.blue() - a.blue()) * f));
            }
        }
        return stops.back().second.rgb();
    }

    QString number(double v) {
        return QString::number(v, 'f', std::abs(v) < 10 && std::abs(v - std::round(v)) > 0.01 ? 1 : 0);
    }
}

CfsPeriodLayer::CfsPeriodLayer() {
    days = Utility::readPrefInt("MM_CFSP_DAYS", 30);
    if (days != 7 && days != 14 && days != 30 && days != 60 && days != 90) {
        days = 30;
    }
    show = std::clamp(Utility::readPrefInt("MM_CFSP_SHOW", 0), 0, 2);
}

std::vector<std::pair<double, QColor>> CfsPeriodLayer::stops() const {
    std::vector<std::pair<double, QColor>> out;
    if (show == 2) {
        for (const auto& [value, color] : precipitationStops()) {
            out.emplace_back(DisplayUnits::value(value, "mm/day", DisplayUnits::Kind::Delta), color);
        }
    } else {
        for (const auto& [value, color] : temperatureStops()) {
            out.emplace_back(DisplayUnits::value(value, "deg C", DisplayUnits::Kind::Delta), color);
        }
    }
    return out;
}

string CfsPeriodLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && frames.find(days) == frames.end()) {
        return "reading the CFSv2 (160 fields, some 25 seconds the first time)...";
    }
    if (frames.find(days) == frames.end()) {
        return {};
    }
    return "CFSv2, the next " + std::to_string(days) + " days against its normal, the mean of its four newest runs";
}

void CfsPeriodLayer::optionChanged(MapHost& host) {
    if (changeRequested) {
        changeRequested = false;
        refresh(host);
    }
}

void CfsPeriodLayer::refresh(MapHost& host) {
    if (loading || frames.find(days) != frames.end()) {
        return;
    }
    load(host);
}

void CfsPeriodLayer::load(MapHost& host) {
    loading = true;
    struct Loaded {
        GfsGrid::Grid temperature, precipitation;
        string error;
        bool ok{false};
    };
    auto loaded = std::make_shared<Loaded>();
    const int length = days;
    host.background(
        [loaded, length] {
            string messageT, messageP;
            const bool t = Cfs30Loader::period(length, "t2m", loaded->temperature, messageT);
            const bool p = Cfs30Loader::period(length, "precip", loaded->precipitation, messageP);
            loaded->ok = t || p;
            loaded->error = !t ? messageT : messageP;
        },
        [this, &host, loaded, length, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            if (!loaded->ok) {
                error = loaded->error.empty() ? "no CFSv2 data could be read" : loaded->error;
                host.redraw();
                return;
            }
            error.clear();
            Frame f;
            f.temperature = std::move(loaded->temperature);
            f.precipitation = std::move(loaded->precipitation);
            for (auto& v : f.temperature.values) {
                v = static_cast<float>(DisplayUnits::value(v, "deg C", DisplayUnits::Kind::Delta));
            }
            GfsGrid::closeWrapGap(f.temperature);
            GfsGrid::closeWrapGap(f.precipitation);
            frames[length] = std::move(f);
            // the pictures are made for what is shown (the options change them)
            auto& frame = frames[length];
            if (!frame.precipitation.empty()) {
                std::vector<std::pair<double, QString>> wet, dry;
                for (const double level : {0.5, 1.0, 1.5, 2.0, 3.0, 4.0}) {
                    wet.emplace_back(level, number(level));
                    dry.emplace_back(-level, number(-level));
                }
                const auto smooth = GfsGrid::smoothed(frame.precipitation, 2);
                frame.wet = BandLines::make(smooth, wet);
                frame.dry = BandLines::make(smooth, dry);
            }
            frame.image = colored(show == 2 ? frame.precipitation : frame.temperature);
            host.redraw();
        });
}

QImage CfsPeriodLayer::colored(const GfsGrid::Grid& grid) const {
    if (grid.empty()) {
        return {};
    }
    int first = 0, last = grid.rows - 1;
    while (first < last && grid.lat0 - first * grid.step > 85.0) {
        first++;
    }
    while (last > first && grid.lat0 - last * grid.step < -85.0) {
        last--;
    }
    const int up = 3;   // the 1 degree cells smoothed (bicubic) onto a grid three times as fine
    QImage image{grid.columns * up, (last - first + 1) * up, QImage::Format_ARGB32_Premultiplied};
    const auto table = stops();
    for (int y = 0; y < image.height(); y++) {
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(y));
        const double lat = grid.lat0 - first * grid.step + grid.step / 2.0 - (y + 0.5) * grid.step / up;
        for (int x = 0; x < image.width(); x++) {
            const double lon = grid.lon0 - grid.step / 2.0 + (x + 0.5) * grid.step / up;
            const float v = grid.sampleCubic(lon, lat);
            out[x] = std::isnan(v) ? qRgba(0, 0, 0, 0) : (colorOf(table, v) | 0xff000000u);
        }
    }
    return image;
}

void CfsPeriodLayer::paint(QPainter& painter, MapHost& host) {
    const auto found = frames.find(days);
    if (found == frames.end() || found->second.image.isNull()) {
        return;
    }
    const auto& g = show == 2 ? found->second.precipitation : found->second.temperature;
    int first = 0;
    while (first < g.rows - 1 && g.lat0 - first * g.step > 85.0) {
        first++;
    }
    const double north = g.lat0 - first * g.step + g.step / 2.0, south = north - found->second.image.height() * g.step / 3.0;
    PlateCarreeImage::paint(painter, host, found->second.image, g.lon0 - g.step / 2.0, south, g.lon0 + g.columns * g.step - g.step / 2.0, north, opacity / 100.0, true);
    if (lines && show == 0) {
        BandLines::paint(painter, host, found->second.wet, QColor{20, 120, 60, 200}, 1.3);
        BandLines::paint(painter, host, found->second.dry, QColor{140, 80, 20, 200}, 1.3);
    }
}

MapHit CfsPeriodLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    const auto found = frames.find(days);
    if (found == frames.end()) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const double lonShown = lon > 180.0 ? lon - 360.0 : lon;
    const float t = found->second.temperature.empty() ? std::nanf("") : found->second.temperature.sample(lonShown, lat);
    const float p = found->second.precipitation.empty() ? std::nanf("") : found->second.precipitation.sample(lonShown, lat);
    if (std::isnan(t) && std::isnan(p)) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = QString{"CFSv2, the next %1 days against its normal"}.arg(days);
    if (!std::isnan(t)) {
        hit.text += QString{"\nTemperature: "} + (t > 0 ? "+" : "") + QString::number(t, 'f', 1) + " " + QString::fromStdString(DisplayUnits::label("deg C"));
    }
    if (!std::isnan(p)) {
        hit.text += QString{"\nPrecipitation: "} + (p > 0 ? "+" : "") + QString::number(DisplayUnits::value(p, "mm/day", DisplayUnits::Kind::Delta), 'f', 2) + " " + QString::fromStdString(DisplayUnits::label("mm/day"));
    }
    return hit;
}

vector<MapLegendRow> CfsPeriodLayer::legend() const {
    if (frames.find(days) == frames.end()) {
        return {};
    }
    MapLegendRow row;
    row.title = QString{show == 2 ? "Precipitation against the CFS normal, next %1 days (%2):" : "Temperature against the CFS normal, next %1 days (%2):"}.arg(days).arg(QString::fromStdString(DisplayUnits::label(show == 2 ? "mm/day" : "deg C")));
    for (const auto& [value, color] : stops()) {
        row.entries.push_back({MapLegendEntry::Square, color, (value > 0 ? "+" : "") + number(value)});
    }
    vector<MapLegendRow> rows{row};
    if (show == 0 && lines) {
        MapLegendRow second;
        second.title = "Lines: precipitation against the normal, mm/day:";
        second.entries.push_back({MapLegendEntry::Square, QColor{20, 120, 60}, "wetter"});
        second.entries.push_back({MapLegendEntry::Square, QColor{140, 80, 20}, "drier"});
        rows.push_back(second);
    }
    return rows;
}

QWidget * CfsPeriodLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"The next:", widget});
    auto * period = UiStandards::combo(widget);
    int at = 0, index = 0;
    for (const int d : {7, 14, 30, 60, 90}) {
        period->addItem(QString{"%1 days"}.arg(d), d);
        if (d == days) {
            index = at;
        }
        at++;
    }
    period->setCurrentIndex(index);
    layout->addWidget(period);
    layout->addWidget(new QLabel{"Show:", widget});
    auto * what = UiStandards::combo(widget);
    what->addItems({"Temperature (colors) with the precipitation (lines)", "Temperature", "Precipitation"});
    what->setCurrentIndex(show);
    layout->addWidget(what);
    auto * linesBox = new QCheckBox{"Precipitation lines, with their numbers", widget};
    linesBox->setChecked(lines);
    layout->addWidget(linesBox);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(period, &QComboBox::currentIndexChanged, [this, period, changed] (int) {
        days = period->currentData().toInt();
        Utility::writePrefInt("MM_CFSP_DAYS", days);
        changeRequested = true;
        changed();
    });
    QObject::connect(what, &QComboBox::currentIndexChanged, [this, changed] (int value) {
        show = value;
        Utility::writePrefInt("MM_CFSP_SHOW", value);
        const auto found = frames.find(days);
        if (found != frames.end()) {   // the colors of what is shown
            found->second.image = colored(show == 2 ? found->second.precipitation : found->second.temperature);
        }
        changed();
    });
    QObject::connect(linesBox, &QCheckBox::toggled, [this, changed] (bool on) { lines = on; changed(); });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
