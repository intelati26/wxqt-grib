// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MasterMapViewer.h"
#include "history/HistoryCursor.h"
#include <QTimeZone>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDateTimeEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QPageSize>
#include <QPdfWriter>
#include <QHBoxLayout>
#include <QCoreApplication>
#include <QFont>
#include <QStyle>
#include <QPainterPath>
#include "hurricane/Coast.h"
#include "cfs/CfsPlumeViewer.h"
#include "mapkit/ModelChartLayer.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"
#include "ui/ElidedText.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
    const int panelWidth = 330;

    vector<string> split(const string& text, char separator) {
        vector<string> parts;
        std::stringstream stream{text};
        string part;
        while (std::getline(stream, part, separator)) {
            if (!part.empty()) {
                parts.push_back(part);
            }
        }
        return parts;
    }
}

MasterMapViewer::MasterMapViewer(Window * parent, const string& startView)
    : Window{parent}
    , comboView{this, {"Your layers"}}
    , buttonRefresh{this, None, "Refresh layers"}
    , buttonSave{this, None, "Export view..."}
    , buttonPlume{this, None, "Plume..."}
    , textStatus{this, "Switch layers on in the tree"}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Master map - everything drawn on a map");
    textStatus.setWordWrap(false);
    layers = MapCatalog::makeLayers();
    std::stable_sort(layers.begin(), layers.end(), [] (const auto& a, const auto& b) { return a->order() < b->order(); });

    const auto dimens = UtilityUI::getScreenBounds();
    const int side = std::max(320, std::min(dimens[0] - panelWidth - 40, dimens[1] - 190));
    mapView = std::make_unique<MapView>(this, side);
    auto * map = mapView->map();
    map->dataLayer = [] (QPainter& painter) { painter.fillRect(QRectF{-1.0e6, -1.0e6, 2.0e6, 2.0e6}, QColor{16, 26, 42}); };
    map->topLayer = [this] (QPainter& painter) { paintMap(painter); paintLegend(painter); };
    mapView->onPointer = [this] (const QPointF& at) {
        showHover(at);
        const auto [lat, lon] = mapView->toLatLon(at);
        pointerLat = lat;
        pointerLon = lon;
        havePointer = true;
    };
    mapView->onLeave = [this] { hoverLabel->hide(); };
    // a click on a mark opens what it has (and does not also zoom the map out)
    map->clickHandler = [this] (const QPointF& at) {
        const auto hit = bestHit(at);
        if (hit.valid() && hit.open) {
            hit.open(this);
            return true;
        }
        return false;
    };
    mapView->showRegion(20.0, 55.0, -127.0, -65.0);
    hoverLabel = new QLabel{map};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 220); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();

    // the side panel: the tree, the options of the layer chosen in it
    sidePanel = new QWidget{this};
    sidePanel->setFixedWidth(panelWidth);
    auto * column = new QVBoxLayout{sidePanel};
    column->setContentsMargins(4, 0, 0, 0);
    // the layers that are on, each with its options one click away; the whole tree (every layer there is) is in the "Add layers" window
    auto * activeTitle = new QLabel{"Layers on", sidePanel};
    activeTitle->setStyleSheet("font-weight: bold;");
    column->addWidget(activeTitle);
    activeBox = new QWidget{sidePanel};
    activeLayout = new QVBoxLayout{activeBox};
    activeLayout->setContentsMargins(0, 0, 0, 0);
    activeLayout->setSpacing(2);
    column->addWidget(activeBox);
    auto * addButton = new QPushButton{"Add layers...", sidePanel};
    addButton->setToolTip("Every layer the map has, in a tree: switch any on or off, and set its options");
    QObject::connect(addButton, &QPushButton::clicked, [this] { showAddLayers(); });
    column->addWidget(addButton);
    tree = new QTreeWidget{sidePanel};
    tree->setHeaderHidden(true);
    tree->setIndentation(16);
    tree->setMinimumHeight(340);
    tree->hide();   // lives in the Add layers window
    legendCheck = new QCheckBox{"Show the legend", sidePanel};
    legendCheck->setChecked(Utility::readPref("MASTERMAP_LEGEND", "true") == "true");
    QObject::connect(legendCheck, &QCheckBox::toggled, [this] (bool on) { Utility::writePref("MASTERMAP_LEGEND", on ? "true" : "false"); redraw(); });
    column->addWidget(legendCheck);
    optionsTitle = new QLabel{sidePanel};
    optionsTitle->setStyleSheet("font-weight: bold;");
    optionsTitle->hide();
    optionsBox = new QWidget{sidePanel};
    optionsLayout = new QVBoxLayout{optionsBox};
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsBox->hide();
    column->addStretch();

    std::vector<string> views{"Your layers"};
    for (const auto& preset : MapCatalog::presets()) {
        views.push_back(preset.name);
    }
    comboView.setList(views);
    comboView.connect([this] {
        if (!building && comboView.getIndex() > 0) {
            applyPreset(static_cast<size_t>(comboView.getIndex() - 1));
        }
    });
    buttonRefresh.connect([this] {
        for (auto& layer : layers) {
            if (layer->enabled()) {
                layer->refresh(*this);
            }
        }
    });
    buttonSave.connect([this] { exportView(); });
    buttonPlume.getView()->setToolTip("The forecast plume (every member or run, at the same valid times) at the place the pointer was last on the map: the model of the chart on the map (CFS: the nine month runs, GEFS: the members, any other: the GFS runs), or the GEFS");
    buttonPlume.connect([this] {
        double lat = pointerLat, lon = pointerLon;
        if (!havePointer) {   // no pointer yet: the middle of the map
            const auto * widget = mapView->map();
            const auto middle = mapView->toLatLon(QPointF{widget->width() / 2.0, widget->height() / 2.0});
            lat = middle.first;
            lon = middle.second;
        }
        QString modelId = "gefs";
        for (const auto& layer : layers) {
            if (layer->id() == "model/chart" && layer->enabled()) {
                const auto& name = static_cast<ModelChartLayer *>(layer.get())->modelName();
                modelId = name == "CFS" ? "cfs" : name.compare(0, 4, "GEFS") == 0 ? "gefs" : "gfs";
            }
        }
        new CfsPlumeViewer{this, lat, lon > 180.0 ? lon - 360.0 : lon, modelId};
    });
    rowTop.addWidget(comboView);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonSave);
    rowTop.addWidget(buttonPlume);
    rowTop.addStretch();
    // the time bar: shown while a layer with frames is on
    timeRow = new QWidget{this};
    auto * timeLayout = new QHBoxLayout{timeRow};
    timeLayout->setContentsMargins(0, 0, 0, 0);
    stepBack = new QPushButton{"<", timeRow};
    playButton = new QPushButton{"Loop", timeRow};
    stepForward = new QPushButton{">", timeRow};
    liveButton = new QPushButton{"Latest", timeRow};
    timeSlider = new QSlider{Qt::Horizontal, timeRow};
    timeSlider->setMinimum(0);
    timeSlider->setMaximum(0);
    speedCombo = new QComboBox{timeRow};
    speedCombo->addItems({"slow", "medium", "fast"});
    speedCombo->setCurrentIndex(std::clamp(Utility::readPrefInt("MASTERMAP_SPEED", 1), 0, 2));
    timeLabel = new QLabel{"live", timeRow};
    timeLabel->setMinimumWidth(180);
    for (auto * button : {stepBack, stepForward}) {
        button->setFixedWidth(34);
    }
    timeLayout->addWidget(stepBack);
    timeLayout->addWidget(playButton);
    timeLayout->addWidget(stepForward);
    timeLayout->addWidget(timeSlider, 1);
    timeLayout->addWidget(speedCombo);
    timeLayout->addWidget(liveButton);
    // a calendar and clock to go to any time (UTC) of the past
    timePick = new QDateTimeEdit{QDateTime::currentDateTimeUtc(), timeRow};
    timePick->setDisplayFormat("yyyy-MM-dd HH:mm 'Z'");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    timePick->setTimeZone(QTimeZone::utc());
#else
    timePick->setTimeSpec(Qt::UTC);   // (Qt 6.4 has no setTimeZone on the date box)
#endif
    timePick->setCalendarPopup(true);
    timePick->setToolTip("Pick a day and time (UTC) and press Go: the layers with a past (radar, satellite, reports, cones) show it");
    timeGo = new QPushButton{"Go", timeRow};
    timeGo->setFixedWidth(40);
    timeLayout->addWidget(timePick);
    timeLayout->addWidget(timeGo);
    timeLayout->addWidget(timeLabel);
    timeRow->hide();
    QObject::connect(timeGo, &QPushButton::clicked, [this] { jumpTo(timePick->dateTime().toUTC()); });
    QObject::connect(stepBack, &QPushButton::clicked, [this] { if (ticks.size() > 1) { playing = false; playTimer.stop(); playButton->setText("Loop"); goTo(std::max(0, timeSlider->value() - 1)); announceTime(); } });
    QObject::connect(stepForward, &QPushButton::clicked, [this] { if (ticks.size() > 1) { playing = false; playTimer.stop(); playButton->setText("Loop"); goTo(std::min(static_cast<int>(ticks.size()) - 1, timeSlider->value() + 1)); announceTime(); } });
    QObject::connect(playButton, &QPushButton::clicked, [this] { togglePlay(); });
    QObject::connect(liveButton, &QPushButton::clicked, [this] { goLive(); });
    QObject::connect(timeSlider, &QSlider::valueChanged, [this] (int value) { if (!settingSlider) { playing = false; playTimer.stop(); playButton->setText("Loop"); goTo(value); announceTime(); } });
    QObject::connect(&HistoryCursor::get(), &HistoryCursor::moved, this, [this] (QObject * origin) { followCursor(origin); });
    QObject::connect(speedCombo, &QComboBox::currentIndexChanged, [this] (int index) {
        Utility::writePrefInt("MASTERMAP_SPEED", index);
        playTimer.setInterval(index == 0 ? 1200 : index == 1 ? 650 : 280);
    });
    playTimer.setInterval(speedCombo->currentIndex() == 0 ? 1200 : speedCombo->currentIndex() == 1 ? 650 : 280);
    QObject::connect(&playTimer, &QTimer::timeout, [this] { advance(); });
    rowMain.addWidgetReal(map, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(sidePanel, 1, Qt::AlignTop | Qt::AlignLeft);
    ElidedText::install(sidePanel);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidgetReal(timeRow, 0, Qt::Alignment{});
    box.addLayout(rowMain);
    box.addStretch();
    box.getAndShow(this);
    resize(1240, 880);
    buildTree();
    // what was on last time
    building = true;
    for (auto id : split(Utility::readPref("MASTERMAP_ON", "obs/airports"), ',')) {
        if (id == "tropical/deepmind") {
            id = "tropical/spaghetti";   // DeepMind's layer became part of the ensemble tracks layer
        }
        setLayerOn(id, true);
        if (auto * item = items.count(id) != 0 ? items[id] : nullptr) {
            item->setCheckState(0, Qt::Checked);
        }
    }
    building = false;
    refreshActiveList();
    // development aid (run with QT_QPA_PLATFORM=offscreen, see main.cpp): WXQT_MAPVIEW=<the start of a view's name> picks that view, as the box does
    if (const auto wanted = startView.empty() ? qEnvironmentVariable("WXQT_MAPVIEW").toStdString() : startView; !wanted.empty()) {
        const auto& all = MapCatalog::presets();
        for (size_t i = 0; i < all.size(); i++) {
            if (all[i].name.compare(0, wanted.size(), wanted) == 0) {
                applyPreset(i);
                break;
            }
        }
    }
    updateStatus();
    listTimer.setSingleShot(true);
    QObject::connect(&listTimer, &QTimer::timeout, [this] { refreshActiveList(); });
    timer.setInterval(30000);
    QObject::connect(&timer, &QTimer::timeout, [this] { tick(); });
    timer.start();
}

MapLayer * MasterMapViewer::layerOf(const string& id) const {
    for (const auto& layer : layers) {
        if (layer->id() == id) {
            return layer.get();
        }
    }
    return nullptr;
}

void MasterMapViewer::buildTree() {
    building = true;
    std::map<string, QTreeWidgetItem *> groups;
    // the tree follows the order of the paths, not of the painting
    vector<MapLayer *> byPath;
    for (const auto& layer : layers) {
        byPath.push_back(layer.get());
    }
    std::stable_sort(byPath.begin(), byPath.end(), [] (const auto * a, const auto * b) { return a->path() < b->path(); });
    for (auto * layer : byPath) {
        const auto parts = split(layer->path(), '/');
        QTreeWidgetItem * parent = nullptr;
        string key;
        for (size_t i = 0; i + 1 < parts.size(); i++) {
            key += "/" + parts[i];
            auto found = groups.find(key);
            if (found == groups.end()) {
                auto * group = parent == nullptr ? new QTreeWidgetItem{tree} : new QTreeWidgetItem{parent};
                group->setText(0, QString::fromStdString(parts[i]));
                group->setFlags(group->flags() | Qt::ItemIsUserCheckable | Qt::ItemIsAutoTristate);
                group->setCheckState(0, Qt::Unchecked);
                QFont font = group->font(0);
                font.setBold(true);
                group->setFont(0, font);
                group->setExpanded(true);
                found = groups.emplace(key, group).first;
            }
            parent = found->second;
        }
        auto * item = parent == nullptr ? new QTreeWidgetItem{tree} : new QTreeWidgetItem{parent};
        item->setText(0, QString::fromStdString(parts.empty() ? layer->id() : parts.back()));
        item->setToolTip(0, QString::fromStdString(layer->tip()));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        item->setData(0, Qt::UserRole, QString::fromStdString(layer->id()));
        items[layer->id()] = item;
    }
    QObject::connect(tree, &QTreeWidget::itemChanged, [this] (QTreeWidgetItem * item, int) {
        if (building) {
            return;
        }
        const auto id = item->data(0, Qt::UserRole).toString().toStdString();
        if (!id.empty()) {
            setLayerOn(id, item->checkState(0) == Qt::Checked);
            comboView.setIndex(0);
        }
    });
    QObject::connect(tree, &QTreeWidget::currentItemChanged, [this] (QTreeWidgetItem * item, QTreeWidgetItem *) {
        showOptions(item == nullptr ? nullptr : layerOf(item->data(0, Qt::UserRole).toString().toStdString()));
    });
    building = false;
}

void MasterMapViewer::showOptions(MapLayer * layer) {
    while (auto * item = optionsLayout->takeAt(0)) {
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    optionsTitle->setText(layer == nullptr ? QString{} : QString::fromStdString(layer->path()).section('/', -1) + " - options");
    if (layer != nullptr) {
        if (auto * widget = layer->options(optionsBox, [this, layer] { layer->optionChanged(*this); redraw(); })) {
            optionsLayout->addWidget(widget);
        } else {
            auto * none = new QLabel{"No options for this layer.", optionsBox};
            none->setEnabled(false);
            optionsLayout->addWidget(none);
        }
    }
}

void MasterMapViewer::setLayerOn(const string& id, bool on) {
    auto * layer = layerOf(id);
    if (layer == nullptr || layer->enabled() == on) {
        return;
    }
    if (on) {
        layer->enable(*this);
        refreshed[id] = std::time(nullptr);
    } else {
        layer->disable();
    }
    if (auto found = items.find(id); found != items.end() && (found->second->checkState(0) == Qt::Checked) != on) {   // the tree follows (it may be hidden)
        const bool was = building;
        building = true;
        found->second->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
        building = was;
    }
    saveState();
    rebuildTicks();
    updateStatus();
    redraw();
    QTimer::singleShot(0, this, [this] { refreshActiveList(); });
}

// the layers that are on, in painting order: a check to switch one off, its name and what it has read, and a gear for its options
void MasterMapViewer::refreshActiveList() {
    if (activeLayout == nullptr) {
        return;
    }
    while (auto * item = activeLayout->takeAt(0)) {
        if (auto * widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    int shown = 0;
    for (const auto& layer : layers) {
        if (!layer->enabled()) {
            continue;
        }
        shown++;
        auto * row = new QWidget{activeBox};
        auto * rowLayout = new QHBoxLayout{row};
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        auto * check = new QCheckBox{QString::fromStdString(layer->path()).section('/', -1), row};
        check->setChecked(true);
        check->setToolTip(QString::fromStdString(layer->tip()) + "\n(uncheck to switch it off)");
        const string id = layer->id();
        QObject::connect(check, &QCheckBox::clicked, [this, id] (bool on) { setLayerOn(id, on); comboView.setIndex(0); });
        rowLayout->addWidget(check, 1);
        auto * gear = new QToolButton{row};
        gear->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));   // no text: the panel's button eliding leaves it alone
        gear->setAutoRaise(true);
        gear->setToolTip("Options: the period, product and the rest of this layer's options");
        QObject::connect(gear, &QToolButton::clicked, [this, id] { showLayerOptions(layerOf(id)); });
        rowLayout->addWidget(gear);
        activeLayout->addWidget(row);
        if (const auto text = layer->summary(); !text.empty()) {
            auto * note = new QLabel{QString::fromStdString(text), activeBox};
            note->setStyleSheet("color: gray; font-size: 9pt; margin-left: 22px;");
            activeLayout->addWidget(note);
        }
    }
    if (shown == 0) {
        auto * none = new QLabel{"Nothing is on: choose a view above, or add layers.", activeBox};
        none->setWordWrap(true);
        none->setEnabled(false);
        activeLayout->addWidget(none);
    }
}

// the options of one layer in a small window of their own, by its gear
void MasterMapViewer::showLayerOptions(MapLayer * layer) {
    if (layer == nullptr) {
        return;
    }
    auto * dialog = new QDialog{this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QString::fromStdString(layer->path()).section('/', -1) + " - options");
    auto * column = new QVBoxLayout{dialog};
    if (auto * widget = layer->options(dialog, [this, layer] { layer->optionChanged(*this); redraw(); QTimer::singleShot(0, this, [this] { refreshActiveList(); }); })) {
        column->addWidget(widget);
    } else {
        column->addWidget(new QLabel{"No options for this layer.", dialog});
    }
    dialog->setMinimumWidth(280);
    dialog->show();
}

void MasterMapViewer::showAddLayers() {
    if (addDialog == nullptr) {
        addDialog = new QDialog{this};
        addDialog->setWindowTitle("Layers");
        auto * column = new QVBoxLayout{addDialog};
        column->addWidget(tree, 3);
        column->addWidget(optionsTitle);
        column->addWidget(optionsBox, 1);
        tree->show();
        optionsTitle->show();
        optionsBox->show();
        addDialog->resize(420, 640);
    }
    addDialog->show();
    addDialog->raise();
}

// the frames of every layer that has them, in one list for the bar; the bar is shown only while there is such a layer
void MasterMapViewer::rebuildTicks() {
    std::set<long> all;
    bool any = false;
    for (const auto& layer : layers) {
        if (layer->enabled() && layer->timeAware()) {
            any = true;
            for (const long t : layer->times()) {
                all.insert(t);
            }
        }
    }
    timeRow->setVisible(any);
    const long keep = !live && timeSlider->value() >= 0 && static_cast<size_t>(timeSlider->value()) < ticks.size() ? ticks[static_cast<size_t>(timeSlider->value())] : 0;
    ticks.assign(all.begin(), all.end());
    settingSlider = true;
    timeSlider->setMaximum(std::max(0, static_cast<int>(ticks.size()) - 1));
    timeSlider->setEnabled(ticks.size() > 1);
    int index = static_cast<int>(ticks.size()) - 1;
    if (!live && keep != 0) {
        const auto found = std::lower_bound(ticks.begin(), ticks.end(), keep);
        index = found == ticks.end() ? static_cast<int>(ticks.size()) - 1 : static_cast<int>(found - ticks.begin());
    }
    timeSlider->setValue(std::max(0, index));
    settingSlider = false;
    if (playWhenReady && ticks.size() > 1) {
        playWhenReady = false;
        playing = true;
        playButton->setText("Stop");
        playTimer.start();
    }
    string text = live ? "latest" : "";
    for (const auto& layer : layers) {
        if (layer->enabled() && layer->timeAware() && !layer->timeText().empty()) {
            text = layer->timeText() + (live ? "  (latest)" : "");
            break;
        }
    }
    timeLabel->setText(QString::fromStdString(text));
    if (ticks.size() > 1) {
        extra.clear();   // the "loading the frames" note
        if (!playing && playButton->text() == "Loading...") {
            playButton->setText("Loop");
        }
    }
    updateStatus();
    redraw();
}

void MasterMapViewer::jumpTo(const QDateTime& utc) {
    following = true;
    live = false;
    playing = false;
    playTimer.stop();
    playButton->setText("Loop");
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            layer->setAsOf(utc, *this);
        }
    }
    rebuildTicks();
    const long seconds = static_cast<long>(utc.toSecsSinceEpoch());
    int index = 0;
    for (size_t i = 0; i < ticks.size(); i++) {
        if (ticks[i] <= seconds) {
            index = static_cast<int>(i);
        }
    }
    live = false;
    goTo(index);
    live = false;
    extra = "as of " + utc.toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
    updateStatus();
    setTitle("Master map - " + extra);
    HistoryCursor::get().setTime(utc, this);
}

void MasterMapViewer::goLive() {
    live = true;
    playing = false;
    playTimer.stop();
    playButton->setText("Loop");
    for (auto& layer : layers) {
        if (layer->enabled() && layer->timeAware()) {
            layer->showTime(0, *this);
        }
    }
    rebuildTicks();
}

void MasterMapViewer::goTo(int index) {
    if (ticks.empty() || index < 0 || static_cast<size_t>(index) >= ticks.size()) {
        return;
    }
    live = static_cast<size_t>(index) + 1 == ticks.size() && !playing;
    const long t = ticks[static_cast<size_t>(index)];
    for (auto& layer : layers) {
        if (layer->enabled() && layer->timeAware()) {
            layer->showTime(live ? 0 : t, *this);
        }
    }
    settingSlider = true;
    timeSlider->setValue(index);
    settingSlider = false;
    string text;
    for (const auto& layer : layers) {
        if (layer->enabled() && layer->timeAware() && !layer->timeText().empty()) {
            text = layer->timeText();
            break;
        }
    }
    timeLabel->setText(QString::fromStdString(text + (live ? "  (latest)" : "")));
    if (timePick != nullptr && !live) {
        timePick->setDateTime(QDateTime::fromSecsSinceEpoch(ticks[static_cast<size_t>(index)], QTimeZone::utc()));
    }
    updateStatus();
    redraw();
}

void MasterMapViewer::togglePlay() {
    if (playing || playWhenReady) {
        playing = false;
        playWhenReady = false;
        playTimer.stop();
        playButton->setText("Loop");
        return;
    }
    if (ticks.size() < 2) {   // the frames are not loaded yet: ask for them and start when they are here
        playWhenReady = true;
        playButton->setText("Loading...");
        status("loading the frames for a loop...");
        for (auto& layer : layers) {
            if (layer->enabled() && layer->timeAware()) {
                layer->prepareTimes(*this);
            }
        }
        return;
    }
    // from the start of the loop, or from where the slider is when it is not at the end
    if (timeSlider->value() >= static_cast<int>(ticks.size()) - 1) {
        timeSlider->setValue(0);
    }
    playing = true;
    playButton->setText("Stop");
    playTimer.start();
}

void MasterMapViewer::advance() {
    if (ticks.size() < 2) {
        return;
    }
    int next = timeSlider->value() + 1;
    if (next >= static_cast<int>(ticks.size())) {
        next = 0;   // round again, with a pause at the newest
    }
    goTo(next);
}

// The whole view as one picture: a header with the view's name, the layers that are on, the time of each frame and the region; the map as it is on the screen
// (legend included) at twice the resolution; a footer with who made the data and when the picture was saved. PNG or PDF.
void MasterMapViewer::paintExport(QPainter& painter, int side, double scale, bool vector) {
    (void)vector;
    const int width = side;
    QStringList names;
    QStringList times;
    QStringList sources;
    for (const auto& layer : layers) {
        if (!layer->enabled()) {
            continue;
        }
        names << QString::fromStdString(layer->path()).section('/', -1);
        if (layer->timeAware() && !layer->timeText().empty()) {
            times << QString::fromStdString(layer->timeText());
        }
        const auto source = QString::fromStdString(layer->source());
        if (!source.isEmpty() && !sources.contains(source)) {
            sources << source;
        }
    }
    QFont base{painter.font()};
    base.setPixelSize(static_cast<int>(13 * scale));
    QFont bold{base};
    bold.setBold(true);
    bold.setPixelSize(static_cast<int>(17 * scale));
    const int pad = static_cast<int>(8 * scale);
    const int headerHeight = static_cast<int>(66 * scale);
    const int footerHeight = static_cast<int>(38 * scale);   // room for two lines of credits
    painter.fillRect(QRect{0, 0, width, headerHeight}, QColor{250, 250, 250});
    painter.setPen(QColor{25, 25, 25});
    painter.setFont(bold);
    const auto viewName = comboView.getIndex() > 0 ? QString::fromStdString(comboView.getValue()) : QString{"Master map"};
    painter.drawText(QRect{pad, pad / 2, width - 2 * pad, static_cast<int>(24 * scale)}, Qt::AlignVCenter | Qt::AlignLeft, viewName);
    painter.setFont(base);
    painter.setPen(QColor{60, 60, 60});
    painter.drawText(QRect{pad, static_cast<int>(27 * scale), width - 2 * pad, static_cast<int>(18 * scale)}, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
        names.isEmpty() ? QString{"No layers on"} : "Layers: " + names.join(", "));
    painter.drawText(QRect{pad, static_cast<int>(45 * scale), width - 2 * pad, static_cast<int>(18 * scale)}, Qt::AlignVCenter | Qt::AlignLeft,
        (times.isEmpty() ? QString{"Valid now"} : "Valid: " + times.join("   ")));
    // the map, with its legend, as drawn on the screen
    auto * map = mapView->map();
    const double factor = static_cast<double>(width) / std::max(1, map->width());
    painter.save();
    painter.translate(0, headerHeight);
    painter.scale(factor, factor);
    map->render(&painter, QPoint{}, QRegion{}, QWidget::DrawWindowBackground);   // without the hover popup
    painter.restore();
    const int mapBottom = headerHeight + static_cast<int>(map->height() * factor);
    painter.fillRect(QRect{0, mapBottom, width, footerHeight}, QColor{245, 245, 245});
    QFont small{base};
    small.setPixelSize(static_cast<int>(11 * scale));
    painter.setFont(small);
    painter.setPen(QColor{100, 100, 100});
    painter.drawText(QRect{pad, mapBottom, width - 2 * pad, footerHeight}, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
        (sources.isEmpty() ? QString{} : "Data: " + sources.join("; ") + "  -  ") + "saved " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + " UTC  -  exported by wxqt-grib");
}

bool MasterMapViewer::saveModelChart(const QString& path, string& problem) {
    for (const auto& layer : layers) {
        if (layer->id() == "model/chart" && layer->enabled()) {
            QImage image;
            if (!static_cast<ModelChartLayer *>(layer.get())->titled(*this, image, problem)) {
                return false;
            }
            if (!image.save(path, "PNG")) {
                problem = "the file could not be written: " + path.toStdString();
                return false;
            }
            return true;
        }
    }
    problem = "no model chart is on the map";
    return false;
}

void MasterMapViewer::exportView() {
    // A model chart on the map can be saved as the Model Viewer's own picture of it (its title with the model, chart, run and valid times, the legend and the credit), for what is in view
    for (const auto& layer : layers) {
        if (layer->id() != "model/chart" || !layer->enabled()) {
            continue;
        }
        QMessageBox ask{this};
        ask.setWindowTitle("Export the view");
        ask.setText("A model chart is on the map. Save it as the Model Viewer's picture (with its title, run and valid times and legend), or the map as it is?");
        auto * titledButton = ask.addButton("Model chart, titled", QMessageBox::AcceptRole);
        auto * mapButton = ask.addButton("The map as it is", QMessageBox::ActionRole);
        ask.addButton(QMessageBox::Cancel);
        ask.exec();
        if (ask.clickedButton() == titledButton) {
            const auto path = QFileDialog::getSaveFileName(this, "Save the model chart", ChartExport::startFolder() + "/" + ChartExport::fileName("model chart", "png"), "PNG image (*.png)");
            if (path.isEmpty()) {
                return;
            }
            string problem;
            if (!saveModelChart(path, problem)) {
                QMessageBox::warning(this, "Export the view", QString::fromStdString(problem));
                return;
            }
            ChartExport::remember(path);
            status("Saved " + path.toStdString());
            return;
        }
        if (ask.clickedButton() != mapButton) {
            return;
        }
        break;
    }
    QString filter;
    const auto path = QFileDialog::getSaveFileName(this, "Export the view", ChartExport::startFolder() + "/" + ChartExport::fileName("master map", "png"),
                                                   "PNG image (*.png);;PDF document (*.pdf)", &filter);
    if (path.isEmpty()) {
        return;
    }
    ChartExport::remember(path);
    exportTo(path);
}

void MasterMapViewer::exportTo(const QString& path) {
    auto * map = mapView->map();
    const double ratio = static_cast<double>(map->height()) / std::max(1, map->width());
    if (path.endsWith(".pdf", Qt::CaseInsensitive)) {
        const int side = map->width();
        const int heightPx = static_cast<int>((66 + 38) + side * ratio);
        QPdfWriter writer{path};
        writer.setResolution(150);
        writer.setPageSize(QPageSize{QSizeF{side * 25.4 / 96.0, heightPx * 25.4 / 96.0}, QPageSize::Millimeter});
        writer.setPageMargins(QMarginsF{0, 0, 0, 0});
        QPainter painter{&writer};
        const double factor = static_cast<double>(writer.width()) / side;
        painter.scale(factor, factor);
        paintExport(painter, side, 1.0, true);
        return;
    }
    const double scale = 2.0;
    const int side = static_cast<int>(map->width() * scale);
    QImage image{side, static_cast<int>((66 + 38) * scale + side * ratio), QImage::Format_ARGB32};
    image.fill(Qt::white);
    image.setDevicePixelRatio(1.0);
    QPainter painter{&image};
    paintExport(painter, side, scale, false);
    painter.end();
    image.save(path, "PNG");
    status("Saved " + path.toStdString());
}

void MasterMapViewer::saveState() {
    string ids;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            ids += (ids.empty() ? "" : ",") + layer->id();
        }
    }
    Utility::writePref("MASTERMAP_ON", ids);
}

void MasterMapViewer::applyPreset(size_t index) {
    const auto& presets = MapCatalog::presets();
    if (index >= presets.size()) {
        return;
    }
    const auto& preset = presets[index];
    building = true;
    for (const auto& layer : layers) {
        const bool want = std::find(preset.layers.begin(), preset.layers.end(), layer->id()) != preset.layers.end();
        if (auto * item = items.count(layer->id()) != 0 ? items[layer->id()] : nullptr) {
            item->setCheckState(0, want ? Qt::Checked : Qt::Unchecked);
        }
        setLayerOn(layer->id(), want);
    }
    building = false;
    if (comboView.getIndex() != static_cast<int>(index) + 1) {   // the box names the view that was applied (also when it was applied by the start of its name)
        comboView.block();
        comboView.setIndex(index + 1);
        comboView.unblock();
    }
    mapView->showRegion(preset.minLat, preset.maxLat, preset.minLon, preset.maxLon);
    updateStatus();
}

// the time bar was moved by the person: the other history screens go to that time
void MasterMapViewer::announceTime() {
    const int index = timeSlider->value();
    if (following && !applyingCursor && index >= 0 && static_cast<size_t>(index) < ticks.size()) {
        HistoryCursor::get().setTime(QDateTime::fromSecsSinceEpoch(ticks[static_cast<size_t>(index)], QTimeZone::utc()), this);
    }
}

// another history screen (or the page) moved the time: the newest frame at or before it, or the layers' own "as of"
void MasterMapViewer::followCursor(QObject * origin) {
    if (!following || origin == this) {
        return;
    }
    const auto utc = HistoryCursor::get().time();
    const long seconds = static_cast<long>(utc.toSecsSinceEpoch());
    applyingCursor = true;
    if (ticks.empty() || seconds < ticks.front() || seconds > ticks.back() + 1800) {   // outside the frames on hand: the layers look again around the new time
        for (const auto& layer : layers) {
            if (layer->enabled()) {
                layer->setAsOf(utc, *this);
            }
        }
        rebuildTicks();
    }
    if (ticks.size() > 1) {
        int index = 0;
        for (size_t i = 0; i < ticks.size(); i++) {
            if (ticks[i] <= seconds) {
                index = static_cast<int>(i);
            }
        }
        playing = false;
        playTimer.stop();
        playButton->setText("Loop");
        goTo(index);
    }
    extra = "history cursor " + utc.toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
    applyingCursor = false;
    updateStatus();
}

void MasterMapViewer::openAt(const QDateTime& utc, double lat, double lon, double radiusKm, const vector<string>& layerIds) {
    following = true;
    building = true;
    for (const auto& layer : layers) {
        const bool want = std::find(layerIds.begin(), layerIds.end(), layer->id()) != layerIds.end();
        if (auto * item = items.count(layer->id()) != 0 ? items[layer->id()] : nullptr) {
            item->setCheckState(0, want ? Qt::Checked : Qt::Unchecked);
        }
        setLayerOn(layer->id(), want);
    }
    building = false;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            layer->setAsOf(utc, *this);
        }
    }
    const double degrees = std::max(0.5, radiusKm / 111.0);
    const double lonDegrees = degrees / std::max(0.2, std::cos(lat * 3.14159265358979 / 180.0));
    mapView->showRegion(lat - degrees * 1.2, lat + degrees * 1.2, lon - lonDegrees * 1.6, lon + lonDegrees * 1.6);
    timePick->setDateTime(utc.toUTC());
    extra = "as of " + utc.toUTC().toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
    updateStatus();
    setTitle("Master map - " + extra);
}

void MasterMapViewer::onlyLayer(const string& id) {
    building = true;
    for (const auto& layer : layers) {
        const bool want = layer->id() == id;
        if (auto * item = items.count(layer->id()) != 0 ? items[layer->id()] : nullptr) {
            item->setCheckState(0, want ? Qt::Checked : Qt::Unchecked);
        }
        setLayerOn(layer->id(), want);
    }
    building = false;
    updateStatus();
}

void MasterMapViewer::updateStatus() {
    string text;
    for (const auto& layer : layers) {
        if (layer->enabled() && !layer->summary().empty()) {
            text += (text.empty() ? "" : "   -   ") + layer->summary();
        }
    }
    if (!extra.empty()) {
        text += (text.empty() ? "" : "   -   ") + extra;
    }
    textStatus.setText(text.empty() ? string{"Switch layers on in the tree"} : text);
}

void MasterMapViewer::tick() {
    const auto now = std::time(nullptr);
    for (auto& layer : layers) {
        const int every = layer->refreshSeconds();
        if (layer->enabled() && every > 0 && now - refreshed[layer->id()] >= every) {
            refreshed[layer->id()] = now;
            layer->refresh(*this);
        }
    }
}

bool MasterMapViewer::claimCell(const QPointF& pixels, double spacing) {
    const long long columns = static_cast<long long>(std::ceil(mapView->map()->width() / spacing)) + 2;
    const long long cell = static_cast<long long>(std::floor(pixels.y() / spacing) + 1) * columns + static_cast<long long>(std::floor(pixels.x() / spacing) + 1);
    // two spacings share nothing: the spacing is part of the key
    return cells.insert(cell * 64 + static_cast<long long>(spacing) % 64).second;
}

void MasterMapViewer::background(std::function<void()> work, std::function<void()> done) {
    new FutureVoid{this, std::move(work), [this, done = std::move(done)] {
        if (!closed) {
            done();
            updateStatus();
        }
    }};
}

void MasterMapViewer::paintMap(QPainter& painter) {
    const auto t = mapView->transform();
    const double px = mapView->unitsPerPixel();
    painter.setRenderHint(QPainter::Antialiasing, true);
    cells.clear();
    // the base layers first (a satellite picture), then the coastlines and borders over them
    for (auto& layer : layers) {
        if (layer->enabled() && layer->underCoast()) {
            painter.save();
            layer->paint(painter, *this);
            painter.restore();
        }
    }
    // the coastlines and borders of the world's tropics and mid-latitudes
    painter.setPen(QPen{QColor{110, 125, 145}, 1.0 * px});
    painter.setBrush(Qt::NoBrush);
    for (const auto& line : Coast::borders()) {
        QPainterPath path;
        bool started = false;
        for (const auto& [lon, lat] : line) {
            const auto p = t(lat, lon);
            if (p.x() < -1500.0 || p.x() > 1500.0 || p.y() < -1250.0 || p.y() > 1750.0) {
                started = false;
                continue;
            }
            started ? path.lineTo(p) : path.moveTo(p);
            started = true;
        }
        painter.drawPath(path);
    }
    for (auto& layer : layers) {
        if (layer->enabled() && !layer->underCoast()) {
            painter.save();
            layer->paint(painter, *this);
            painter.restore();
        }
    }
}

void MasterMapViewer::paintLegend(QPainter& painter) {
    if (!legendCheck->isChecked()) {
        return;
    }
    vector<MapLegendRow> rows;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            for (auto& row : layer->legend()) {
                rows.push_back(std::move(row));
            }
        }
    }
    const auto * map = mapView->map();
    MapLegend::draw(painter, rows, mapView->unitsPerPixel(), -250.0 + 1000.0 * map->height() / std::max(1, map->width()) - 2.0);
}

MapHit MasterMapViewer::bestHit(const QPointF& pixels) const {
    MapHit best;
    for (const auto& layer : layers) {
        if (!layer->enabled()) {
            continue;
        }
        const auto hit = layer->pick(pixels, const_cast<MasterMapViewer&>(*this));
        if (!hit.valid()) {
            continue;
        }
        if (!best.valid() || hit.priority > best.priority || (hit.priority == best.priority && hit.distance < best.distance)) {
            best = hit;
        }
    }
    return best;
}

void MasterMapViewer::showHover(const QPointF& pixels) {
    const auto hit = bestHit(pixels);
    if (!hit.valid()) {
        hoverLabel->hide();
        mapView->map()->setCursor(Qt::ArrowCursor);
        return;
    }
    mapView->map()->setCursor(hit.open ? Qt::PointingHandCursor : Qt::ArrowCursor);
    hoverLabel->setText(hit.text + (hit.open ? "\n(click to open)" : ""));
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}

void MasterMapViewer::resizeEventCustom() {
    if (mapView == nullptr) {
        return;
    }
    // The time bar is one row: when the window is narrow the calendar and Go (the rarely used) give way first, and the time label may be cut instead of widening the window
    if (timePick != nullptr) {
        const bool room = width() >= 1000;
        timePick->setVisible(room);
        timeGo->setVisible(room);
        timeLabel->setMinimumWidth(room ? 180 : 40);
    }
    const int above = ControlRow::heightIn(rowTop.getView(), width()) + textStatus.getView()->sizeHint().height();
    mapView->fitRectangle(width() - panelWidth - 30, height() - above - 40);
}
