// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/ModelChartLayer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"
#include "models/ProductPicker.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

namespace {
    std::vector<string> splitList(const string& text) {
        std::vector<string> out;
        for (size_t at = 0; at < text.size();) {
            auto end = text.find(',', at);
            end = end == string::npos ? text.size() : end;
            if (end > at) {
                out.push_back(text.substr(at, end - at));
            }
            at = end + 1;
        }
        return out;
    }
    string joinList(const std::vector<string>& list) {
        string out;
        for (const auto& item : list) {
            out += (out.empty() ? "" : ",") + item;
        }
        return out;
    }
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
    overlays = splitList(Utility::readPref("MM_MODELCHART_OVERLAYS", ""));
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
    return model + "|" + param + "|" + run + "|" + std::to_string(hour) + variantKey();
}

// what the chart is besides model, chart, run and hour: its extra lines, one member, deviations, the change since an older run
string ModelChartLayer::variantKey() const {
    string key;
    for (const auto& id : overlays) {
        key += "|+" + id;
    }
    return key + (member.empty() ? string{} : "|m" + member) + (sigma ? "|sd" : "") + (changeBack > 0 ? "|chg" + std::to_string(changeBack) : string{});
}

GfsRender::Variant ModelChartLayer::variantNow() const {
    GfsRender::Variant v;
    if (changeBack > 0) {
        v.kind = GfsRender::Variant::Kind::Change;
        v.hoursBack = changeBack;
    }
    v.member = member;
    v.standardDeviations = sigma;
    return v;
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
    const auto overlaysNow = overlays;
    auto variantBase = variantNow();
    const auto * session_ = &session;
    host.background(
        [result, problem, key, modelNow, paramNow, runNow, overlaysNow, variantBase, hour, w, s, e, n, width, session_] {
            GfsRender::Variant variant = variantBase;
            variant.bare = true;
            variant.width = width;
            auto rendered = std::make_shared<Rendered>();
            string message;
            const auto bytes = GfsRender::png(const_cast<GfsRender::Session&>(*session_), modelNow, paramNow, GfsChart::boxId(w, s, e, n), runNow, hour, overlaysNow, message, &rendered->probe, variant);
            if (bytes.isEmpty() || !rendered->image.loadFromData(bytes, "PNG")) {
                *problem = message.empty() ? "the chart could not be drawn" : message;
                return;
            }
            rendered->west = w;
            rendered->south = s;
            rendered->east = e;
            rendered->north = n;
            rendered->widthPixels = rendered->image.width();
            rendered->key = key;
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
    // on whole pixels: the copies of a whole-world chart meet at the date line, and an edge between pixels would let the dark map show through as a line
    const double px = host.view().unitsPerPixel();
    const auto snap = [px] (double v) { return std::round(v / px) * px; };
    const QRectF drawn = QRectF{a, b}.normalized();
    painter.drawImage(QRectF{QPointF{snap(drawn.left()), snap(drawn.top())}, QPointF{snap(drawn.right()), snap(drawn.bottom())}}, shownImage->image);
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
    if (product == nullptr || product->linesOnly || changeBack > 0 || sigma) {
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
    auto variant = variantNow();
    variant.width = 1400;
    const auto bytes = GfsRender::png(const_cast<GfsRender::Session&>(session), model, param, GfsChart::boxId(w, s, e, n), run, hourShown(), overlays, problem, nullptr, variant);
    if (bytes.isEmpty() || !out.loadFromData(bytes, "PNG")) {
        if (problem.empty()) {
            problem = "the chart could not be drawn";
        }
        return false;
    }
    return true;
}

// The same pickers as the Model Viewer's page: models in groups, charts in groups with a search box, favorites and the lines and barbs that can be ticked on, the member of an ensemble,
// deviations, the change since an older run
QWidget * ModelChartLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * modelButton = new QPushButton{widget};
    auto * chartButton = new QPushButton{widget};
    modelButton->setToolTip("The model of the chart: opens the list of models in groups (global, regional, convection allowing, ensembles ...), with a search box");
    chartButton->setToolTip("The chart: opens the list of the model's charts in groups, with a search box, favorites (the star) and lines and wind barbs to tick onto it");
    layout->addWidget(modelButton);
    layout->addWidget(chartButton);
    auto * changeBox = new QComboBox{widget};
    changeBox->addItems({"Plain chart", "Change since the run 6 h earlier", "Change since the run 12 h earlier", "Change since the run 24 h earlier"});
    changeBox->setToolTip("How the chart changed since an earlier run: the same valid time from the older run is subtracted (blue = lower now, red = higher now). Charts of lines only have none");
    layout->addWidget(changeBox);
    auto * memberBox = new QComboBox{widget};
    memberBox->setToolTip("An ensemble's chart for one member instead of the mean");
    layout->addWidget(memberBox);
    auto * sigmaBox = new QCheckBox{"In standard deviations from the normal", widget};
    layout->addWidget(sigmaBox);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);

    // what the buttons say and which of the extra boxes the chart has
    const auto refresh = [this, modelButton, chartButton, changeBox, memberBox, sigmaBox] {
        const auto * product = GfsChart::product(param, model);
        modelButton->setText(QString::fromStdString("Model: " + model + "  \xE2\x96\xBE"));
        chartButton->setText(QString::fromStdString("Chart: " + (product ? product->label : param) + (overlays.empty() ? "" : "  + " + std::to_string(overlays.size()) + " more") + "  \xE2\x96\xBE"));
        changeBox->blockSignals(true);
        changeBox->setCurrentIndex(changeBack == 0 ? 0 : changeBack == 6 ? 1 : changeBack == 12 ? 2 : 3);
        changeBox->setVisible(product != nullptr && !product->linesOnly);
        changeBox->blockSignals(false);
        std::vector<GfsChart::MemberChoice> choices;
        if (product != nullptr) {
            choices = GfsChart::memberChoices(*product, run);
        }
        if (choices.empty()) {
            member.clear();
        }
        memberBox->blockSignals(true);
        memberBox->clear();
        memberBox->addItem("The ensemble's mean (or its own chart)", "");
        for (const auto& c : choices) {
            memberBox->addItem(QString::fromStdString(c.label), QString::fromStdString(c.value));
        }
        memberBox->setCurrentIndex(std::max(0, memberBox->findData(QString::fromStdString(member))));
        memberBox->setVisible(!choices.empty());
        memberBox->blockSignals(false);
        const bool offered = product != nullptr && GfsChart::hasStandardDeviations(*product);
        if (!offered) {
            sigma = false;
        }
        sigmaBox->blockSignals(true);
        sigmaBox->setChecked(sigma);
        sigmaBox->setVisible(offered);
        sigmaBox->blockSignals(false);
    };
    refresh();
    const auto save = [this] {
        Utility::writePref("MM_MODELCHART_MODEL", model);
        Utility::writePref("MM_MODELCHART_PARAM", param);
        Utility::writePref("MM_MODELCHART_OVERLAYS", joinList(overlays));
    };
    QObject::connect(modelButton, &QPushButton::clicked, [this, widget, refresh, save, changed] {
        std::vector<ProductPicker::Entry> entries;
        for (const auto& def : GfsModels::all()) {
            if (!def.storm && GfsRender::drawsModel(def.id)) {
                const auto group = GfsModels::pickerGroup(def.id);
                entries.push_back({def.id, group == "SPC guidance and mesoanalysis" ? def.id + "  -  " + def.label : def.id, group});
            }
        }
        static const std::vector<string> order{"Global", "Regional", "Convection allowing", "Ensembles", "Waves and ocean", "Other"};
        std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
            return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
        });
        auto * picker = new ProductPicker{widget->window(), "NCEP", entries, model, {}, {}, {}};
        picker->setAttribute(Qt::WA_DeleteOnClose);
        picker->setWording("models", "Show this model");
        picker->resize(320, 520);
        picker->onPick = [this, refresh, save, changed] (const string& id) {
            if (id == model) {
                return;
            }
            model = id;
            overlays.clear();
            member.clear();
            sigma = false;
            changeBack = 0;
            // the first chart of the model (the one it opens with)
            for (const auto& product : GfsChart::products()) {
                if (product.source == model && !product.id.empty() && product.id.compare(0, 4, "gen~") != 0) {
                    param = product.id;
                    break;
                }
            }
            save();
            refresh();
            changed();
        };
        picker->show();
    });
    QObject::connect(chartButton, &QPushButton::clicked, [this, widget, refresh, save, changed] {
        std::vector<ProductPicker::Entry> entries;
        for (const auto& product : GfsChart::products()) {
            if (product.source == model && !product.id.empty() && product.id.compare(0, 4, "gen~") != 0) {
                entries.push_back({product.id, product.label, GfsChart::category(product)});
            }
        }
        const auto favorites = splitList(Utility::readPref("MODELFAV_" + model, ""));
        auto * picker = new ProductPicker{widget->window(), model, entries, param, favorites, GfsChart::overlayChoices(model), overlays};
        picker->setAttribute(Qt::WA_DeleteOnClose);
        const string forModel = model;
        picker->onPick = [this, forModel, refresh, save, changed] (const string& id) {
            if (forModel != model) {
                return;
            }
            param = id;
            member.clear();
            save();
            refresh();
            changed();
        };
        picker->onOverlays = [this, forModel, refresh, save, changed] (const std::vector<string>& ids) {
            if (forModel != model) {
                return;
            }
            overlays = ids;
            save();
            refresh();
            changed();
        };
        picker->onFavorites = [forModel] (const std::vector<string>& ids) {
            Utility::writePref("MODELFAV_" + forModel, joinList(ids));
        };
        picker->show();
    });
    QObject::connect(changeBox, &QComboBox::currentIndexChanged, [this, changed] (int i) {
        changeBack = i == 0 ? 0 : i == 1 ? 6 : i == 2 ? 12 : 24;
        changed();
    });
    QObject::connect(memberBox, &QComboBox::currentIndexChanged, [this, memberBox, changed] (int) {
        member = memberBox->currentData().toString().toStdString();
        changed();
    });
    QObject::connect(sigmaBox, &QCheckBox::toggled, [this, changed] (bool on) {
        sigma = on;
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) {
        opacity = value;
        Utility::writePrefInt("MM_MODELCHART_OPACITY", value);
        changed();
    });
    return widget;
}
