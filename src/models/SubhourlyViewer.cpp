// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SubhourlyViewer.h"
#include <algorithm>
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

    QString leadText(int minutes) {
        return QString{"+%1:%2"}.arg(minutes / 60).arg(minutes % 60, 2, 10, QChar{'0'});
    }
}

SubhourlyViewer::SubhourlyViewer(Window * parent, const std::string& modelId, const std::string& area0, const std::string& run, const std::vector<std::string>& charts)
    : Window{parent}
    , model{modelId}
    , sector{area0}
    , cancel{std::make_shared<std::atomic<bool>>(false)}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(model + " 15 minute surface fields");
    if (const auto * def = GfsModels::find(model); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, chartIds, chartNames);   // (all of them until the ones with fields in the subhourly files are known)
    }
    paneCharts = charts;
    for (const auto * id : {"sim_radar_comp", "2m_temp_10m_wnd", "10m_wnd_gust", "visibility"}) {
        if (paneCharts.size() < 4 && std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
            paneCharts.push_back(id);
        }
    }
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(new QLabel{QString::fromStdString(model) + " run", top});
    runEdit = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addSecs(-3 * 3600), top};
    runEdit->setDisplayFormat("yyyy-MM-dd HH 'Z'");
    runEdit->setTimeZone(QTimeZone::utc());
    runEdit->setCalendarPopup(true);
    runEdit->setToolTip("The run (UTC). The subhourly files stay on the server for a day or two");
    topLayout->addWidget(runEdit);
    newestButton = UiStandards::button(top, "Newest", "The newest run that has its subhourly files");
    topLayout->addWidget(newestButton);
    areaButton = UiStandards::button(top, "");
    areaButton->setToolTip("The part of the map drawn: the same areas as the Model Viewer's");
    topLayout->addWidget(areaButton);
    topLayout->addWidget(new QLabel{"from", top});
    fromLead = new QSpinBox{top};
    fromLead->setRange(15, 1080);
    fromLead->setSingleStep(15);
    fromLead->setValue(15);
    fromLead->setSuffix(" min");
    fromLead->setToolTip("The first time of the loop, in minutes after the run's start");
    topLayout->addWidget(fromLead);
    topLayout->addWidget(new QLabel{"to", top});
    toLead = new QSpinBox{top};
    toLead->setRange(15, 1080);
    toLead->setSingleStep(15);
    toLead->setValue(180);
    toLead->setSuffix(" min");
    toLead->setToolTip("The last time of the loop (the files go to 18 hours: 1080 minutes)");
    topLayout->addWidget(toLead);
    step = UiStandards::combo(top);
    for (const int m : {15, 30, 60}) {
        step->addItem(QString{"every %1 min"}.arg(m), m);
    }
    topLayout->addWidget(step);
    panes = UiStandards::combo(top);
    for (int n = 1; n <= 4; n++) {
        panes->addItem(n == 1 ? QString{"1 chart"} : QString{"%1 charts"}.arg(n), n);
    }
    topLayout->addWidget(panes);
    buildButton = UiStandards::button(top, "Draw the loop", "Draw each time of the range in the background; the loop plays as they arrive");
    buildButton->setEnabled(false);
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
        QObject::connect(tile->image, &ZoomImage::clicked, this, [this] (double fx, double fy) {
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
        tile->setMessage("Looking for the newest run...");
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
    QObject::connect(newestButton, &QPushButton::clicked, this, [this] { findNewestRun(); });
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
            timer.start(300);
            playButton->setText("Pause");
        }
    });
    QObject::connect(slider, &QSlider::valueChanged, this, [this] (int index) { showFrame(index); });
    QObject::connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    QObject::connect(&timer, &QTimer::timeout, this, [this] { advance(); });
    if (!run.empty()) {
        auto t = QDateTime::fromString(QString::fromStdString(run), "yyyyMMddHH");
        t.setTimeZone(QTimeZone::utc());
        runEdit->setDateTime(t);
        runId = run;
        listCharts();
    } else {
        findNewestRun();
    }
}

std::string SubhourlyViewer::chartLabel(const std::string& id) const {
    for (size_t i = 0; i < chartIds.size() && i < chartNames.size(); i++) {
        if (chartIds[i] == id) {
            return chartNames[i];
        }
    }
    return id;
}

void SubhourlyViewer::closeEventCustom() {
    closed = true;
    cancel->store(true);
    timer.stop();
}

// the newest run (counting back from the lag the model has) whose subhourly file of the first hour has the radar
void SubhourlyViewer::findNewestRun() {
    status->setText("Looking for the newest run with subhourly files...");
    newestButton->setEnabled(false);
    buildButton->setEnabled(false);
    const auto id = model;
    auto found = std::make_shared<std::string>();
    new FutureVoid{this, [id, found] {
                       const auto * def = GfsModels::find(id);
                       const int lag = def != nullptr ? def->source("").lagHours : 2;
                       auto base = QDateTime::currentDateTimeUtc().addSecs(-3600LL * lag);
                       base = QDateTime{base.date(), QTime{base.time().hour(), 0}, QTimeZone::utc()};
                       GfsRender::Session session;
                       for (int back = 0; back < 14; back++) {
                           const auto run = base.addSecs(-3600LL * back).toString("yyyyMMddHH").toStdString();
                           if (!GfsRender::availableMinutes(session, id, "sim_radar_comp", run, {60}).empty()) {
                               *found = run;
                               return;
                           }
                       }
                   },
                   [this, found] {
                       if (closed) {
                           return;
                       }
                       newestButton->setEnabled(true);
                       if (found->empty()) {
                           status->setText("No run with subhourly files was found in the last 14 hours: pick a run.");
                           for (auto * tile : tiles) {
                               tile->setMessage("No run with subhourly files was found.");
                           }
                           buildButton->setEnabled(true);
                           return;
                       }
                       auto t = QDateTime::fromString(QString::fromStdString(*found), "yyyyMMddHH");
                       t.setTimeZone(QTimeZone::utc());
                       runEdit->setDateTime(t);
                       runId = *found;
                       invalidate("");
                       listCharts();
                   }};
}

// the charts of the model that have their fields in the subhourly files of this run: the surface ones
void SubhourlyViewer::listCharts() {
    status->setText("Looking at which charts the subhourly files hold...");
    buildButton->setEnabled(false);
    const auto id = model;
    const auto run = runId;
    const auto ids = chartIds;
    const auto names = chartNames;
    auto usable = std::make_shared<std::pair<std::vector<std::string>, std::vector<std::string>>>();
    new FutureVoid{this, [id, run, ids, names, usable] {
                       GfsRender::Session session;
                       for (size_t i = 0; i < ids.size() && i < names.size(); i++) {
                           if (!GfsRender::availableMinutes(session, id, ids[i], run, {60}).empty()) {
                               usable->first.push_back(ids[i]);
                               usable->second.push_back(names[i]);
                           }
                       }
                   },
                   [this, usable] {
                       if (closed) {
                           return;
                       }
                       if (!usable->first.empty()) {
                           chartIds = usable->first;
                           chartNames = usable->second;
                       }
                       // a pane whose chart is not in the files gets the first one that is
                       for (size_t i = 0; i < paneCharts.size(); i++) {
                           if (std::find(chartIds.begin(), chartIds.end(), paneCharts[i]) == chartIds.end() && !chartIds.empty()) {
                               for (const auto& id : chartIds) {
                                   if (std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
                                       paneCharts[i] = id;
                                       break;
                                   }
                               }
                           }
                           tiles[i]->caption->setText(QString::fromStdString(chartLabel(paneCharts[i])));
                           tiles[i]->setMessage("Pick the times, then Draw the loop.");
                       }
                       buildButton->setEnabled(true);
                       status->setText(QString::number(chartIds.size()) + " charts have their fields in the subhourly files (the surface ones: no pressure levels, no hourly maxima or amounts).");
                   }};
}

void SubhourlyViewer::applyLayout() {
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

void SubhourlyViewer::refreshAreaButton() {
    areaButton->setText(QString::fromStdString(areaLabel(sector)) + "  \xE2\x96\xBE");
}

void SubhourlyViewer::pickChart(int pane) {
    if (chartPicker) {
        chartPicker->close();
    }
    std::vector<ProductPicker::Entry> entries;
    for (size_t i = 0; i < chartIds.size() && i < chartNames.size(); i++) {
        const auto * product = GfsChart::product(chartIds[i], model);
        entries.push_back({chartIds[i], chartNames[i], product ? GfsChart::category(*product) : std::string{"Other"}});
    }
    const auto key = "MODELFAV_" + model;
    chartPicker = new ProductPicker{this, model, entries, paneCharts[static_cast<size_t>(pane)], favoritesOf(key), {}, {}};
    chartPicker->setAttribute(Qt::WA_DeleteOnClose);
    chartPicker->setNote("Only the charts whose fields are in the subhourly files are listed.");
    chartPicker->onPick = [this, pane] (const std::string& id) {
        paneCharts[static_cast<size_t>(pane)] = id;
        tiles[static_cast<size_t>(pane)]->caption->setText(QString::fromStdString(chartLabel(id)));
        invalidate("A chart changed: Draw the loop again.");
    };
    chartPicker->onFavorites = [key] (const std::vector<std::string>& ids) { saveFavorites(key, ids); };
    chartPicker->show();
}

void SubhourlyViewer::pickArea() {
    if (areaPicker) {
        areaPicker->close();
    }
    const auto * def = GfsModels::find(model);
    std::vector<std::string> ids = def != nullptr && def->sectors == GfsModels::Sectors::Conus ? UtilityModelNcepInterface::sectorsNbm : UtilityModelNcepInterface::sectorsGfs;
    if (std::find(ids.begin(), ids.end(), sector) == ids.end()) {
        ids.insert(ids.begin(), sector);
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

void SubhourlyViewer::stop() {
    cancel->store(true);
    ++generation;
}

void SubhourlyViewer::invalidate(const QString& why) {
    if (frames.empty()) {
        if (!why.isEmpty()) {
            status->setText(why);
        }
        return;
    }
    stop();
    timer.stop();
    playButton->setText("Play");
    frames.clear();
    frameNames.clear();
    minutes.clear();
    redrawing.clear();
    shown = -1;
    slider->setEnabled(false);
    playButton->setEnabled(false);
    saveButton->setEnabled(false);
    if (!why.isEmpty()) {
        status->setText(why);
    }
}

void SubhourlyViewer::build() {
    stop();
    cancel = std::make_shared<std::atomic<bool>>(false);
    const int mine = generation;
    runId = runEdit->dateTime().toUTC().toString("yyyyMMddHH").toStdString();
    const int each = step->currentData().toInt();
    minutes.clear();
    for (int m = fromLead->value(); m <= toLead->value(); m += each) {
        minutes.push_back(m);
    }
    if (minutes.empty()) {
        status->setText("The range is empty: from is after to.");
        return;
    }
    frames.assign(minutes.size(), std::vector<Cell>{});
    frameNames.assign(minutes.size(), QString{});
    redrawing.clear();
    done = failed = 0;
    firstError.clear();
    shown = -1;
    timer.stop();
    playButton->setText("Play");
    slider->setRange(0, static_cast<int>(minutes.size()) - 1);
    slider->setEnabled(true);
    playButton->setEnabled(true);
    saveButton->setEnabled(true);
    status->setText(QString{"Drawing %1 times..."}.arg(minutes.size()));
    for (size_t index = 0; index < minutes.size(); index++) {
        renderStep(index, mine, false);
    }
}

void SubhourlyViewer::renderStep(size_t index, int mine, bool again) {
    const int count = panes->currentData().toInt();
    const std::vector<std::string> charts(paneCharts.begin(), paneCharts.begin() + count);
    const int lead = minutes[index];
    const auto flag = cancel;
    const auto id = model, area0 = sector, run = runId;
    auto result = std::make_shared<std::vector<Cell>>();
    auto error = std::make_shared<std::string>();
    if (again) {
        redrawing.push_back(index);
    }
    new FutureVoid{this, [result, error, charts, lead, flag, id, area0, run] {
                       if (flag->load()) {
                           return;
                       }
                       GfsRender::Session session;
                       std::vector<Cell> cells;
                       std::string problem;
                       for (const auto& chart : charts) {
                           GfsRender::Variant variant;
                           variant.minutes = lead;
                           variant.width = 900;
                           Cell cell;
                           cell.probe = std::make_shared<GfsChart::Probe>();
                           cell.bytes = GfsRender::png(session, id, chart, area0, run, (lead + 59) / 60, {}, problem, cell.probe.get(), variant);
                           if (cell.bytes.isEmpty()) {
                               break;
                           }
                           cells.push_back(std::move(cell));
                       }
                       if (cells.size() == charts.size()) {
                           *result = std::move(cells);
                       } else {
                           *error = problem;
                       }
                   },
                   [this, mine, index, result, error, again] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (again) {
                           redrawing.erase(std::remove(redrawing.begin(), redrawing.end(), index), redrawing.end());
                           if (!result->empty() && index < frames.size()) {
                               frames[index] = std::move(*result);
                               if (shown == static_cast<int>(index)) {
                                   showFrame(shown);
                               }
                           }
                           return;
                       }
                       ++done;
                       if (result->empty()) {
                           ++failed;
                           if (firstError.empty()) {
                               firstError = leadText(minutes[index]).toStdString() + ": " + *error;
                           }
                       } else {
                           frames[index] = std::move(*result);
                           auto valid = QDateTime::fromString(QString::fromStdString(runId), "yyyyMMddHH");
                           valid.setTimeZone(QTimeZone::utc());
                           valid = valid.addSecs(60LL * minutes[index]);
                           frameNames[index] = QString::fromStdString(runId.substr(8, 2)) + "Z run " + leadText(minutes[index]) + "   valid " + valid.toString("ddd dd HH:mm") + "Z";
                           if (shown < 0) {
                               showFrame(static_cast<int>(index));
                           }
                           trimProbes();
                       }
                       status->setText(QString{"%1 of %2 times drawn%3%4"}.arg(done).arg(frames.size())
                                           .arg(failed > 0 ? QString{", %1 could not be drawn"}.arg(failed) : QString{}).arg(failed > 0 && !firstError.empty() ? QString{" (first: %1)"}.arg(QString::fromStdString(firstError)) : QString{}));
                   }};
}

void SubhourlyViewer::trimProbes() {
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
    std::sort(held.begin(), held.end(), [around] (size_t a, size_t b) { return std::abs(static_cast<int>(a) - around) > std::abs(static_cast<int>(b) - around); });
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

void SubhourlyViewer::showFrame(int index) {
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
    bool lost = false;
    for (size_t p = 0; p < cells.size() && p < tiles.size(); p++) {
        tiles[p]->caption->setText(QString::fromStdString(chartLabel(paneCharts[p])) + "   " + frameNames[static_cast<size_t>(index)]);
        tiles[p]->setChart(cells[p].bytes, cells[p].probe, paneCharts[p]);
        lost = lost || !cells[p].probe;
    }
    setTitle(model + " 15 minute - " + frameNames[static_cast<size_t>(index)].toStdString());
    if (lost && std::find(redrawing.begin(), redrawing.end(), static_cast<size_t>(index)) == redrawing.end() && !closed) {
        renderStep(static_cast<size_t>(index), generation, true);
    }
}

void SubhourlyViewer::advance() {
    const int n = static_cast<int>(frames.size());
    for (int k = 1; k <= n; k++) {
        const int next = ((shown < 0 ? -1 : shown) + k) % n;
        if (!frames[static_cast<size_t>(next)].empty()) {
            showFrame(next);
            return;
        }
    }
}

QByteArray SubhourlyViewer::composite(size_t index) const {
    const auto& cells = frames[index];
    if (cells.size() == 1) {
        return cells.front().bytes;
    }
    const int columns = cells.size() == 4 ? 2 : static_cast<int>(cells.size());
    const int rows = (static_cast<int>(cells.size()) + columns - 1) / columns;
    std::vector<QImage> images;
    const int cellWidth = 900;
    int cellHeight = 100;
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

void SubhourlyViewer::save() {
    std::vector<QByteArray> ready;
    for (size_t i = 0; i < frames.size(); i++) {
        if (!frames[i].empty()) {
            ready.push_back(composite(i));
        }
    }
    if (ready.empty()) {
        return;
    }
    const auto name = QString::fromStdString(model + "_15min_" + runId);
    const QByteArray still = shown >= 0 && !frames[static_cast<size_t>(shown)].empty() ? composite(static_cast<size_t>(shown)) : ready.front();
    UtilityAnimationExport::saveWithDialog(this, ready.size() >= 2 ? ready : std::vector<QByteArray>{}, 300, still, name, QByteArray{}, false, name);
}
