// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/CfsPeriodLayer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QDateTime>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QTimer>
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
    using Stops = std::vector<std::pair<double, QColor>>;

    // a departure of temperature (degrees C): blues under the normal, warm colors over it, a half degree clear
    const Stops& temperatureStops() {
        static const Stops stops{{-5, QColor{"#2b4a9a"}}, {-3, QColor{"#3f74b8"}}, {-1.5, QColor{"#86b4dc"}}, {-0.5, QColor{"#d3e5f2"}}, {0.0, QColor{250, 250, 250}},
                                 {0.5, QColor{"#f6e0b8"}}, {1.5, QColor{"#f0b070"}}, {3, QColor{"#d9633a"}}, {5, QColor{"#a02a2a"}}};
        return stops;
    }
    // the same for the precipitation (mm/day): browns drier, greens wetter
    const Stops& precipitationStops() {
        static const Stops stops{{-2.4, QColor{"#7a4a1c"}}, {-1.2, QColor{"#b07a3a"}}, {-0.5, QColor{"#dcc08a"}}, {-0.15, QColor{"#efe3c4"}}, {0.0, QColor{250, 250, 250}},
                                 {0.15, QColor{"#c8e6c8"}}, {0.5, QColor{"#8cc99a"}}, {1.2, QColor{"#3fa06a"}}, {2.4, QColor{"#1b6e48"}}};
        return stops;
    }
    // the sea level pressure against the normal (hPa): purples lower (stormier), oranges higher
    const Stops& pressureDepartureStops() {
        static const Stops stops{{-6, QColor{"#4b2a8a"}}, {-3, QColor{"#7e63b8"}}, {-1, QColor{"#cdbfe6"}}, {0.0, QColor{250, 250, 250}}, {1, QColor{"#f3d9b0"}}, {3, QColor{"#e0a05a"}}, {6, QColor{"#b8622a"}}};
        return stops;
    }
    // the values themselves: a mean temperature (degrees C), a mean precipitation rate (mm/day), a mean sea level pressure (hPa)
    const Stops& temperatureValueStops() {
        static const Stops stops{{-30, QColor{"#5b2c83"}}, {-20, QColor{"#3a4fa5"}}, {-10, QColor{"#4f86c6"}}, {0, QColor{"#a9d3ec"}}, {5, QColor{"#d8ecc8"}}, {10, QColor{"#f6f1b0"}}, {20, QColor{"#f3b868"}},
                                 {30, QColor{"#d9633a"}}, {40, QColor{"#8f1d1d"}}};
        return stops;
    }
    const Stops& precipitationValueStops() {
        static const Stops stops{{0, QColor{"#f7f4ea"}}, {0.5, QColor{"#dcebc8"}}, {1, QColor{"#a9d7a3"}}, {2, QColor{"#5fb98a"}}, {4, QColor{"#2f93b5"}}, {7, QColor{"#2d5aa8"}}, {12, QColor{"#4b2a8a"}}};
        return stops;
    }
    const Stops& pressureValueStops() {
        static const Stops stops{{995, QColor{"#4b2a8a"}}, {1005, QColor{"#8e7cc3"}}, {1013, QColor{250, 250, 250}}, {1020, QColor{"#e9d3a8"}}, {1030, QColor{"#b8622a"}}};
        return stops;
    }

    // what each view shows: the key of the field that is colored, its stops (in the CFS's units: deg C, mm/day, hPa), whether it is a difference (for the unit's conversion), and the title of its legend
    struct Look {
        const char * key;
        const Stops& (*stops)();
        const char * unit;
        bool delta;
        const char * title;   // %1 the days, %2 the unit
    };
    const Look& lookOf(int show) {
        static const Look looks[] = {
            {"t2m", temperatureStops, "deg C", true, "Temperature against the CFS normal, next %1 days (%2):"},
            {"t2m", temperatureStops, "deg C", true, "Temperature against the CFS normal, next %1 days (%2):"},
            {"precip", precipitationStops, "mm/day", true, "Precipitation against the CFS normal, next %1 days (%2):"},
            {"t2m.v", temperatureValueStops, "deg C", false, "Mean temperature, next %1 days (%2):"},
            {"precip.v", precipitationValueStops, "mm/day", false, "Mean precipitation rate, next %1 days (%2):"},
            {"mslp", pressureDepartureStops, "hPa", true, "Sea level pressure against the CFS normal, next %1 days (%2):"},
            {"mslp.v", pressureValueStops, "hPa", false, "Mean sea level pressure, next %1 days (%2):"},
        };
        return looks[std::clamp(show, 0, 6)];
    }

    QRgb colorOf(const Stops& stops, double v) {
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

    // a key of the frame ("t2m.v") as the loader asks for it: the CFS's key and whether it is the mean itself
    std::pair<string, bool> loaderKey(const string& key) {
        const auto dot = key.find(".v");
        return dot == string::npos ? std::pair<string, bool>{key, false} : std::pair<string, bool>{key.substr(0, dot), true};
    }

    const char * nameOf(const string& key) {
        if (key == "t2m") return "Temperature against the normal";
        if (key == "precip") return "Precipitation against the normal";
        if (key == "mslp") return "Sea level pressure against the normal";
        if (key == "t2m.v") return "Mean temperature";
        if (key == "precip.v") return "Mean precipitation rate";
        return "Mean sea level pressure";
    }
}

CfsPeriodLayer::CfsPeriodLayer() {
    days = Utility::readPrefInt("MM_CFSP_DAYS", 30);
    if (days != 7 && days != 14 && days != 30 && days != 60 && days != 90) {
        days = 30;
    }
    show = std::clamp(Utility::readPrefInt("MM_CFSP_SHOW", 0), 0, 6);
}

// the fields the view on show is made of (only those are read)
std::vector<string> CfsPeriodLayer::needed() const {
    if (show == 0) {
        return {"t2m", "precip"};
    }
    return {lookOf(show).key};
}

bool CfsPeriodLayer::hasNeeded(const Frame& frame) const {
    const auto keys = needed();
    return std::all_of(keys.begin(), keys.end(), [&frame] (const string& key) { return frame.grids.find(key) != frame.grids.end(); });
}

string CfsPeriodLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    const auto found = frames.find(days);
    if (loading && (found == frames.end() || !hasNeeded(found->second))) {
        return "reading the CFSv2 (some 25 seconds the first time)...";
    }
    if (found == frames.end() || !hasNeeded(found->second)) {
        return {};
    }
    return "CFSv2, the next " + std::to_string(days) + " days" + (show == 3 || show == 4 || show == 6 ? "" : " against its normal") + ", the mean of its four newest runs";
}

void CfsPeriodLayer::optionChanged(MapHost& host) {
    if (changeRequested) {
        changeRequested = false;
        refresh(host);
    }
}

void CfsPeriodLayer::refresh(MapHost& host) {
    // read again when the frame on hand is hours old (the day's first result replaces yesterday's), or lacks a field of the view
    const auto found = frames.find(days);
    if (loading || (found != frames.end() && hasNeeded(found->second) && QDateTime::currentSecsSinceEpoch() - loadedAt < 6 * 3600)) {
        return;
    }
    load(host);
}

// The fields as they were last worked out (today's, or a day or two old) show at once; the fresh ones replace them when they are in (the first of the day takes some 25 seconds)
void CfsPeriodLayer::load(MapHost& host) {
    loading = true;
    struct Loaded {
        std::map<string, GfsGrid::Grid> grids;
        string error;
        bool ok{false};
        int age{-1};   // days: -1 none kept
    };
    const int length = days;
    const auto keys = needed();
    auto kept = std::make_shared<Loaded>();
    host.background(
        [kept, length, keys] {
            int age = 0;
            for (const auto& key : keys) {
                const auto [name, values] = loaderKey(key);
                GfsGrid::Grid grid;
                int ageOne = 0;
                if (Cfs30Loader::cached(length, name, grid, ageOne, values)) {
                    kept->grids[key] = std::move(grid);
                    age = std::max(age, ageOne);
                }
            }
            kept->ok = !kept->grids.empty();
            kept->age = kept->ok ? age : -1;
        },
        [this, &host, kept, length, keys, alive = alive] {
            if (!*alive) {
                return;
            }
            if (kept->ok) {
                install(std::move(kept->grids), length);
                host.redraw();
            }
            if (kept->ok && kept->age == 0 && frames.find(length) != frames.end() && hasNeeded(frames.at(length))) {   // today's: nothing fresher to be had
                loading = false;
                loadedAt = QDateTime::currentSecsSinceEpoch();
                return;
            }
            auto loaded = std::make_shared<Loaded>();
            host.background(
                [loaded, length, keys] {
                    for (const auto& key : keys) {
                        const auto [name, values] = loaderKey(key);
                        GfsGrid::Grid grid;
                        string message;
                        if (Cfs30Loader::period(length, name, grid, message, values)) {
                            loaded->grids[key] = std::move(grid);
                        } else if (loaded->error.empty()) {
                            loaded->error = message;
                        }
                    }
                    loaded->ok = !loaded->grids.empty();
                },
                [this, &host, loaded, length, alive = alive] {
                    if (!*alive) {
                        return;
                    }
                    loading = false;
                    if (!loaded->ok) {
                        if (frames.find(length) == frames.end() || !hasNeeded(frames.at(length))) {
                            error = loaded->error.empty() ? "no CFSv2 data could be read" : loaded->error;
                        }
                        host.redraw();
                        return;
                    }
                    error.clear();
                    install(std::move(loaded->grids), length);
                    loadedAt = QDateTime::currentSecsSinceEpoch();
                    host.redraw();
                });
        });
}

// the fields of a stretch (in the CFS's units) joined to the frame: the lines of those that have them, and the picture of the view on show
void CfsPeriodLayer::install(std::map<string, GfsGrid::Grid> grids, int length) {
    auto& f = frames[length];
    for (auto& [key, grid] : grids) {
        GfsGrid::closeWrapGap(grid);
        f.grids[key] = std::move(grid);
    }
    if (const auto p = f.grids.find("precip"); p != f.grids.end() && !p->second.empty()) {
        std::vector<std::pair<double, QString>> wet, dry;
        for (const double level : {0.5, 1.0, 1.5, 2.0, 3.0, 4.0}) {
            wet.emplace_back(level, number(level));   // (the lines are numbered in mm/day, as their legend says: in inches the levels round to nothing)
            dry.emplace_back(-level, number(-level));
        }
        const auto smooth = GfsGrid::smoothed(p->second, 2);
        f.wet = BandLines::make(smooth, wet);
        f.dry = BandLines::make(smooth, dry);
    }
    if (const auto p = f.grids.find("mslp.v"); p != f.grids.end() && !p->second.empty()) {   // isobars every 4 hPa
        std::vector<std::pair<double, QString>> levels;
        for (int hPa = 960; hPa <= 1060; hPa += 4) {
            levels.emplace_back(hPa, QString::number(hPa));
        }
        f.isobars = BandLines::make(GfsGrid::smoothed(p->second, 2), levels);
    }
    drawFrame(f);
}

void CfsPeriodLayer::drawFrame(Frame& f) const {
    const auto found = f.grids.find(lookOf(show).key);
    f.image = found == f.grids.end() ? QImage{} : colored(found->second);
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
    const auto& table = lookOf(show).stops();
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
    const auto grid = found->second.grids.find(lookOf(show).key);
    if (grid == found->second.grids.end()) {
        return;
    }
    const auto& g = grid->second;
    int first = 0;
    while (first < g.rows - 1 && g.lat0 - first * g.step > 85.0) {
        first++;
    }
    const double north = g.lat0 - first * g.step + g.step / 2.0, south = north - found->second.image.height() * g.step / 3.0;
    PlateCarreeImage::paint(painter, host, found->second.image, g.lon0 - g.step / 2.0, south, g.lon0 + g.columns * g.step - g.step / 2.0, north, opacity / 100.0, true);
    if (lines && show == 0) {
        QElapsedTimer timer;
        timer.start();
        BandLines::paint(painter, host, found->second.wet, QColor{20, 120, 60, 200}, 1.3);
        BandLines::paint(painter, host, found->second.dry, QColor{140, 80, 20, 200}, 1.3);
        if (qEnvironmentVariableIsSet("WXQT_CFSP_DEBUG")) {
            qWarning("CFSP lines painted in %lld ms", timer.elapsed());
        }
    }
    if (lines && show == 6) {
        BandLines::paint(painter, host, found->second.isobars, QColor{40, 30, 60, 210}, 1.2);
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
    QString text;
    for (const auto& key : needed()) {   // every field of the view on show, at the point
        const auto grid = found->second.grids.find(key);
        if (grid == found->second.grids.end() || grid->second.empty()) {
            continue;
        }
        const float v = grid->second.sample(lonShown, lat);
        if (std::isnan(v)) {
            continue;
        }
        const bool departure = key.find(".v") == string::npos;
        const string unit = key.compare(0, 4, "t2m") == 0 ? "deg C" : key.compare(0, 6, "precip") == 0 ? "mm/day" : "hPa";
        const double shown = DisplayUnits::value(v, unit, departure ? DisplayUnits::Kind::Delta : DisplayUnits::Kind::Absolute);
        const int decimals = unit == "hPa" ? 1 : unit == "mm/day" ? 2 : 1;
        text += QString{"\n"} + nameOf(key) + ": " + (departure && shown > 0 ? "+" : "") + QString::number(shown, 'f', decimals) + " " + QString::fromStdString(DisplayUnits::label(unit));
    }
    if (text.isEmpty()) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = QString{"CFSv2, the next %1 days"}.arg(days) + text;
    return hit;
}

vector<MapLegendRow> CfsPeriodLayer::legend() const {
    const auto found = frames.find(days);
    if (found == frames.end() || !hasNeeded(found->second)) {
        return {};
    }
    const auto& look = lookOf(show);
    MapLegendRow row;
    row.title = QString{look.title}.arg(days).arg(QString::fromStdString(DisplayUnits::label(look.unit)));
    for (const auto& [value, color] : look.stops()) {
        const double shown = DisplayUnits::value(value, look.unit, look.delta ? DisplayUnits::Kind::Delta : DisplayUnits::Kind::Absolute);
        row.entries.push_back({MapLegendEntry::Square, color, (look.delta && shown > 0 ? "+" : "") + number(shown)});
    }
    vector<MapLegendRow> rows{row};
    if (show == 0 && lines) {
        MapLegendRow second;
        second.title = "Lines: precipitation against the normal, mm/day:";
        second.entries.push_back({MapLegendEntry::Square, QColor{20, 120, 60}, "wetter"});
        second.entries.push_back({MapLegendEntry::Square, QColor{140, 80, 20}, "drier"});
        rows.push_back(second);
    }
    if (show == 6 && lines) {
        MapLegendRow second;
        second.title = "Lines: mean sea level pressure, every 4 hPa";
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
    what->addItems({"Temperature (colors) with the precipitation (lines)", "Temperature against the normal", "Precipitation against the normal", "Mean temperature", "Mean precipitation rate",
                    "Sea level pressure against the normal", "Mean sea level pressure, with isobars"});
    what->setCurrentIndex(show);
    layout->addWidget(what);
    auto * linesBox = new QCheckBox{"Lines (the precipitation, or the isobars), with their numbers", widget};
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
        if (found != frames.end()) {
            if (hasNeeded(found->second)) {   // the colors of what is shown
                drawFrame(found->second);
            } else {                           // a field not read yet: asked of the loader (the picture of the last view stays until it is in)
                found->second.image = QImage{};
                changeRequested = true;
            }
        } else {
            changeRequested = true;
        }
        changed();
    });
    QObject::connect(linesBox, &QCheckBox::toggled, [this, changed] (bool on) { lines = on; changed(); });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
