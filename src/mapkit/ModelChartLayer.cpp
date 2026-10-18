// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ModelChartLayer.h"
#include <algorithm>
#include <cmath>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

namespace {
    QDateTime runTimeOf(const string& run) {
        return QDateTime{QDate::fromString(QString::fromStdString(run.substr(0, 8)), "yyyyMMdd"), QTime{std::stoi(run.substr(8, 2)), 0}, QTimeZone::utc()};
    }
}

ModelChartLayer::ModelChartLayer() {
    model = Utility::readPref("MM_MODELCHART_MODEL", "GFS");
    param = Utility::readPref("MM_MODELCHART_PARAM", "500_wnd_ht");
    if (GfsChart::product(param, model) == nullptr) {
        model = "GFS";
        param = "500_wnd_ht";
    }
    opacity = std::clamp(Utility::readPrefInt("MM_MODELCHART_OPACITY", 90), 20, 100);
}

string ModelChartLayer::runOf() const {
    return run;
}

// the forecast hours the run has: the model's own list, cut at the length of the run of that cycle
static std::vector<int> hoursOf(const string& model, const string& run) {
    std::vector<int> out;
    const auto * def = GfsModels::find(model);
    if (def == nullptr || run.size() < 10) {
        return out;
    }
    const int cycle = std::stoi(run.substr(8, 2));
    const int last = def->runLength ? def->runLength(cycle) : 100000;
    for (const auto& range : def->hours) {
        for (int h = range.from; h <= range.to; h += std::max(1, range.step)) {
            if (h <= last) {
                out.push_back(h);
            }
        }
    }
    return out;
}

vector<long> ModelChartLayer::times() const {
    vector<long> out;
    if (run.size() < 10) {
        return out;
    }
    const long start = static_cast<long>(runTimeOf(run).toSecsSinceEpoch());
    for (const int h : hours) {
        out.push_back(start + static_cast<long>(h) * 3600);
    }
    return out;
}

int ModelChartLayer::hourShown() const {
    if (hours.empty() || run.size() < 10) {
        return 0;
    }
    if (shown == 0) {
        return hours.front();
    }
    const long start = static_cast<long>(runTimeOf(run).toSecsSinceEpoch());
    int best = hours.front();
    for (const int h : hours) {
        if (start + static_cast<long>(h) * 3600 <= shown) {
            best = h;
        }
    }
    return best;
}

string ModelChartLayer::timeText() const {
    const auto * product = GfsChart::product(param, model);
    if (run.size() < 10 || product == nullptr) {
        return {};
    }
    const int hour = hourShown();
    return model + " " + product->label + "  " + runTimeOf(run).toString("yyyy-MM-dd HH").toStdString() + "Z run, F" + std::to_string(hour) + "  valid " +
           runTimeOf(run).addSecs(hour * 3600LL).toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z";
}

string ModelChartLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (run.empty()) {
        return finding ? "looking for the newest " + model + " run..." : string{};
    }
    return model + " " + run + (busy ? "  (drawing the chart for this view...)" : "");
}

void ModelChartLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    render(host);
}

void ModelChartLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    // the run at or before that time, on the model's own steps
    const auto * def = GfsModels::find(model);
    const int step = def == nullptr ? 6 : def->hourlyRuns ? 1 : std::max(1, def->cycleStep);
    run = QDateTime{asOf.date(), QTime{asOf.time().hour() / step * step, 0}, QTimeZone::utc()}.toString("yyyyMMddHH").toStdString();
    hours = hoursOf(model, run);
    shown = static_cast<long>(asOf.toSecsSinceEpoch());
    current.reset();
    previous.reset();
    error.clear();
    host.timesChanged();
    render(host);
}

void ModelChartLayer::refresh(MapHost& host) {
    if (finding || asOf.isValid()) {
        return;
    }
    finding = true;
    auto found = std::make_shared<std::pair<string, string>>();
    const string modelNow = model;
    host.background(
        [found, modelNow] {
            string cycle, date;
            if (GfsRender::latestCycle(modelNow, cycle, "", &date)) {
                *found = {date, cycle.substr(0, 2)};
            }
        },
        [this, &host, found, alive = alive, modelNow] {
            if (!*alive) {
                return;
            }
            finding = false;
            if (asOf.isValid()) {   // a time of the past was set while the newest run was being looked for: that stays
                return;
            }
            if (modelNow != model) {
                refresh(host);
                return;
            }
            if (found->first.empty()) {
                error = "no " + model + " run found";
            } else {
                error.clear();
                const string newest = found->first + found->second;
                if (newest != run) {
                    run = newest;
                    hours = hoursOf(model, run);
                    current.reset();
                    previous.reset();
                }
            }
            host.timesChanged();
            render(host);
        });
}

void ModelChartLayer::optionChanged(MapHost& host) {
    current.reset();
    previous.reset();
    run.clear();
    hours.clear();
    error.clear();
    if (asOf.isValid()) {
        setAsOf(asOf, host);
    } else {
        refresh(host);
    }
    host.timesChanged();
}

string ModelChartLayer::keyFor(int hour) const {
    return model + "|" + param + "|" + run + "|" + std::to_string(hour);
}

// the part of the world in view, as longitudes and latitudes (the map widget's corners)
bool ModelChartLayer::visibleBox(MapHost& host, double& west, double& south, double& east, double& north) const {
    auto& view = host.view();
    const auto * widget = view.map();
    if (widget == nullptr || widget->width() < 50) {
        return false;
    }
    const auto [latTop, lonLeft] = view.toLatLonRaw(QPointF{0.0, 0.0});
    const auto [latBottom, lonRight] = view.toLatLonRaw(QPointF{static_cast<double>(widget->width()), static_cast<double>(widget->height())});
    west = lonLeft;
    east = lonRight;
    north = latTop;
    south = latBottom;
    if (!(east > west && north > south)) {
        return false;
    }
    // The map may be scrolled round the world: the chart is drawn for the copy of the world the middle of the view is in, and when the view reaches over the edge of that copy (180 degrees) the box
    // is the whole world, which every copy then repeats
    const double middle = (west + east) / 2.0;
    const double shift = 360.0 * std::floor((middle + 180.0) / 360.0);
    west -= shift;
    east -= shift;
    if (west < -180.0 || east > 180.0) {
        west = -180.0;
        east = 180.0;
    }
    return true;
}

void ModelChartLayer::render(MapHost& host) {
    if (busy || run.size() < 10 || hours.empty()) {
        return;
    }
    double west, south, east, north;
    if (!visibleBox(host, west, south, east, north)) {
        return;
    }
    const int hour = hourShown();
    const string key = keyFor(hour);
    const int widthNow = host.view().map()->width();
    // is what is on hand this chart, covering the view, fine enough?
    if (current && current->key == key) {
        const double pixelsPerDegree = current->widthPixels / std::max(0.1, current->east - current->west), needed = widthNow / std::max(0.1, east - west);
        const bool covers = west >= current->west - 0.5 && east <= current->east + 0.5 && south >= current->south - 0.5 && north <= current->north + 0.5;
        if (covers && pixelsPerDegree >= needed * 0.7) {
            return;
        }
    }
    // the box drawn: the view and a quarter more each way (so that panning a little needs no new picture), whole world at most
    const double marginLon = (east - west) * 0.25, marginLat = (north - south) * 0.25;
    double w = std::max(-180.0, west - marginLon), e = std::min(180.0, east + marginLon), s = std::max(-85.0, south - marginLat), n = std::min(85.0, north + marginLat);
    if (e - w < 2.0) {
        w = std::max(-180.0, (w + e) / 2.0 - 1.0);
        e = std::min(180.0, w + 2.0);
    }
    if (n - s < 2.0) {
        s = std::max(-85.0, (s + n) / 2.0 - 1.0);
        n = std::min(85.0, s + 2.0);
    }
    const int width = std::clamp(static_cast<int>(widthNow * (e - w) / std::max(0.1, east - west) * 1.2), 700, 2800);
    busy = true;
    auto result = std::make_shared<std::shared_ptr<Rendered>>();
    auto problem = std::make_shared<string>();
    const string modelNow = model, paramNow = param, runNow = run;
    const auto * session_ = &session;
    host.background(
        [result, problem, modelNow, paramNow, runNow, hour, w, s, e, n, width, session_] {
            GfsRender::Variant variant;
            variant.bare = true;
            variant.width = width;
            auto rendered = std::make_shared<Rendered>();
            string message;
            const auto bytes = GfsRender::png(const_cast<GfsRender::Session&>(*session_), modelNow, paramNow, GfsChart::boxId(w, s, e, n), runNow, hour, {}, message, &rendered->probe, variant);
            if (bytes.isEmpty() || !rendered->image.loadFromData(bytes, "PNG")) {
                *problem = message.empty() ? "the chart could not be drawn" : message;
                return;
            }
            rendered->west = w;
            rendered->south = s;
            rendered->east = e;
            rendered->north = n;
            rendered->widthPixels = rendered->image.width();
            rendered->key = modelNow + "|" + paramNow + "|" + runNow + "|" + std::to_string(hour);
            *result = rendered;
        },
        [this, &host, result, problem, alive = alive] {
            if (!*alive) {
                return;
            }
            busy = false;
            if (*result) {
                previous = current;
                current = *result;
                error.clear();
            } else {
                error = *problem;
            }
            host.redraw();
            render(host);   // the view or the hour may have moved on while it was drawn
        });
    host.redraw();
}

void ModelChartLayer::paint(QPainter& painter, MapHost& host) {
    const auto shownImage = current && current->key == keyFor(hourShown()) ? current : (current ? current : previous);
    if (!shownImage || shownImage->image.isNull()) {
        render(host);
        return;
    }
    const auto t = host.view().transform();
    const QPointF a = t(shownImage->north, shownImage->west), b = t(shownImage->south, shownImage->east);
    painter.save();
    painter.setOpacity(opacity / 100.0);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.drawImage(QRectF{a, b}.normalized(), shownImage->image);
    painter.restore();
    render(host);   // (asks only when what is on hand no longer fits the view)
}

MapHit ModelChartLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    if (!current || !current->probe.valid()) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    double fx = 0.0, fy = 0.0;
    if (!current->probe.place(lon, lat, fx, fy)) {
        return hit;
    }
    const auto text = current->probe.read(fx, fy);
    if (text.isEmpty()) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = text;
    return hit;
}

vector<MapLegendRow> ModelChartLayer::legend() const {
    const auto * product = GfsChart::product(param, model);
    if (product == nullptr || product->linesOnly) {
        return {};
    }
    const bool us = UIPreferences::unitsF;
    const auto ramp = GfsChart::defaultRamp(*product, true);
    if (ramp.stops.size() < 2) {
        return {};
    }
    MapLegendRow row;
    row.title = QString::fromStdString(product->fillTitleFor ? product->fillTitleFor(hourShown()) : product->fillTitle);
    const auto unit = GfsChart::unitLabel(product->quantity, us);
    if (!unit.empty()) {
        row.title += QString::fromStdString(" (" + unit + ")");
    }
    row.title += ":";
    const size_t n = ramp.stops.size();
    const size_t step = std::max<size_t>(1, n / 9);
    for (size_t i = 0; i < n; i += step) {
        const auto& stop = ramp.stops[i];
        const double value = GfsChart::shownValue(product->quantity, stop.first, us);
        row.entries.push_back({MapLegendEntry::Square, stop.second.alpha() == 0 ? QColor{200, 200, 200} : stop.second, QString::number(value, 'f', std::abs(value) < 10 && value != std::round(value) ? 1 : 0)});
    }
    return {row};
}

// the Model Viewer's own picture of the chart for what is in view
bool ModelChartLayer::titled(MapHost& host, QImage& out, string& problem) const {
    double w, s, e, n;
    if (run.size() < 10 || !visibleBox(host, w, s, e, n)) {
        problem = "no chart is loaded yet";
        return false;
    }
    w = std::max(-180.0, w);
    e = std::min(180.0, e);
    s = std::max(-85.0, s);
    n = std::min(85.0, n);
    GfsRender::Variant variant;
    variant.width = 1400;
    const auto bytes = GfsRender::png(const_cast<GfsRender::Session&>(session), model, param, GfsChart::boxId(w, s, e, n), run, hourShown(), {}, problem, nullptr, variant);
    if (bytes.isEmpty() || !out.loadFromData(bytes, "PNG")) {
        if (problem.empty()) {
            problem = "the chart could not be drawn";
        }
        return false;
    }
    return true;
}

QWidget * ModelChartLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Model:", widget});
    auto * models = new QComboBox{widget};
    for (const auto& def : GfsModels::all()) {
        if (!def.storm && GfsRender::drawsModel(def.id)) {
            models->addItem(QString::fromStdString(def.id + "  " + def.label), QString::fromStdString(def.id));
        }
    }
    models->setCurrentIndex(std::max(0, models->findData(QString::fromStdString(model))));
    layout->addWidget(models);
    layout->addWidget(new QLabel{"Chart:", widget});
    auto * charts = new QComboBox{widget};
    layout->addWidget(charts);
    const auto fillCharts = [this, charts] {
        charts->blockSignals(true);
        charts->clear();
        std::vector<std::pair<QString, QString>> list;
        for (const auto& product : GfsChart::products()) {
            if (product.source == model && !product.id.empty() && product.id.compare(0, 4, "gen~") != 0) {
                list.emplace_back(QString::fromStdString(GfsChart::category(product) + ": " + product.label), QString::fromStdString(product.id));
            }
        }
        std::sort(list.begin(), list.end());
        for (const auto& [label, id] : list) {
            charts->addItem(label, id);
        }
        charts->setCurrentIndex(std::max(0, charts->findData(QString::fromStdString(param))));
        charts->blockSignals(false);
    };
    fillCharts();
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(models, &QComboBox::currentIndexChanged, [this, models, fillCharts, charts, changed] (int) {
        model = models->currentData().toString().toStdString();
        fillCharts();
        if (charts->count() > 0) {
            param = charts->currentData().toString().toStdString();
        }
        Utility::writePref("MM_MODELCHART_MODEL", model);
        Utility::writePref("MM_MODELCHART_PARAM", param);
        changed();
    });
    QObject::connect(charts, &QComboBox::currentIndexChanged, [this, charts, changed] (int) {
        param = charts->currentData().toString().toStdString();
        Utility::writePref("MM_MODELCHART_PARAM", param);
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) {
        opacity = value;
        Utility::writePrefInt("MM_MODELCHART_OPACITY", value);
        changed();
    });
    return widget;
}
