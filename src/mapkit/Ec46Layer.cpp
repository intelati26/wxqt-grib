// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/Ec46Layer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include "cfs/UtilityOpenMeteoSeasonal.h"
#include "mapkit/PlateCarreeImage.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "util/DisplayUnits.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

namespace {
    struct Stop {
        double value;
        QColor color;
    };
    using UtilityOpenMeteoSeasonal::Variable;

    // the fields of the map: the ones with a weekly mean and a departure
    std::vector<const Variable *> fields() {
        std::vector<const Variable *> out;
        for (const auto& v : UtilityOpenMeteoSeasonal::variables()) {
            if (!v.periodKey.empty() && !v.anomalyKey.empty()) {
                out.push_back(&v);
            }
        }
        return out;
    }

    const std::vector<Stop>& valueRamp(const string& key) {
        static const std::vector<Stop> temperature{{-40, QColor{90, 40, 150}}, {-25, QColor{50, 90, 200}}, {-10, QColor{80, 170, 230}}, {0, QColor{200, 235, 245}}, {10, QColor{170, 215, 120}},
                                                   {20, QColor{250, 230, 90}}, {30, QColor{240, 140, 50}}, {40, QColor{200, 40, 40}}};
        static const std::vector<Stop> rain{{0, QColor{250, 250, 245}}, {5, QColor{225, 215, 170}}, {15, QColor{170, 215, 150}}, {30, QColor{90, 190, 120}}, {60, QColor{50, 150, 200}}, {100, QColor{40, 80, 190}}, {160, QColor{110, 50, 160}}};
        static const std::vector<Stop> pressure{{980, QColor{110, 50, 160}}, {995, QColor{50, 90, 200}}, {1005, QColor{120, 190, 230}}, {1013, QColor{235, 235, 225}}, {1020, QColor{250, 210, 90}}, {1030, QColor{240, 130, 50}}, {1040, QColor{200, 40, 40}}};
        if (key == "precipitation_sum") return rain;
        if (key == "pressure_msl_mean") return pressure;
        return temperature;
    }

    QRgb colorOf(const std::vector<Stop>& stops, double v) {
        if (v <= stops.front().value) {
            return stops.front().color.rgb();
        }
        for (size_t i = 1; i < stops.size(); i++) {
            if (v <= stops[i].value) {
                const double f = (v - stops[i - 1].value) / (stops[i].value - stops[i - 1].value);
                const auto a = stops[i - 1].color, b = stops[i].color;
                return qRgb(static_cast<int>(a.red() + (b.red() - a.red()) * f), static_cast<int>(a.green() + (b.green() - a.green()) * f), static_cast<int>(a.blue() + (b.blue() - a.blue()) * f));
            }
        }
        return stops.back().color.rgb();
    }

    // the field's name without "(daily mean)" / "(daily total)": the map is of weeks
    std::string weekly(std::string label) {
        for (const char * part : {" (daily mean)", " (daily total)"}) {
            const auto at = label.find(part);
            if (at != std::string::npos) {
                label.erase(at, std::string{part}.size());
            }
        }
        return label;
    }

    // the spacing of the points (degrees) that keeps a view to about 450 of them
    double stepFor(double west, double south, double east, double north) {
        for (const double candidate : {0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0, 15.0}) {
            const double points = (std::floor((east - west) / candidate) + 1.0) * (std::floor((north - south) / candidate) + 1.0);
            if (points <= 450.0) {
                return candidate;
            }
        }
        return 20.0;
    }
}

Ec46Layer::Ec46Layer() {
    variableKey = Utility::readPref("MM_EC46_VARIABLE", "temperature_2m_mean");
    if (UtilityOpenMeteoSeasonal::find(variableKey) == nullptr || UtilityOpenMeteoSeasonal::find(variableKey)->anomalyKey.empty()) {
        variableKey = "temperature_2m_mean";
    }
    mode = std::clamp(Utility::readPrefInt("MM_EC46_MODE", 0), 0, 1);
}

long Ec46Layer::middleOf(const QDate& weekStart) const {
    return QDateTime{weekStart.addDays(3), QTime{12, 0}, QTimeZone::utc()}.toSecsSinceEpoch();
}

vector<long> Ec46Layer::times() const {
    vector<long> out;
    for (const auto& [week, frame] : frames) {
        (void) frame;
        out.push_back(middleOf(week));
    }
    return out;
}

QDate Ec46Layer::weekShown() const {
    if (frames.empty()) {
        return {};
    }
    // the first week is in progress: the map opens on the one after it (the first whole week)
    auto at = frames.begin();
    if (shown == 0) {
        return frames.size() > 1 ? std::next(at)->first : at->first;
    }
    for (auto it = frames.begin(); it != frames.end(); ++it) {
        if (middleOf(it->first) <= shown) {
            at = it;
        }
    }
    return at->first;
}

string Ec46Layer::timeText() const {
    const auto week = weekShown();
    if (!week.isValid()) {
        return {};
    }
    return "week of " + week.toString("d MMM yyyy").toStdString() + "  ECMWF EC46";
}

string Ec46Layer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && frames.empty()) {
        return "reading the EC46 forecast...";
    }
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    if (frames.empty() || variable == nullptr) {
        return {};
    }
    return "EC46 " + weekly(variable->label) + (mode == 0 ? string{" departure"} : string{}) + ", weekly ensemble mean, " + std::to_string(points) + " points at " + QString::number(step).toStdString() + " degrees" + (loading ? "  (loading)" : "");
}

void Ec46Layer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

void Ec46Layer::optionChanged(MapHost& host) {
    if (changeRequested) {
        changeRequested = false;
        frames.clear();
        refresh(host);
    }
}

void Ec46Layer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    if (frames.empty()) {
        load(host);
    }
}

// the points over the view (a map of the whole world is a coarse grid), the weekly means of each
void Ec46Layer::load(MapHost& host) {
    auto& view = host.view();
    if (view.map() == nullptr || view.pixelWidth() < 50) {
        return;
    }
    const auto [latTop, lonLeft] = view.toLatLonRaw(QPointF{0.0, 0.0});
    const auto [latBottom, lonRight] = view.toLatLonRaw(QPointF{static_cast<double>(view.pixelWidth()), static_cast<double>(view.pixelHeight())});
    double w = lonLeft, e = lonRight, n = latTop, s = latBottom;
    if (!(e > w && n > s)) {
        return;
    }
    const double shift = 360.0 * std::floor(((w + e) / 2.0 + 180.0) / 360.0);   // the copy of the world the middle of the view is in
    w = std::max(-180.0, w - shift);
    e = std::min(180.0, e - shift);
    n = std::min(80.0, n);
    s = std::max(-80.0, s);
    const double spacing = stepFor(w, s, e, n);
    // whole multiples of the spacing, so that views of the same area share the points (and the cache)
    w = std::floor(w / spacing) * spacing;
    e = std::ceil(e / spacing) * spacing;
    n = std::ceil(n / spacing) * spacing;
    s = std::floor(s / spacing) * spacing;
    loading = true;
    struct Loaded {
        UtilityOpenMeteoSeasonal::WeeklyGrid grid;
        string error;
        bool ok{false};
    };
    auto loaded = std::make_shared<Loaded>();
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    const string key = mode == 0 ? variable->anomalyKey : variable->periodKey;
    host.background(
        [loaded, key, w, s, e, n, spacing] { loaded->ok = UtilityOpenMeteoSeasonal::weeklyGrid(key, w, s, e, n, spacing, loaded->grid, loaded->error); },
        [this, &host, loaded, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            if (!loaded->ok) {
                error = loaded->error.empty() ? "no EC46 data could be read" : loaded->error;
                host.timesChanged();
                host.redraw();
                return;
            }
            error.clear();
            const auto& g = loaded->grid;
            west = g.west;
            north = g.north;
            step = g.step;
            south = g.north - (g.rows - 1) * g.step;
            east = g.west + (g.columns - 1) * g.step;
            points = g.columns * g.rows;
            frames.clear();
            for (size_t k = 0; k < g.weeks.size(); k++) {
                GfsGrid::Grid grid;
                grid.columns = g.columns;
                grid.rows = g.rows;
                grid.lon0 = g.west;
                grid.lat0 = g.north;
                grid.step = g.step;
                grid.values = g.values[k];
                for (auto& v : grid.values) {
                    if (!std::isnan(v)) {
                        v = static_cast<float>(inUnits(v));
                    }
                }
                Frame f;
                f.image = colored(grid);
                {
                    std::vector<std::pair<double, QString>> levels;
                    for (const auto& [value, color] : stops()) {
                        (void) color;
                        levels.emplace_back(value, (mode == 0 && value > 0 ? "+" : "") + QString::number(value, 'f', (std::abs(value) < 10 && std::abs(value - std::round(value)) > 0.01) ? 1 : 0));
                    }
                    f.lines = BandLines::make(GfsGrid::smoothed(grid, 1), levels);
                }
                f.grid = std::move(grid);
                frames[g.weeks[k]] = std::move(f);
            }
            host.timesChanged();
            host.redraw();
        });
}

double Ec46Layer::inUnits(double value) const {
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    if (variable == nullptr) {
        return value;
    }
    return DisplayUnits::value(value, variable->units, mode == 0 && variable->delta ? DisplayUnits::Kind::Delta : DisplayUnits::Kind::Absolute);
}

QString Ec46Layer::unitsShown() const {
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    return variable == nullptr ? QString{} : QString::fromStdString(DisplayUnits::label(variable->units));
}

std::vector<std::pair<double, QColor>> Ec46Layer::stops() const {
    std::vector<std::pair<double, QColor>> out;
    if (mode == 1) {
        for (const auto& stop : valueRamp(variableKey)) {
            out.emplace_back(inUnits(stop.value), stop.color);
        }
        return out;
    }
    const bool wet = variableKey == "precipitation_sum";
    const double half = wet ? 25.0 : variableKey == "pressure_msl_mean" ? 8.0 : 6.0;   // as wide as a weekly departure usually is
    const QColor low = wet ? QColor{150, 90, 30} : QColor{40, 80, 190}, lowMid = wet ? QColor{225, 195, 140} : QColor{150, 200, 235};
    const QColor highMid = wet ? QColor{150, 215, 160} : QColor{245, 180, 140}, high = wet ? QColor{30, 140, 70} : QColor{190, 40, 40};
    out = {{inUnits(-half), low}, {inUnits(-half / 2.0), lowMid}, {0.0, QColor{250, 250, 250}}, {inUnits(half / 2.0), highMid}, {inUnits(half), high}};
    return out;
}

QImage Ec46Layer::colored(const GfsGrid::Grid& grid) const {
    // the cells smoothed (bicubic) onto a grid several times as fine, so that the colours run on and no cell shows
    const int up = std::max(2, static_cast<int>(std::lround(24.0 / std::max(0.5, grid.step * 4.0))) * 2);
    QImage image{grid.columns * up, grid.rows * up, QImage::Format_ARGB32_Premultiplied};
    std::vector<Stop> table;
    for (const auto& [value, color] : stops()) {
        table.push_back({value, color});
    }
    for (int y = 0; y < image.height(); y++) {
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(y));
        const double lat = grid.lat0 + grid.step / 2.0 - (y + 0.5) * grid.step / up;
        for (int x = 0; x < image.width(); x++) {
            const double lon = grid.lon0 - grid.step / 2.0 + (x + 0.5) * grid.step / up;
            const float v = grid.sampleCubic(std::clamp(lon, grid.lon0, grid.lon0 + (grid.columns - 1) * grid.step), std::clamp(lat, grid.lat0 - (grid.rows - 1) * grid.step, grid.lat0));
            out[x] = std::isnan(v) ? qRgba(0, 0, 0, 0) : (colorOf(table, v) | 0xff000000u);
        }
    }
    return image;
}

void Ec46Layer::paint(QPainter& painter, MapHost& host) {
    const auto found = frames.find(weekShown());
    if (found == frames.end()) {
        return;
    }
    const auto& g = found->second.grid;
    PlateCarreeImage::paint(painter, host, found->second.image, g.lon0 - g.step / 2.0, g.lat0 - (g.rows - 1) * g.step - g.step / 2.0, g.lon0 + (g.columns - 1) * g.step + g.step / 2.0, g.lat0 + g.step / 2.0, opacity / 100.0, true);
    if (numbers) {
        BandLines::paint(painter, host, found->second.lines);
    }
}

MapHit Ec46Layer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    const auto found = frames.find(weekShown());
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    if (found == frames.end() || variable == nullptr) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const auto& g = found->second.grid;
    if (lon < g.lon0 - g.step / 2.0 || lon > g.lon0 + (g.columns - 0.5) * g.step || lat > g.lat0 + g.step / 2.0 || lat < g.lat0 - (g.rows - 0.5) * g.step) {
        return hit;
    }
    const float v = g.sample(std::clamp(lon, g.lon0, g.lon0 + (g.columns - 1) * g.step), std::clamp(lat, g.lat0 - (g.rows - 1) * g.step, g.lat0));
    if (std::isnan(v)) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = QString::fromStdString("EC46 " + weekly(variable->label) + ", " + timeText()) + "\n" + QString{mode == 0 && v > 0 ? "+" : ""} + QString::number(v, 'f', std::abs(v) < 10.0 ? 1 : 0) + " " + unitsShown() + (mode == 0 ? " (departure)" : "");
    return hit;
}

vector<MapLegendRow> Ec46Layer::legend() const {
    const auto * variable = UtilityOpenMeteoSeasonal::find(variableKey);
    if (variable == nullptr || frames.empty()) {
        return {};
    }
    MapLegendRow row;
    row.title = QString::fromStdString(weekly(variable->label)) + (mode == 0 ? ", weekly departure from the ECMWF climate (" : " (weekly, ") + unitsShown() + "):";
    for (const auto& [value, color] : stops()) {
        row.entries.push_back({MapLegendEntry::Square, color, (mode == 0 && value > 0 ? "+" : "") + QString::number(value, 'f', (std::abs(value) < 10 && std::abs(value - std::round(value)) > 0.01) ? 1 : 0)});
    }
    return {row};
}

QWidget * Ec46Layer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * numbersBox = new QCheckBox{"Lines between the colours, with their numbers", widget};
    numbersBox->setChecked(numbers);
    QObject::connect(numbersBox, &QCheckBox::toggled, [this, changed] (bool on) { numbers = on; changed(); });
    layout->addWidget(numbersBox);
    layout->addWidget(new QLabel{"Field:", widget});
    auto * field = UiStandards::combo(widget);
    int index = 0, at = 0;
    for (const auto * v : fields()) {
        field->addItem(QString::fromStdString(v->label), QString::fromStdString(v->key));
        if (v->key == variableKey) {
            index = at;
        }
        at++;
    }
    field->setCurrentIndex(index);
    layout->addWidget(field);
    layout->addWidget(new QLabel{"Show:", widget});
    auto * how = UiStandards::combo(widget);
    how->addItems({"Departure from the ECMWF climate", "The forecast values"});
    how->setCurrentIndex(mode);
    layout->addWidget(how);
    auto * reload = new QPushButton{"Load the view", widget};
    reload->setToolTip("Read the forecast again for the area on the screen (a coarser grid for a larger area): about 450 points of Open-Meteo's, kept six hours");
    layout->addWidget(reload);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(field, &QComboBox::currentIndexChanged, [this, field, changed] (int) {
        variableKey = field->currentData().toString().toStdString();
        Utility::writePref("MM_EC46_VARIABLE", variableKey);
        changeRequested = true;
        changed();
    });
    QObject::connect(how, &QComboBox::currentIndexChanged, [this, changed] (int value) {
        mode = value;
        Utility::writePrefInt("MM_EC46_MODE", value);
        changeRequested = true;
        changed();
    });
    QObject::connect(reload, &QPushButton::clicked, [this, changed] {
        changeRequested = true;
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
