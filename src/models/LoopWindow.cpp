// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/LoopWindow.h"
#include <algorithm>
#include <cstdlib>
#include "gfs/GfsModels.h"
#include "models/ChartPickers.h"
#include "models/ChartResolution.h"
#include "objects/UtilityAnimationExport.h"
#include "ui/UiStandards.h"

LoopWindow::LoopWindow(Window * parent, const std::string& area0, const std::vector<std::string>& charts)
    : Window{parent}
    , sector{area0}
    , paneCharts{charts}
    , cancel{std::make_shared<std::atomic<bool>>(false)}
{
    setAttribute(Qt::WA_DeleteOnClose);
    auto * top = new QWidget{this};
    topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    areaButton = UiStandards::button(top, "");
    areaButton->setToolTip("The part of the map drawn: the same areas as the Model Viewer's");
    panes = UiStandards::combo(top);
    for (int n = 1; n <= PaneGrid::maxPanes; n++) {
        panes->addItem(n == 1 ? QString{"1 chart"} : QString{"%1 charts"}.arg(n), n);
    }
    panes->setToolTip("One chart, or two to four side by side (the same times, one loop)");
    resolution = ChartResolution::makeBox(top, [this] { invalidate("The chart size changed: Draw the loop again."); });
    buildButton = UiStandards::button(top, "Draw the loop", "Draw each time of the range in the background; the loop plays as they arrive");
    box.addWidgetReal(top, 0, Qt::Alignment{});
    grid = new PaneGrid{this};
    box.addWidgetReal(grid, 1, Qt::Alignment{});
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
    static const std::vector<std::string> fill{"2m_temp_10m_wnd", "2m_dewp_10m_wnd", "10m_wnd_gust", "total_cloud_cover"};
    for (const auto& id : fill) {   // (the window may say better ones once it knows the model's charts)
        if (paneCharts.size() < PaneGrid::maxPanes && std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
            paneCharts.push_back(id);
        }
    }
    panes->setCurrentIndex(std::clamp(static_cast<int>(charts.size()), 1, PaneGrid::maxPanes) - 1);
    grid->setCount(panes->currentData().toInt());
    grid->onChange([this] (int pane) { pickChart(pane); });
    QObject::connect(areaButton, &QPushButton::clicked, this, [this] { pickArea(); });
    QObject::connect(panes, &QComboBox::currentIndexChanged, this, [this] (int) {
        grid->setCount(panes->currentData().toInt());
        invalidate("The number of charts changed: Draw the loop again.");
    });
    QObject::connect(buildButton, &QPushButton::clicked, this, [this] { build(); });
    QObject::connect(playButton, &QPushButton::clicked, this, [this] {
        if (timer.isActive()) {
            timer.stop();
            playButton->setText("Play");
        } else {
            timer.start(delayMs());
            playButton->setText("Pause");
        }
    });
    QObject::connect(slider, &QSlider::valueChanged, this, [this] (int index) { showFrame(index); });
    QObject::connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    QObject::connect(&timer, &QTimer::timeout, this, [this] { advance(); });
}

std::string LoopWindow::chartLabel(const std::string& id) const {
    for (size_t i = 0; i < chartIds.size() && i < chartNames.size(); i++) {
        if (chartIds[i] == id) {
            return chartNames[i];
        }
    }
    return id;
}

void LoopWindow::setCharts(const std::vector<std::string>& ids, const std::vector<std::string>& names) {
    chartIds = ids;
    chartNames = names;
    for (size_t i = 0; i < paneCharts.size() && i < PaneGrid::maxPanes; i++) {
        grid->tile(i)->caption->setText(QString::fromStdString(chartLabel(paneCharts[i])));
    }
}

std::vector<std::string> LoopWindow::activeCharts() const {
    return std::vector<std::string>(paneCharts.begin(), paneCharts.begin() + paneCount());
}

void LoopWindow::closeEventCustom() {
    closed = true;
    cancel->store(true);
    timer.stop();
}

void LoopWindow::refreshAreaButton() {
    areaButton->setText(QString::fromStdString(ChartPickers::areaLabel(sector)) + "  \xE2\x96\xBE");
}

void LoopWindow::pickChart(int pane) {
    if (chartPicker) {
        chartPicker->close();
    }
    const auto model = pickerModel();
    chartPicker = ChartPickers::chartPicker(this, model, ChartPickers::chartEntries(model, chartIds, chartNames), paneCharts[static_cast<size_t>(pane)], [this, pane] (const std::string& id) {
        paneCharts[static_cast<size_t>(pane)] = id;
        grid->tile(static_cast<size_t>(pane))->caption->setText(QString::fromStdString(chartLabel(id)));
        invalidate("A chart changed: Draw the loop again.");
    });
    chartPicker->setAttribute(Qt::WA_DeleteOnClose);
    chartPicker->show();
}

void LoopWindow::pickArea() {
    if (areaPicker) {
        areaPicker->close();
    }
    auto ids = areaIds();
    if (std::find(ids.begin(), ids.end(), sector) == ids.end()) {
        ids.insert(ids.begin(), sector);   // (a saved place or an area of its own)
    }
    areaPicker = ChartPickers::areaPicker(this, pickerModel(), ChartPickers::areaEntries(ids), sector, [this] (const std::string& id) {
        sector = id;
        refreshAreaButton();
        invalidate("The area changed: Draw the loop again.");
    });
    areaPicker->setAttribute(Qt::WA_DeleteOnClose);
    areaPicker->show();
}

void LoopWindow::stop() {
    cancel->store(true);
    ++generation;
}

void LoopWindow::invalidate(const QString& why) {
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
    redrawing.clear();
    shown = -1;
    slider->setEnabled(false);
    playButton->setEnabled(false);
    saveButton->setEnabled(false);
    if (!why.isEmpty()) {
        status->setText(why);
    }
}

void LoopWindow::beginFrames(size_t count) {
    stop();
    cancel = std::make_shared<std::atomic<bool>>(false);
    frames.assign(count, std::vector<Cell>{});
    frameNames.assign(count, QString{});
    redrawing.clear();
    shown = -1;
    timer.stop();
    playButton->setText("Play");
    slider->setRange(0, static_cast<int>(count) - 1);
    slider->setEnabled(true);
    playButton->setEnabled(true);
    saveButton->setEnabled(true);
}

void LoopWindow::storeFrame(size_t index, std::vector<Cell>&& cells, const QString& name) {
    if (index >= frames.size()) {
        return;
    }
    frames[index] = std::move(cells);
    frameNames[index] = name;
    if (shown < 0) {
        showFrame(static_cast<int>(index));
    }
    trimProbes();
}

void LoopWindow::storeAgain(size_t index, std::vector<Cell>&& cells) {
    redrawing.erase(index);
    if (!cells.empty() && index < frames.size()) {
        frames[index] = std::move(cells);
        if (shown == static_cast<int>(index)) {
            showFrame(shown);
        }
    }
}

// the values under the pointer are kept for the frames near the one on view; the others let go (a long loop would hold gigabytes) and are drawn again when they are stepped to
void LoopWindow::trimProbes() {
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

void LoopWindow::showFrame(int index) {
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
    for (size_t p = 0; p < cells.size() && p < PaneGrid::maxPanes; p++) {
        grid->tile(p)->caption->setText(captionOf(static_cast<size_t>(index), p));
        grid->tile(p)->setChart(cells[p].bytes, cells[p].probe, paneCharts[p]);
        lost = lost || !cells[p].probe;
    }
    setTitle(titleOf(static_cast<size_t>(index)));
    if (lost && !redrawing.count(static_cast<size_t>(index)) && !closed) {
        redrawing.insert(static_cast<size_t>(index));
        renderFrame(static_cast<size_t>(index), generation, true);   // the hover values come back in a moment
    }
}

// the next frame that is drawn, going round
void LoopWindow::advance() {
    const int n = static_cast<int>(frames.size());
    for (int k = 1; k <= n; k++) {
        const int next = ((shown < 0 ? -1 : shown) + k) % n;
        if (!frames[static_cast<size_t>(next)].empty()) {
            showFrame(next);
            return;
        }
    }
}

void LoopWindow::save() {
    std::vector<QByteArray> ready;
    const auto compose = [this] (size_t index) {
        std::vector<QByteArray> pictures;
        for (const auto& cell : frames[index]) {
            pictures.push_back(cell.bytes);
        }
        return PaneGrid::composite(pictures);
    };
    for (size_t i = 0; i < frames.size(); i++) {
        if (!frames[i].empty()) {
            ready.push_back(compose(i));
        }
    }
    if (ready.empty()) {
        return;
    }
    const QByteArray still = shown >= 0 && !frames[static_cast<size_t>(shown)].empty() ? compose(static_cast<size_t>(shown)) : ready.front();
    const auto name = QString::fromStdString(saveName());
    UtilityAnimationExport::saveWithDialog(this, ready.size() >= 2 ? ready : std::vector<QByteArray>{}, delayMs(), still, name, QByteArray{}, false, name);
}
