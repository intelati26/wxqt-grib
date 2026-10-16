// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ClimateGridLayer.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
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

ClimateGridLayer::ClimateGridLayer() {
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
                part.image = QImage{part.field.columns, part.field.rows, QImage::Format_ARGB32_Premultiplied};
                part.image.fill(Qt::transparent);
                for (int row = 0; row < part.field.rows; row++) {
                    auto * line = reinterpret_cast<QRgb *>(part.image.scanLine(row));
                    for (int column = 0; column < part.field.columns; column++) {
                        const float v = part.field.values[static_cast<size_t>(row) * part.field.columns + column];
                        if (!std::isnan(v)) {
                            const QColor c = product->bands[static_cast<size_t>(AcisClimate::bandOf(*product, v))].color;
                            line[column] = qPremultiply(qRgba(c.red(), c.green(), c.blue(), 255));
                        }
                    }
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
    for (const auto& part : parts) {
        PlateCarreeImage::paint(painter, host, part.image, part.field.spec.west, part.field.spec.south, part.field.spec.east, part.field.spec.north, opacity / 100.0);
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
            const double dy = (s.lat - lat) * 111.2, dx = (s.lon - lon) * 111.2 * std::cos(lat * M_PI / 180.0);
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
