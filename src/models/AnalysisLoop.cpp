// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/AnalysisLoop.h"
#include <algorithm>
#include <limits>
#include <tuple>
#include <QBuffer>
#include <QHBoxLayout>
#include <QImage>
#include <QPainter>
#include <QTimeZone>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "hurricane/HafsIntensity.h"
#include "misc/ImageViewer.h"
#include "models/UtilityModelNcepInterface.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"

namespace {
    // label, the end of the models' names, the area drawn
    const std::vector<std::tuple<std::string, std::string, std::string>>& regions() {
        static const std::vector<std::tuple<std::string, std::string, std::string>> list{
            {"Contiguous US", "", "CONUS"}, {"Alaska", "-AK", "ALASKA"}, {"Hawaii", "-HI", "HAWAII-ISLANDS"}, {"Puerto Rico", "-PR", "PUERTO-RICO"}, {"Guam (RTMA only)", "-GU", "GUAM"}};
        return list;
    }

    // what an area is called in a list (as the Model Viewer says it)
    std::string areaLabel(const std::string& id) {
        double lat = 0, lon = 0, km = 0;
        std::string name;
        if (GfsChart::parseLocalId(id, lat, lon, km, name)) {
            return (name.empty() ? std::string{"Saved location"} : name) + ", " + std::to_string(static_cast<int>(km)) + " km square";
        }
        return Hafs::label(id);
    }

    std::vector<std::string> favoritesOf(const std::string& key) {
        std::vector<std::string> out;
        const auto stored = Utility::readPref(key, "");
        for (size_t at = 0; at < stored.size();) {
            auto end = stored.find(',', at);
            end = end == std::string::npos ? stored.size() : end;
            if (end > at) {
                out.push_back(stored.substr(at, end - at));
            }
            at = end + 1;
        }
        return out;
    }

    void saveFavorites(const std::string& key, const std::vector<std::string>& ids) {
        std::string joined;
        for (const auto& id : ids) {
            joined += (joined.empty() ? "" : ",") + id;
        }
        Utility::writePref(key, joined);
    }
}

AnalysisLoop::AnalysisLoop(Window * parent, const std::string& area0, const std::vector<std::string>& charts, const QDateTime& fromTime, const QDateTime& toTime, const std::string& region)
    : Window{parent}
    , sector{area0}
    , suffix{region}
    , cancel{std::make_shared<std::atomic<bool>>(false)}
    , urmaEnds{std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max())}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("NWS analysis");
    if (const auto * def = GfsModels::find("RTMA"); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, chartIds, chartNames);
    }
    static const std::vector<std::string> fill{"2m_temp_10m_wnd", "2m_dewp_10m_wnd", "10m_wnd_gust", "total_cloud_cover"};
    paneCharts = charts;
    for (const auto& id : fill) {
        if (paneCharts.size() < 4 && std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
            paneCharts.push_back(id);
        }
    }
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    const auto dateBox = [top] (const QDateTime& value, const QString& tip) {
        auto * edit = new QDateTimeEdit{value.toUTC(), top};
        edit->setDisplayFormat("yyyy-MM-dd HH 'Z'");
        edit->setTimeZone(QTimeZone::utc());
        edit->setCalendarPopup(true);
        edit->setToolTip(tip);
        return edit;
    };
    regionBox = UiStandards::combo(top);
    for (const auto& [label, end, defaultArea] : regions()) {
        regionBox->addItem(QString::fromStdString(label), QString::fromStdString(end));
        if (end == region) {
            regionBox->setCurrentIndex(regionBox->count() - 1);
        }
    }
    regionBox->setToolTip("The analyses of an area: each has its own pair of RTMA and URMA (the URMA used where it has the hour, the RTMA for the newest hours). Guam has an RTMA only");
    topLayout->addWidget(regionBox);
    areaButton = UiStandards::button(top, "");
    areaButton->setToolTip("The part of the map drawn: the same areas as the Model Viewer's");
    topLayout->addWidget(areaButton);
    topLayout->addWidget(new QLabel{"From", top});
    from = dateBox(fromTime, "The first hour of the loop (UTC). The URMA goes back to 29 January 2014, the RTMA to 19 March 2013");
    topLayout->addWidget(from);
    topLayout->addWidget(new QLabel{"To", top});
    to = dateBox(toTime, "The last hour of the loop (UTC). Hours the URMA has not come to yet (about 7 hours behind) are drawn from the RTMA");
    topLayout->addWidget(to);
    step = UiStandards::combo(top);
    for (const int hours : {1, 2, 3, 6, 12, 24}) {
        step->addItem(hours == 1 ? QString{"every hour"} : QString{"every %1 hours"}.arg(hours), hours);
    }
    step->setToolTip("The hours between the pictures: a long range is fewer, faster pictures at a larger step");
    topLayout->addWidget(step);
    panes = UiStandards::combo(top);
    for (int n = 1; n <= 4; n++) {
        panes->addItem(n == 1 ? QString{"1 chart"} : QString{"%1 charts"}.arg(n), n);
    }
    panes->setToolTip("One chart, or two to four side by side (the same hours, one loop)");
    topLayout->addWidget(panes);
    buildButton = UiStandards::button(top, "Draw the loop", "Draw each hour of the range in the background; the loop plays as the hours arrive");
    topLayout->addWidget(buildButton);
    topLayout->addStretch(1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    area = new QWidget{this};
    grid = new QGridLayout{area};
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    for (int i = 0; i < 4; i++) {
        auto * tile = new CompareTile{area};
        tiles[static_cast<size_t>(i)] = tile;
        QObject::connect(tile->change, &QPushButton::clicked, this, [this, i] { pickChart(i); });
        QObject::connect(tile->image, &ZoomImage::clicked, this, [this] (double fx, double fy) {   // a point marked in every pane
            for (auto * t : tiles) {
                t->image->setMarker(fx, fy);
            }
        });
        QObject::connect(tile->image, &ZoomImage::doubleClicked, this, [this, tile] {
            if (!tile->shownBytes.isEmpty()) {
                new ImageViewer{this, tile->shownBytes, tile->caption->text().toStdString()};
            }
        });
        tile->caption->setText(QString::fromStdString(chartLabel(paneCharts[static_cast<size_t>(i)])));
        tile->setMessage("Pick the range, then Draw the loop.");
    }
    box.addWidgetReal(area, 1, Qt::Alignment{});
    auto * bottom = new QWidget{this};
    auto * bottomLayout = new QHBoxLayout{bottom};
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    playButton = UiStandards::button(bottom, "Play");
    playButton->setEnabled(false);
    slider = new QSlider{Qt::Horizontal, bottom};
    slider->setEnabled(false);
    saveButton = UiStandards::button(bottom, "Save...", "Save the pictures drawn as a loop (GIF or video), the panes side by side");
    saveButton->setEnabled(false);
    bottomLayout->addWidget(playButton);
    bottomLayout->addWidget(slider, 1);
    bottomLayout->addWidget(saveButton);
    box.addWidgetReal(bottom, 0, Qt::Alignment{});
    status = new QLabel{"", this};
    UiStandards::muted(status);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1100, 850);
    panes->setCurrentIndex(std::clamp(static_cast<int>(charts.size()), 1, 4) - 1);
    refreshAreaButton();
    applyLayout();
    QObject::connect(areaButton, &QPushButton::clicked, this, [this] { pickArea(); });
    QObject::connect(regionBox, &QComboBox::currentIndexChanged, this, [this] (int index) {   // another area: its own pair of models, drawn over the area that fits them
        if (index >= 0 && index < static_cast<int>(regions().size())) {
            suffix = regionBox->currentData().toString().toStdString();
            sector = std::get<2>(regions()[static_cast<size_t>(index)]);
            refreshAreaButton();
            invalidate("The area changed: Draw the loop again.");
        }
    });
    QObject::connect(panes, &QComboBox::currentIndexChanged, this, [this] (int) {
        applyLayout();
        invalidate("The number of charts changed: Draw the loop again.");
    });
    QObject::connect(buildButton, &QPushButton::clicked, this, [this] { build(); });
    QObject::connect(playButton, &QPushButton::clicked, this, [this] {
        if (timer.isActive()) {
            timer.stop();
            playButton->setText("Play");
        } else {
            timer.start(450);
            playButton->setText("Pause");
        }
    });
    QObject::connect(slider, &QSlider::valueChanged, this, [this] (int index) { showFrame(index); });
    QObject::connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    QObject::connect(&timer, &QTimer::timeout, this, [this] { advance(); });
}

std::string AnalysisLoop::chartLabel(const std::string& id) const {
    for (size_t i = 0; i < chartIds.size() && i < chartNames.size(); i++) {
        if (chartIds[i] == id) {
            return chartNames[i];
        }
    }
    return id;
}

void AnalysisLoop::closeEventCustom() {
    closed = true;
    cancel->store(true);
    timer.stop();
}

// one pane, a row of them, or two by two (as the Model Viewer lays its tiles out)
void AnalysisLoop::applyLayout() {
    const int n = panes->currentData().toInt();
    for (auto * tile : tiles) {
        grid->removeWidget(tile);
    }
    const int columns = n == 4 ? 2 : n;
    for (int i = 0; i < 4; i++) {
        const bool used = i < n;
        tiles[static_cast<size_t>(i)]->setVisible(used);
        if (used) {
            grid->addWidget(tiles[static_cast<size_t>(i)], i / columns, i % columns);
        }
    }
    for (int c = 0; c < 2; c++) {
        grid->setColumnStretch(c, c < columns ? 1 : 0);
    }
    for (int r = 0; r < 2; r++) {
        grid->setRowStretch(r, r * columns < n ? 1 : 0);
    }
}

void AnalysisLoop::refreshAreaButton() {
    areaButton->setText(QString::fromStdString(areaLabel(sector)) + "  \xE2\x96\xBE");
}

void AnalysisLoop::pickChart(int pane) {
    if (chartPicker) {
        chartPicker->close();
    }
    const auto model = "RTMA" + suffix;
    std::vector<ProductPicker::Entry> entries;
    for (size_t i = 0; i < chartIds.size() && i < chartNames.size(); i++) {
        const auto * product = GfsChart::product(chartIds[i], model);
        entries.push_back({chartIds[i], chartNames[i], product ? GfsChart::category(*product) : std::string{"Other"}});
    }
    const auto key = "MODELFAV_" + model;
    chartPicker = new ProductPicker{this, model, entries, paneCharts[static_cast<size_t>(pane)], favoritesOf(key), {}, {}};
    chartPicker->setAttribute(Qt::WA_DeleteOnClose);
    chartPicker->onPick = [this, pane] (const std::string& id) {
        paneCharts[static_cast<size_t>(pane)] = id;
        tiles[static_cast<size_t>(pane)]->caption->setText(QString::fromStdString(chartLabel(id)));
        invalidate("A chart changed: Draw the loop again.");
    };
    chartPicker->onFavorites = [key] (const std::vector<std::string>& ids) { saveFavorites(key, ids); };
    chartPicker->show();
}

void AnalysisLoop::pickArea() {
    if (areaPicker) {
        areaPicker->close();
    }
    const auto model = "RTMA" + suffix;
    const auto * def = GfsModels::find(model);
    std::vector<std::string> ids = def != nullptr && def->sectors == GfsModels::Sectors::Conus ? UtilityModelNcepInterface::sectorsNbm : UtilityModelNcepInterface::sectorsGfs;
    if (std::find(ids.begin(), ids.end(), sector) == ids.end()) {
        ids.insert(ids.begin(), sector);   // (a saved place or an area of its own)
    }
    std::vector<ProductPicker::Entry> entries;
    for (const auto& id : ids) {
        entries.push_back({id, areaLabel(id), ProductPicker::sectorGroup(id)});
    }
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        static const std::vector<std::string> order{"Around a saved location", "United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Storms in the model", "Other"};
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    const auto key = "SECTORFAV_" + model;
    areaPicker = new ProductPicker{this, model, entries, sector, favoritesOf(key), {}, {}};
    areaPicker->setWording("areas", "Show this area");
    areaPicker->setAttribute(Qt::WA_DeleteOnClose);
    areaPicker->resize(360, 560);
    areaPicker->onPick = [this] (const std::string& id) {
        sector = id;
        refreshAreaButton();
        invalidate("The area changed: Draw the loop again.");
    };
    areaPicker->onFavorites = [key] (const std::vector<std::string>& ids) { saveFavorites(key, ids); };
    areaPicker->show();
}

void AnalysisLoop::stop() {
    cancel->store(true);   // the hours not yet started are left
    ++generation;
}

void AnalysisLoop::invalidate(const QString& why) {
    if (frames.empty()) {
        status->setText(why);
        return;
    }
    stop();
    timer.stop();
    playButton->setText("Play");
    frames.clear();
    frameNames.clear();
    sources.clear();
    times.clear();
    redrawing.clear();
    shown = -1;
    slider->setEnabled(false);
    playButton->setEnabled(false);
    saveButton->setEnabled(false);
    status->setText(why);
}

void AnalysisLoop::build() {
    stop();
    cancel = std::make_shared<std::atomic<bool>>(false);
    urmaEnds = std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max());
    const int mine = generation;
    const auto start = from->dateTime().toUTC();
    const auto end = to->dateTime().toUTC();
    const int hourStep = step->currentData().toInt();
    times.clear();
    for (auto t = start; t <= end; t = t.addSecs(static_cast<qint64>(hourStep) * 3600)) {
        times.push_back(t.toString("yyyyMMddHH").toStdString());
    }
    if (times.empty()) {
        status->setText("The range is empty: From is after To.");
        return;
    }
    frames.assign(times.size(), std::vector<Cell>{});
    frameNames.assign(times.size(), QString{});
    sources.assign(times.size(), std::string{});
    redrawing.clear();
    done = urma = rtma = failed = 0;
    firstError.clear();
    shown = -1;
    timer.stop();
    playButton->setText("Play");
    slider->setRange(0, static_cast<int>(times.size()) - 1);
    slider->setEnabled(true);
    playButton->setEnabled(true);
    saveButton->setEnabled(true);
    status->setText(QString{"Drawing %1 hours..."}.arg(times.size()));
    for (size_t index = 0; index < times.size(); index++) {
        renderHour(index, mine, false);
    }
}

// One hour's panes (and the values under the pointer): from the URMA when it has the hour, else the RTMA. `again`: an hour already drawn, its values (let go of) wanted back, from the model it came from.
void AnalysisLoop::renderHour(size_t index, int mine, bool again) {
    const int count = panes->currentData().toInt();
    const std::vector<std::string> charts(paneCharts.begin(), paneCharts.begin() + count);
    const auto valid = times[index];
    const auto urmaModel = "URMA" + suffix, rtmaModel = "RTMA" + suffix;
    const bool hasUrma = GfsModels::find(urmaModel) != nullptr;   // Guam has none
    const std::string only = again ? sources[index] : std::string{};
    const auto flag = cancel;
    const auto ends = urmaEnds;
    const auto area0 = sector;
    auto result = std::make_shared<std::pair<std::vector<Cell>, std::string>>();
    auto error = std::make_shared<std::string>();
    if (again) {
        redrawing.insert(index);
    }
    new FutureVoid{this, [result, error, valid, charts, flag, ends, area0, urmaModel, rtmaModel, hasUrma, only] {
                       if (flag->load()) {
                           return;
                       }
                       auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                       when.setTimeZone(QTimeZone::utc());
                       const long long seconds = when.toSecsSinceEpoch();
                       for (const auto& model : {urmaModel, rtmaModel}) {
                           if (!only.empty() && model != only) {
                               continue;
                           }
                           if (only.empty() && model == urmaModel && (!hasUrma || seconds >= ends->load())) {
                               continue;   // an earlier hour had no URMA: nor will this one
                           }
                           GfsRender::Session session;
                           std::vector<Cell> cells;
                           std::string problem;
                           for (const auto& chart : charts) {
                               GfsRender::Variant variant;
                               variant.validTime = valid;
                               variant.width = 900;
                               Cell cell;
                               cell.probe = std::make_shared<GfsChart::Probe>();
                               cell.bytes = GfsRender::png(session, model, chart, area0, "", 0, {}, problem, cell.probe.get(), variant);
                               if (cell.bytes.isEmpty()) {
                                   break;
                               }
                               cells.push_back(std::move(cell));
                           }
                           if (cells.size() == charts.size()) {
                               result->first = std::move(cells);
                               result->second = model;
                               return;
                           }
                           *error = problem;
                           if (model == urmaModel) {
                               auto current = ends->load();
                               while (seconds < current && !ends->compare_exchange_weak(current, seconds)) {
                               }
                           }
                       }
                   },
                   [this, mine, index, result, error, valid, again] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (again) {
                           redrawing.erase(index);
                           if (!result->first.empty() && index < frames.size()) {
                               frames[index] = std::move(result->first);
                               if (shown == static_cast<int>(index)) {
                                   showFrame(shown);
                               }
                           }
                           return;
                       }
                       ++done;
                       if (result->first.empty()) {
                           ++failed;
                           if (firstError.empty()) {
                               firstError = valid + ": " + *error;
                           }
                       } else {
                           frames[index] = std::move(result->first);
                           sources[index] = result->second;
                           (result->second.compare(0, 4, "URMA") == 0 ? urma : rtma)++;
                           auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                           frameNames[index] = when.toString("yyyy-MM-dd HH") + "Z";
                           if (shown < 0) {
                               showFrame(static_cast<int>(index));
                           }
                           trimProbes();
                       }
                       status->setText(QString{"%1 of %2 hours drawn: %3 from the URMA, %4 from the RTMA%5%6"}.arg(done).arg(frames.size()).arg(urma).arg(rtma)
                                           .arg(failed > 0 ? QString{", %1 could not be drawn"}.arg(failed) : QString{}).arg(failed > 0 && !firstError.empty() ? QString{" (first: %1)"}.arg(QString::fromStdString(firstError)) : QString{}));
                   }};
}

// the values under the pointer are kept for the hours near the one on view; the others let go (a long loop would hold gigabytes) and are drawn again when they are stepped to
void AnalysisLoop::trimProbes() {
    constexpr size_t limit = 400ull * 1024 * 1024;
    size_t total = 0;
    std::vector<size_t> held;
    for (size_t i = 0; i < frames.size(); i++) {
        for (const auto& cell : frames[i]) {
            if (cell.probe) {
                total += cell.probe->memory();
            }
        }
        if (!frames[i].empty() && frames[i].front().probe) {
            held.push_back(i);
        }
    }
    if (total <= limit) {
        return;
    }
    const int around = shown < 0 ? 0 : shown;
    std::sort(held.begin(), held.end(), [around] (size_t a, size_t b) { return std::abs(static_cast<int>(a) - around) > std::abs(static_cast<int>(b) - around); });   // the farthest first
    for (const auto i : held) {
        if (total <= limit * 3 / 4) {
            break;
        }
        for (auto& cell : frames[i]) {
            if (cell.probe) {
                total -= std::min(total, cell.probe->memory());
                cell.probe.reset();
            }
        }
    }
}

void AnalysisLoop::showFrame(int index) {
    if (index < 0 || index >= static_cast<int>(frames.size()) || frames[static_cast<size_t>(index)].empty()) {
        return;
    }
    shown = index;
    if (slider->value() != index) {
        slider->blockSignals(true);
        slider->setValue(index);
        slider->blockSignals(false);
    }
    const auto& cells = frames[static_cast<size_t>(index)];
    const auto source = QString::fromStdString(sources[static_cast<size_t>(index)]);
    bool lost = false;
    for (size_t p = 0; p < cells.size() && p < tiles.size(); p++) {
        const auto& chart = paneCharts[p];
        tiles[p]->caption->setText(QString::fromStdString(chartLabel(chart)) + "   " + source + "  " + frameNames[static_cast<size_t>(index)]);
        tiles[p]->setChart(cells[p].bytes, cells[p].probe, chart);
        lost = lost || !cells[p].probe;
    }
    setTitle("NWS analysis - " + frameNames[static_cast<size_t>(index)].toStdString() + " " + source.toStdString());
    if (lost && !redrawing.count(static_cast<size_t>(index)) && !closed) {
        renderHour(static_cast<size_t>(index), generation, true);   // the hover values come back in a moment
    }
}

// the next hour that is drawn, going round
void AnalysisLoop::advance() {
    const int n = static_cast<int>(frames.size());
    for (int k = 1; k <= n; k++) {
        const int next = ((shown < 0 ? -1 : shown) + k) % n;
        if (!frames[static_cast<size_t>(next)].empty()) {
            showFrame(next);
            return;
        }
    }
}

// the panes of one hour side by side in one picture
QByteArray AnalysisLoop::composite(size_t index) const {
    const auto& cells = frames[index];
    if (cells.size() == 1) {
        return cells.front().bytes;
    }
    const int columns = cells.size() == 4 ? 2 : static_cast<int>(cells.size());
    const int rows = (static_cast<int>(cells.size()) + columns - 1) / columns;
    std::vector<QImage> images;
    int cellWidth = 900, cellHeight = 100;
    for (const auto& cell : cells) {
        images.push_back(QImage::fromData(cell.bytes).scaledToWidth(cellWidth, Qt::SmoothTransformation));
        cellHeight = std::max(cellHeight, images.back().height());
    }
    QImage out(columns * cellWidth + (columns - 1) * 6, rows * cellHeight + (rows - 1) * 6, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter{&out};
    for (size_t i = 0; i < images.size(); i++) {
        painter.drawImage(static_cast<int>(i % static_cast<size_t>(columns)) * (cellWidth + 6), static_cast<int>(i / static_cast<size_t>(columns)) * (cellHeight + 6), images[i]);
    }
    painter.end();
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    out.save(&buffer, "PNG");
    return bytes;
}

void AnalysisLoop::save() {
    std::vector<QByteArray> ready;
    for (size_t i = 0; i < frames.size(); i++) {
        if (!frames[i].empty()) {
            ready.push_back(composite(i));
        }
    }
    if (ready.empty()) {
        return;
    }
    const auto name = QString{"NWS-analysis_%1_%2"}.arg(from->dateTime().toUTC().toString("yyyyMMddHH"), to->dateTime().toUTC().toString("yyyyMMddHH"));
    const QByteArray still = shown >= 0 && !frames[static_cast<size_t>(shown)].empty() ? composite(static_cast<size_t>(shown)) : ready.front();
    UtilityAnimationExport::saveWithDialog(this, ready.size() >= 2 ? ready : std::vector<QByteArray>{}, 450, still, name, QByteArray{}, false, name);
}
