// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ClimateGridLayer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QCheckBox>
#include <QComboBox>
#include <QPainterPath>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include "mapkit/LoadingTicker.h"
#include "mapkit/PlateCarreeImage.h"
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    struct PeriodChoice {
        const char * label;
        int days;   // > 0: the last this many days; 0: the month so far; -1: last month
    };
    const PeriodChoice periods[] = {{"Last 7 days", 7}, {"Last 14 days", 14}, {"Last 30 days", 30}, {"Last 60 days", 60}, {"Last 90 days", 90}, {"This month so far", 0}, {"Last month", -1}};
    const int periodCount = static_cast<int>(sizeof(periods) / sizeof(periods[0]));
}

void ClimateGridLayer::configure(const string& product, int periodIndex, int opacityPercent, bool linesOn, bool farOn) {
    if (AcisClimate::product(product) != nullptr) {
        productId = product;
    }
    periodChoice = std::clamp(periodIndex, 0, periodCount - 1);
    opacity = std::clamp(opacityPercent, 5, 100);
    numbers = linesOn;
    showFar = farOn;
    parts.clear();
}

int ClimateGridLayer::periodCountOf() {
    return periodCount;
}

string ClimateGridLayer::periodLabel(int index) {
    return periods[std::clamp(index, 0, periodCount - 1)].label;
}

ClimateGridLayer::ClimateGridLayer() {
    showFar = Utility::readPref("MM_ACIS_FAR", "true").compare(0, 1, "t") == 0;
    productId = Utility::readPref("MM_ACIS_PRODUCT", "avgt-dep");
    if (AcisClimate::product(productId) == nullptr) {
        productId = "avgt-dep";
    }
    periodChoice = std::clamp(Utility::readPrefInt("MM_ACIS_PERIOD", 1), 0, periodCount - 1);
    opacity = std::clamp(Utility::readPrefInt("MM_ACIS_OPACITY", 85), 20, 100);
    detail = std::clamp(Utility::readPrefInt("MM_ACIS_DETAIL", 1), 0, 2);
}

// the days of the period: ending yesterday (or the day before the time asked for: the day itself is not over)
void ClimateGridLayer::period(QDate& from, QDate& to) const {
    const QDate end = (asOf.isValid() ? asOf.date() : QDate::currentDate()).addDays(-1);
    const auto& choice = periods[periodChoice];
    if (choice.days > 0) {
        to = end;
        from = end.addDays(-(choice.days - 1));
    } else if (choice.days == 0) {
        to = end;
        from = QDate{end.year(), end.month(), 1};
    } else {
        to = QDate{end.year(), end.month(), 1}.addDays(-1);
        from = QDate{to.year(), to.month(), 1};
    }
}

QString ClimateGridLayer::periodText() const {
    QDate from, to;
    period(from, to);
    return from.toString("MMM d") + (from == to ? QString{} : " to " + to.toString("MMM d")) + ", " + to.toString("yyyy");
}

void ClimateGridLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    refresh(host);
}

void ClimateGridLayer::refresh(MapHost& host) {
    if (loading) {
        again = true;
        return;
    }
    const auto * product = AcisClimate::product(productId);
    if (product == nullptr) {
        return;
    }
    loading = true;
    again = false;
    QDate from, to;
    period(from, to);
    struct Loaded {
        std::vector<Part> parts;
        std::string error;
    };
    auto result = std::make_shared<Loaded>();
    const int level = detail;
    LoadingTicker::start(host, alive, [this] { return loading; }, [] { return "ACIS: " + AcisClimate::progressText(); });
    host.background(
        [product, from, to, result, level] {
            for (const auto& region : AcisClimate::regions()) {
                auto read = AcisClimate::read(*product, region, from, to);
                if (read.stations.empty()) {
                    if (result->error.empty()) {
                        result->error = read.error;
                    }
                    continue;
                }
                if (!read.error.empty() && result->error.empty()) {
                    result->error = read.error;
                }
                Part part;
                part.region = &region;
                std::vector<GridAnalysis::Point> points;
                points.reserve(read.stations.size());
                for (const auto& s : read.stations) {
                    points.push_back({s.lat, s.lon, s.value});
                }
                GridAnalysis::Spec spec;
                spec.west = region.west;
                spec.south = region.south;
                spec.east = region.east;
                spec.north = region.north;
                spec.step = region.step;
                spec.reachKm = region.reachKm;
                spec.fillGapsKm = region.nearestKm * 1.7;   // holes between far apart stations are filled; the outer edge stays
                spec.baseKm = level == 0 ? 0.0 : region.reachKm * (level == 1 ? 0.35 : 0.2);   // detailed and fine: a short reach, longer where the stations are far apart
                spec.nearestKm = region.nearestKm;
                part.field = GridAnalysis::analyze(points, spec);
                const auto picture = [product] (const GridAnalysis::Field& field) {
                    QImage image{field.columns, field.rows, QImage::Format_ARGB32_Premultiplied};
                    image.fill(Qt::transparent);
                    for (int row = 0; row < field.rows; row++) {
                        auto * line = reinterpret_cast<QRgb *>(image.scanLine(row));
                        for (int column = 0; column < field.columns; column++) {
                            const float v = field.values[static_cast<size_t>(row) * field.columns + column];
                            if (!std::isnan(v)) {
                                const QColor c = product->bands[static_cast<size_t>(AcisClimate::bandOf(*product, v))].color;
                                line[column] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), 255));
                            }
                        }
                    }
                    return image;
                };
                part.image = picture(part.field);
                {   // the same stations carried much farther (a wide reach, a place is left empty only far from every station): the lighter draw round the real map
                    auto wide = spec;
                    wide.reachKm = region.reachKm * 2.4;
                    wide.baseKm = 0.0;
                    wide.nearestKm = region.nearestKm * 7.0;
                    wide.fillGapsKm = wide.nearestKm * 1.7;
                    wide.west -= 9.0;   // past the coasts and the borders: a box wider than the region's, on a coarser step
                    wide.east += 9.0;
                    wide.south -= 6.0;
                    wide.north += 6.0;
                    wide.step = region.step * 2.0;
                    const auto farField = GridAnalysis::analyze(points, wide);
                    part.farSpec = wide;
                    auto far = picture(farField).convertToFormat(QImage::Format_ARGB32);
                    {   // the edge of the carried field fades out (a box blur of the alpha, a few cells wide), so it does not end in a hard line
                        const int w = far.width(), h = far.height(), radius = 5;
                        std::vector<float> alpha(static_cast<size_t>(w) * h), tmp(alpha.size());
                        for (int y = 0; y < h; y++) {
                            const auto * line = reinterpret_cast<const QRgb *>(far.constScanLine(y));
                            for (int x = 0; x < w; x++) {
                                alpha[static_cast<size_t>(y) * w + x] = qAlpha(line[x]) / 255.0f;
                            }
                        }
                        for (int pass = 0; pass < 2; pass++) {
                            for (int y = 0; y < h; y++) {   // along the row
                                for (int x = 0; x < w; x++) {
                                    float sum = 0;
                                    int n = 0;
                                    for (int k = -radius; k <= radius; k++) {
                                        const int xx = x + k;
                                        sum += (xx < 0 || xx >= w) ? 0.0f : alpha[static_cast<size_t>(y) * w + xx];
                                        n++;
                                    }
                                    tmp[static_cast<size_t>(y) * w + x] = sum / n;
                                }
                            }
                            for (int y = 0; y < h; y++) {   // down the column
                                for (int x = 0; x < w; x++) {
                                    float sum = 0;
                                    int n = 0;
                                    for (int k = -radius; k <= radius; k++) {
                                        const int yy = y + k;
                                        sum += (yy < 0 || yy >= h) ? 0.0f : tmp[static_cast<size_t>(yy) * w + x];
                                        n++;
                                    }
                                    alpha[static_cast<size_t>(y) * w + x] = sum / n;
                                }
                            }
                        }
                        for (int y = 0; y < h; y++) {
                            auto * line = reinterpret_cast<QRgb *>(far.scanLine(y));
                            for (int x = 0; x < w; x++) {
                                if (qAlpha(line[x]) == 0) {   // beyond the field: a colour of the nearest side would be better, but the edge cells carry it: leave empty cells empty
                                    continue;
                                }
                                const auto pale = [] (int c) { return static_cast<int>(c * 0.45 + 255 * 0.55); };   // toward white: the lighter draw reads as pastel on a dark map as well as a light one
                                line[x] = qRgba(pale(qRed(line[x])), pale(qGreen(line[x])), pale(qBlue(line[x])), static_cast<int>(std::clamp(alpha[static_cast<size_t>(y) * w + x], 0.0f, 1.0f) * 255.0f));
                            }
                        }
                    }
                    part.farImage = far.convertToFormat(QImage::Format_ARGB32_Premultiplied);
                }
                {   // the edges of the bands as lines, each with its value: the map reads without the legend
                    GfsGrid::Grid grid;
                    grid.columns = part.field.columns;
                    grid.rows = part.field.rows;
                    grid.lon0 = region.west;
                    grid.lat0 = region.north;
                    grid.step = region.step;
                    grid.values = part.field.values;
                    std::vector<std::pair<double, QString>> levels;
                    for (size_t b = 1; b < product->bands.size(); b++) {
                        levels.emplace_back(product->bands[b].from, product->bands[b].label.isEmpty() ? QString::number(product->bands[b].from, 'f', product->decimals) : product->bands[b].label);
                    }
                    part.contours = BandLines::make(grid, levels);
                }
                part.stations = std::move(read.stations);
                result->parts.push_back(std::move(part));
            }
        },
        [this, &host, result, product] {
            loading = false;
            if (result->parts.empty()) {
                error = result->error.empty() ? "no climate stations answered" : result->error;
            } else {
                error = result->error;   // a part that could not be read is said, the rest is drawn
                parts = std::move(result->parts);
                shown = product;
                shownPeriod = periodText();
            }
            host.timesChanged();   // the status line is written again (it showed the progress)
            host.redraw();
            if (again) {
                refresh(host);
            }
        });
}

void ClimateGridLayer::paint(QPainter& painter, MapHost& host) {
    if (showFar) {   // the lighter draw first: the real map covers it where there are stations
        for (const auto& part : parts) {
            if (!part.farImage.isNull()) {
                PlateCarreeImage::paint(painter, host, part.farImage, part.farSpec.west, part.farSpec.south, part.farSpec.east, part.farSpec.north, opacity * 0.9 / 100.0);
            }
        }
    }
    for (const auto& part : parts) {
        PlateCarreeImage::paint(painter, host, part.image, part.field.spec.west, part.field.spec.south, part.field.spec.east, part.field.spec.north, opacity / 100.0);
    }
    if (!numbers) {
        return;
    }
    for (const auto& part : parts) {
        BandLines::paint(painter, host, part.contours);
    }
}

MapHit ClimateGridLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    if (parts.empty() || shown == nullptr) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    for (const auto& part : parts) {
        const float v = part.field.at(lat, lon);
        if (std::isnan(v)) {
            continue;
        }
        const AcisClimate::Station * nearest = nullptr;
        double best = 1e9;
        for (const auto& s : part.stations) {
            const double dy = (s.lat - lat) * 111.2, dx = (s.lon - lon) * 111.2 * std::cos(lat * std::numbers::pi / 180.0);
            const double d = std::hypot(dx, dy);
            if (d < best) {
                best = d;
                nearest = &s;
            }
        }
        hit.distance = 0.0;
        hit.text = shown->label + "\n" + shownPeriod + ": " + (v > 0 && shown->unit != "%" ? "+" : "") + QString::number(v, 'f', shown->decimals) + " " + shown->unit;
        if (nearest != nullptr) {
            hit.text += "\nnearest station: " + QString::fromStdString(nearest->name) + " " + QString::fromStdString(nearest->state) + ", " + QString::number(nearest->value, 'f', shown->decimals) + " " + shown->unit + ", " + QString::number(best, 'f', 0) + " km";
        }
        return hit;
    }
    return hit;
}

vector<MapLegendRow> ClimateGridLayer::legend() const {
    if (parts.empty() || shown == nullptr) {
        return {};
    }
    MapLegendRow row;
    row.title = shown->label + ", " + shownPeriod + " (" + shown->unit + "):";
    for (const auto& band : shown->bands) {
        row.entries.push_back({MapLegendEntry::Square, band.color, band.label});
    }
    return {row};
}

string ClimateGridLayer::summary() const {
    if (loading && parts.empty()) {
        return "ACIS: " + AcisClimate::progressText();
    }
    if (parts.empty()) {
        return error;
    }
    size_t stations = 0;
    for (const auto& part : parts) {
        stations += part.stations.size();
    }
    return (shown ? shown->label.toStdString() : string{}) + ", " + shownPeriod.toStdString() + ": " + std::to_string(stations) + " stations" + (error.empty() ? string{} : " (" + error + ")");
}

QWidget * ClimateGridLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * numbersBox = new QCheckBox{"Lines between the bands, with their numbers", widget};
    numbersBox->setChecked(numbers);
    QObject::connect(numbersBox, &QCheckBox::toggled, [this, changed] (bool on) { numbers = on; changed(); });
    layout->addWidget(numbersBox);
    auto * farBox = new QCheckBox{"A lighter draw beyond the stations (sea, Canada, Mexico)", widget};
    farBox->setChecked(showFar);
    farBox->setToolTip("The same measure carried much farther from the stations, drawn light under the real map, so there is no white space round the area and the pattern can be read past the coast");
    QObject::connect(farBox, &QCheckBox::toggled, [this, changed] (bool on) { showFar = on; Utility::writePref("MM_ACIS_FAR", on ? "true" : "false"); changed(); });
    layout->addWidget(farBox);
    layout->addWidget(new QLabel{"Measure:", widget});
    auto * measure = new QComboBox{widget};
    int current = 0;
    for (const auto& p : AcisClimate::products()) {
        measure->addItem(p.label, QString::fromStdString(p.id));
        if (p.id == productId) {
            current = measure->count() - 1;
        }
    }
    measure->setCurrentIndex(current);
    layout->addWidget(measure);
    layout->addWidget(new QLabel{"Period (ending yesterday):", widget});
    auto * days = new QComboBox{widget};
    for (const auto& p : periods) {
        days->addItem(p.label);
    }
    days->setCurrentIndex(periodChoice);
    layout->addWidget(days);
    layout->addWidget(new QLabel{"Detail:", widget});
    auto * level = new QComboBox{widget};
    level->addItems({"Smooth (each station carries far)", "Detailed", "Fine (follows the stations closely)"});
    level->setCurrentIndex(detail);
    layout->addWidget(level);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    layout->addWidget(new QLabel{"The stations are read from ACIS in parts (the first time takes a minute) and kept for a few hours; a period that ended days ago is kept for good.", widget});
    QObject::connect(measure, &QComboBox::currentIndexChanged, [this, measure, changed] (int) {
        productId = measure->currentData().toString().toStdString();
        Utility::writePref("MM_ACIS_PRODUCT", productId);
        changeRequested = true;
        changed();
    });
    QObject::connect(days, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        periodChoice = std::clamp(index, 0, periodCount - 1);
        Utility::writePrefInt("MM_ACIS_PERIOD", periodChoice);
        changeRequested = true;
        changed();
    });
    QObject::connect(level, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        detail = std::clamp(index, 0, 2);
        Utility::writePrefInt("MM_ACIS_DETAIL", detail);
        changeRequested = true;
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) {
        opacity = value;
        Utility::writePrefInt("MM_ACIS_OPACITY", value);
        changed();
    });
    return widget;
}
