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
#include <QBuffer>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenu>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QProgressDialog>
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
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/ModelScreen.h"
#include "climate/TempHistoryViewer.h"
#include "mapkit/ComboChartLayer.h"
#include <QEventLoop>
#include <QFrame>
#include <QGridLayout>
#include <QScrollArea>
#include "mapkit/GlobeViewer.h"
#include "settings/RadarPreferences.h"
#include "models/ProductPicker.h"
#include "objects/UtilityAnimationExport.h"
#include "models/SoundingViewer.h"
#include "settings/Location.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"
#include "ui/ElidedText.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
    const int panelWidth = 360;

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
    , buttonOthers{this, None, "Other models..."}
    , buttonViews{this, None, "Views..."}
    , buttonCompare{this, None, "Compare..."}
    , buttonArea{this, None, "Area..."}
    , buttonGlobe{this, None, "Globe..."}
    , buttonTemps{this, None, "Temperature history..."}
    , buttonSounding{this, None, "Sounding..."}
    , buttonMembers{this, None, "Members graph..."}
    , textStatus{this, "Switch layers on in the tree"}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Master map - everything drawn on a map");
    textStatus.setWordWrap(false);
    textStatus.getView()->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);   // (a long line is cut at the window's edge, not made the window wider; its tooltip has it whole)
    textStatus.getView()->setMinimumWidth(1);
    layers = MapCatalog::makeLayers();
    std::stable_sort(layers.begin(), layers.end(), [] (const auto& a, const auto& b) { return a->order() < b->order(); });

    const auto dimens = UtilityUI::getScreenBounds();
    const int side = std::max(320, std::min(dimens[0] - panelWidth - 40, dimens[1] - 190));
    mapView = std::make_unique<MapView>(this, side);
    mapView->useLayoutSpace();   // (the layout gives the map its size, after the controls)
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
    auto * activeTitle = new QLabel{"Layer manager", sidePanel};
    activeTitle->setStyleSheet("font-weight: bold;");
    column->addWidget(activeTitle);
    activeBox = new QWidget{sidePanel};
    activeLayout = new QVBoxLayout{activeBox};
    activeLayout->setContentsMargins(0, 0, 0, 0);
    activeLayout->setSpacing(2);
    {   // the manager stays in view: when many layers have their options open it scrolls instead of pushing the rest away
        auto * scroll = activeScroll = new QScrollArea{sidePanel};
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scroll->setWidget(activeBox);
        scroll->setMinimumHeight(60);
        column->addWidget(scroll);
    }
    for (const auto& id : QString::fromStdString(Utility::readPref("MASTERMAP_OPEN_OPTIONS", "")).split(',', Qt::SkipEmptyParts)) {
        expanded.insert(id.toStdString());
    }
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
    noticeLabel = new QLabel{sidePanel};
    noticeLabel->setWordWrap(true);
    noticeLabel->setStyleSheet("background: #fff1c2; color: #3a2e00; padding: 4px; border: 1px solid #d9b84a;");
    noticeLabel->hide();
    column->addWidget(noticeLabel);
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
        if (building || comboView.getIndex() <= 0) {
            return;
        }
        const auto index = static_cast<size_t>(comboView.getIndex() - 1);
        if (index < MapCatalog::presets().size()) {
            applyPreset(index);
        } else if (const auto kept = savedViews(); index - MapCatalog::presets().size() < kept.size()) {
            applySaved(kept[index - MapCatalog::presets().size()]);
        }
    });
    refreshViewList();
    buttonRefresh.connect([this] {
        for (auto& layer : layers) {
            if (layer->enabled()) {
                layer->refresh(*this);
            }
        }
    });
    buttonSave.connect([this] { exportView(); });
    buttonPlume.getView()->setToolTip("The forecast plume (every member or run, at the same valid times) at the place the pointer was last on the map: the model of the chart on the map (CFS: the nine month runs, GEFS: the members, any other: the GFS runs), or the GEFS");
    buttonOthers.getView()->setToolTip("The model pages that are not charts of the map: SPC Post (calibrated chances), NSSL CAMs and the picture models kept until they are retired (SPC HRRR, NSSL WRF, WPC GEFS, SPC HREF)");
    buttonOthers.connect([this] {
        QMenu menu{this};
        for (const auto& source : ModelScreen::sources()) {
            menu.addAction(source.label)->setData(source.id);
        }
        if (const auto * chosen = menu.exec(QCursor::pos())) {
            new ModelScreen{this, chosen->data().toString().toStdString()};
        }
    });
    buttonViews.getView()->setToolTip("Keep what is on the map as a view of your own: the layers, the part of the world and the model chart (model, chart, lines, variants). Kept views are in the list on the left");
    buttonViews.connect([this] { viewsMenu(); });
    buttonCompare.getView()->setToolTip("A second map beside this one at the same place and time, with the model chart on: set its model, chart or run to another to compare them (the time bars move together)");
    buttonCompare.connect([this] { openCompare(); });
    buttonArea.getView()->setToolTip("Move the map to a named area: a region of the world (the United States and its regions, continents, oceans, the poles) or a square round one of your saved locations");
    buttonArea.connect([this] { chooseArea(); });
    buttonGlobe.getView()->setToolTip("Every layer on a globe seen from space instead of the flat map, with nothing stretched toward the poles: turn it with the mouse, zoom with the wheel; hover and click work as on the map. The button turns it back into the map");
    buttonGlobe.connect([this] { setGlobe(!globeOn); });
    buttonTemps.getView()->setToolTip("How warm the week, month or year up to a date has been against the same stretch of every year since 1940 (ERA5), at the place the pointer was last on the map: bars by year, the rank, the warmest and coldest years");
    buttonTemps.connect([this] {
        double lat = 0.0, lon = 0.0;
        pointFor(lat, lon);
        new TempHistoryViewer{this, lat, lon > 180.0 ? lon - 360.0 : lon, timePick->dateTime().toUTC().date()};
    });
    buttonSounding.getView()->setToolTip("The sounding at the place the pointer was last on the map for the valid time on the time bar: the column of the chart's own model (GFS, HRRR, IFS ...) read from its pressure levels, or the RRFS sounding when no model chart is on");
    buttonSounding.connect([this] {
        double lat = 0.0, lon = 0.0;
        pointFor(lat, lon);
        const auto * chart = modelLayer();
        if (chart != nullptr && chart->modelName() != "RRFS") {   // the chart's own model: its column over the point
            string why;
            if (chart->openSounding(this, lat, lon > 180.0 ? lon - 360.0 : lon, why)) {
                return;
            }
            status(why + "; the RRFS sounding instead");
        }
        auto valid = chart != nullptr ? chart->validTime() : QDateTime{};
        if (!valid.isValid()) {
            valid = timePick->dateTime().toUTC();
        }
        new SoundingViewer{this, lon > 180.0 ? lon - 360.0 : lon, lat, valid};
    });
    buttonMembers.getView()->setToolTip("REFS charts: each of the 5 members' value at the place the pointer was last on the map over the forecast hours, with the ensemble mean (and the threshold of a probability chart)");
    buttonMembers.connect([this] {
        double lat = 0.0, lon = 0.0;
        pointFor(lat, lon);
        string why;
        const auto * chart = modelLayer();
        if (chart == nullptr || !chart->openMembers(this, lat, lon > 180.0 ? lon - 360.0 : lon, why)) {
            status(chart == nullptr ? "put a REFS chart on the map for the members graph" : why);
        }
    });
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
    rowTop.addWidget(buttonViews);
    rowTop.addWidget(buttonOthers);
    rowTop.addWidget(buttonCompare);
    rowTop.addWidget(buttonArea);
    rowTop.addWidget(buttonGlobe);
    rowTop.addWidget(buttonTemps);
    rowTop.addWidget(buttonSounding);
    rowTop.addWidget(buttonMembers);
    rowTop.addStretch();
    // the time bar: shown while a layer with frames is on
    timeRow = new QWidget{this};
    auto * timeLayout = new QHBoxLayout{timeRow};
    timeLayout->setContentsMargins(0, 0, 0, 0);
    stepBack = new QPushButton{"<", timeRow};
    playButton = new QPushButton{"Loop", timeRow};
    stepForward = new QPushButton{">", timeRow};
    rangeButton = new QPushButton{"From-to...", timeRow};
    rangeButton->setToolTip("Loop from one time to another (UTC): the layers that can read any stretch of their archive (radar, satellite) show the frames between the two times, every few when it is long; a model chart loops through its run from the first time. Clear goes back to the usual loop");
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
    timeLayout->addWidget(rangeButton);
    hoursButton = new QPushButton{"Hours \xE2\x96\xBE", timeRow};
    hoursButton->setToolTip("A grid of every hour of the forecast (or every frame) to jump to");
    timeLayout->addWidget(hoursButton);
    QObject::connect(hoursButton, &QPushButton::clicked, [this] { openHours(); });
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
    QObject::connect(rangeButton, &QPushButton::clicked, [this] { askRange(); });
    QObject::connect(liveButton, &QPushButton::clicked, [this] { goLive(); });
    QObject::connect(timeSlider, &QSlider::valueChanged, [this] (int value) { if (!settingSlider) { playing = false; playTimer.stop(); playButton->setText("Loop"); goTo(value); announceTime(); } });
    QObject::connect(&HistoryCursor::get(), &HistoryCursor::moved, this, [this] (QObject * origin) { followCursor(origin); });
    QObject::connect(speedCombo, &QComboBox::currentIndexChanged, [this] (int index) {
        Utility::writePrefInt("MASTERMAP_SPEED", index);
        playTimer.setInterval(index == 0 ? 1200 : index == 1 ? 650 : 280);
    });
    playTimer.setInterval(speedCombo->currentIndex() == 0 ? 1200 : speedCombo->currentIndex() == 1 ? 650 : 280);
    QObject::connect(&playTimer, &QTimer::timeout, [this] { advance(); });
    rowMain.addWidgetReal(map, 1, Qt::Alignment{});   // the controls above are laid out first and the map takes what is left
    globeView = new GlobeMapView{this};
    globeView->hide();
    rowMain.addWidgetReal(globeView, 1, Qt::Alignment{});
    rowMain.addWidgetReal(sidePanel, 0, Qt::AlignLeft);   // (the whole height: the manager scrolls)
    ElidedText::install(sidePanel);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidgetReal(timeRow, 0, Qt::Alignment{});
    box.addLayout(rowMain, 1);
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

// The layer manager: the layers that are on, in painting order, each with a check to switch it off, its name and what it has read, and an arrow that opens its options right
// there (they stay open between runs). The rows are made again only when the layers or the open options change; what the layers have read is written into the rows that are there,
// so that a slider being dragged is not taken away from under the hand.
void MasterMapViewer::refreshActiveList() {
    if (activeLayout == nullptr) {
        return;
    }
    string signature;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            signature += layer->id() + (expanded.count(layer->id()) ? "+" : "-") + ";";
        }
    }
    if (signature == activeSignature) {
        for (const auto& layer : layers) {
            if (const auto found = summaryLabels.find(layer->id()); layer->enabled() && found != summaryLabels.end()) {
                const auto text = layer->summary();
                found->second->setText(QString::fromStdString(text));
                found->second->setVisible(!text.empty());
            }
        }
        return;
    }
    activeSignature = signature;
    summaryLabels.clear();
    while (auto * item = activeLayout->takeAt(0)) {
        if (auto * widget = item->widget()) {
            if (widget->property("keep").toBool()) {   // an open option panel: kept for the next time
                widget->hide();
                widget->setParent(nullptr);
            } else {
                widget->deleteLater();
            }
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
        const string id = layer->id();
        const bool open = expanded.count(id) > 0;
        auto * arrow = new QToolButton{row};
        arrow->setArrowType(open ? Qt::DownArrow : Qt::RightArrow);
        arrow->setAutoRaise(true);
        arrow->setToolTip("Options of this layer, here in the manager");
        QObject::connect(arrow, &QToolButton::clicked, [this, id] {
            if (!expanded.erase(id)) {
                expanded.insert(id);
            }
            QStringList list;
            for (const auto& e : expanded) {
                list << QString::fromStdString(e);
            }
            Utility::writePref("MASTERMAP_OPEN_OPTIONS", list.join(',').toStdString());
            QTimer::singleShot(0, this, [this] { refreshActiveList(); });
        });
        rowLayout->addWidget(arrow);
        auto * check = new QCheckBox{QString::fromStdString(layer->path()).section('/', -1), row};
        check->setChecked(true);
        check->setToolTip(QString::fromStdString(layer->tip()) + "\n(uncheck to switch it off)");
        QObject::connect(check, &QCheckBox::clicked, [this, id] (bool on) { setLayerOn(id, on); comboView.setIndex(0); });
        rowLayout->addWidget(check, 1);
        activeLayout->addWidget(row);
        auto * note = new QLabel{QString::fromStdString(layer->summary()), activeBox};
        note->setStyleSheet("color: gray; font-size: 9pt; margin-left: 22px;");
        note->setWordWrap(true);
        note->setVisible(!layer->summary().empty());
        activeLayout->addWidget(note);
        summaryLabels[id] = note;
        if (open) {
            auto found = inlineOptions.find(id);
            if (found == inlineOptions.end()) {
                auto * holder = new QWidget{activeBox};
                holder->setProperty("keep", true);
                auto * holderLayout = new QVBoxLayout{holder};
                holderLayout->setContentsMargins(10, 0, 0, 6);
                MapLayer * target = layer.get();
                if (auto * widget = target->options(holder, [this, target] { target->optionChanged(*this); redraw(); QTimer::singleShot(0, this, [this] { refreshActiveList(); }); })) {
                    holderLayout->addWidget(widget);
                } else {
                    auto * none = new QLabel{"No options for this layer.", holder};
                    none->setEnabled(false);
                    holderLayout->addWidget(none);
                }
                ElidedText::install(holder);
                found = inlineOptions.emplace(id, holder).first;
            }
            activeLayout->addWidget(found->second);
            found->second->show();
        }
    }
    if (shown == 0) {
        auto * none = new QLabel{"Nothing is on: choose a view above, or add layers.", activeBox};
        none->setWordWrap(true);
        none->setEnabled(false);
        activeLayout->addWidget(none);
    }
    activeLayout->addStretch();
    QTimer::singleShot(0, this, [this] {   // the area is as tall as its rows, up to what the window has: it scrolls only beyond that
        activeBox->adjustSize();
        const int wanted = activeBox->sizeHint().height() + 4, room = std::max(120, height() - 300);
        activeScroll->setFixedHeight(std::min(wanted, room));
    });
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

// the layers' option panels go before the layers do (a panel being deleted tells its layer)
MasterMapViewer::~MasterMapViewer() {
    for (auto& [id, holder] : inlineOptions) {
        delete holder;
    }
    inlineOptions.clear();
}

// a grid of every frame on the bar: the forecast hours (F000 ...) when only model charts are on, else the times
void MasterMapViewer::openHours() {
    if (ticks.size() < 2) {
        return;
    }
    const bool forecasts = forecastsOnly();
    auto * popup = new QFrame{this, Qt::Popup};
    popup->setAttribute(Qt::WA_DeleteOnClose);
    popup->setFrameShape(QFrame::StyledPanel);
    auto * grid = new QGridLayout{popup};
    grid->setSpacing(3);
    const int columns = ticks.size() > 80 ? 12 : 8;
    for (size_t i = 0; i < ticks.size(); i++) {
        const auto label = forecasts ? QString{"F%1"}.arg((ticks[i] - ticks.front()) / 3600, 3, 10, QChar{'0'})
                                     : QDateTime::fromSecsSinceEpoch(ticks[i], QTimeZone::utc()).toString("dd HH'Z'");
        auto * b = new QPushButton{label, popup};
        b->setCheckable(true);
        b->setChecked(static_cast<int>(i) == timeSlider->value());
        b->setMinimumWidth(52);
        QObject::connect(b, &QPushButton::clicked, popup, [this, popup, i] {
            playing = false;
            playTimer.stop();
            playButton->setText("Loop");
            goTo(static_cast<int>(i));
            announceTime();
            popup->close();
        });
        grid->addWidget(b, static_cast<int>(i) / columns, static_cast<int>(i) % columns);
    }
    popup->move(hoursButton->mapToGlobal(QPoint{0, hoursButton->height()}));
    popup->show();
}

// only forecasts on the bar (the model charts): "latest" is the first hour of the run, not the last tick
bool MasterMapViewer::forecastsOnly() const {
    bool any = false;
    for (const auto& layer : layers) {
        if (layer->enabled() && layer->timeAware()) {
            if (!layer->forecast()) {
                return false;
            }
            any = true;
        }
    }
    return any;
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
    {   // the members graph is for the REFS charts
        const auto * chart = modelLayer();
        buttonMembers.getView()->setVisible(chart != nullptr && chart->modelName() == "REFS");
    }
    const long keep = !live && wantedTime != 0 ? wantedTime : (!live && timeSlider->value() >= 0 && static_cast<size_t>(timeSlider->value()) < ticks.size() ? ticks[static_cast<size_t>(timeSlider->value())] : 0);
    ticks.assign(all.begin(), all.end());
    settingSlider = true;
    timeSlider->setMaximum(std::max(0, static_cast<int>(ticks.size()) - 1));
    timeSlider->setEnabled(ticks.size() > 1);
    hoursButton->setVisible(ticks.size() > 1);
    int index = forecastsOnly() ? 0 : static_cast<int>(ticks.size()) - 1;
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
    loopFrom = loopTo = 0;
    rangeButton->setText("From-to...");
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
    wantedTime = 0;
    loopFrom = loopTo = 0;
    rangeButton->setText("From-to...");
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
    live = (forecastsOnly() ? index == 0 : static_cast<size_t>(index) + 1 == ticks.size()) && !playing;
    const long t = ticks[static_cast<size_t>(index)];
    wantedTime = t;   // (a chart changed under the bar keeps this hour)
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

// the first and last frame of the loop: all of them, or those between the two times asked for
void MasterMapViewer::loopBounds(int& first, int& last) const {
    if (loopFrom == 0 || ticks.empty()) {
        return;
    }
    const auto from = std::lower_bound(ticks.begin(), ticks.end(), loopFrom);
    const auto to = std::upper_bound(ticks.begin(), ticks.end(), loopTo);
    if (from != ticks.end() && to > from) {
        first = static_cast<int>(from - ticks.begin());
        last = static_cast<int>(to - ticks.begin()) - 1;
    }
}

void MasterMapViewer::askRange() {
    QDialog dialog{this};
    dialog.setWindowTitle("Loop from time to time");
    auto * column = new QVBoxLayout{&dialog};
    column->addWidget(new QLabel{"Loop the frames from one time to another (UTC). Radar and satellite read the whole stretch (every few frames when it is long); a model chart loops from the first time through its run.", &dialog});
    auto * from = new QDateTimeEdit{&dialog};
    auto * to = new QDateTimeEdit{&dialog};
    for (auto * edit : {from, to}) {
        edit->setDisplayFormat("yyyy-MM-dd HH:mm 'Z'");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        edit->setTimeZone(QTimeZone::utc());
#else
        edit->setTimeSpec(Qt::UTC);
#endif
        edit->setCalendarPopup(true);
    }
    const auto now = timePick->dateTime().toUTC();
    from->setDateTime(loopFrom != 0 ? QDateTime::fromSecsSinceEpoch(loopFrom, QTimeZone::utc()) : now.addSecs(-3 * 3600));
    to->setDateTime(loopTo != 0 ? QDateTime::fromSecsSinceEpoch(loopTo, QTimeZone::utc()) : now);
    auto * row1 = new QHBoxLayout;
    row1->addWidget(new QLabel{"From:", &dialog});
    row1->addWidget(from, 1);
    auto * row2 = new QHBoxLayout;
    row2->addWidget(new QLabel{"To:", &dialog});
    row2->addWidget(to, 1);
    column->addLayout(row1);
    column->addLayout(row2);
    auto * buttons = new QHBoxLayout;
    auto * go = new QPushButton{"Loop these times", &dialog};
    auto * clear = new QPushButton{"Clear", &dialog};
    auto * cancel = new QPushButton{"Cancel", &dialog};
    buttons->addWidget(go);
    buttons->addWidget(clear);
    buttons->addWidget(cancel);
    column->addLayout(buttons);
    QObject::connect(go, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(clear, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(2); });
    const int result = dialog.exec();
    if (result == 2) {
        clearRange();
    } else if (result == QDialog::Accepted) {
        auto a = from->dateTime().toUTC(), b = to->dateTime().toUTC();
        if (b < a) {
            std::swap(a, b);
        }
        applyRange(a, b);
    }
}

void MasterMapViewer::clearRange() {
    loopFrom = loopTo = 0;
    rangeButton->setText("From-to...");
    jumpTo(timePick->dateTime().toUTC());
}

void MasterMapViewer::applyRange(const QDateTime& from, const QDateTime& to) {
    following = true;
    live = false;
    playing = false;
    playWhenReady = false;
    playTimer.stop();
    playButton->setText("Loop");
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            layer->setRange(from, to, *this);
        }
    }
    loopFrom = static_cast<long>(from.toSecsSinceEpoch());
    loopTo = static_cast<long>(to.toSecsSinceEpoch());
    rangeButton->setText(from.toString("MM-dd HH") + "Z to " + to.toString("MM-dd HH") + "Z");
    rebuildTicks();
    int first = 0, last = static_cast<int>(ticks.size()) - 1;
    loopBounds(first, last);
    live = false;
    goTo(first);
    live = false;
    extra = "loop " + from.toString("yyyy-MM-dd HH:mm").toStdString() + "Z to " + to.toString("yyyy-MM-dd HH:mm").toStdString() + "Z";
    updateStatus();
    setTitle("Master map - " + extra);
    for (auto& layer : layers) {   // the frames of the stretch are read now, and the loop starts when they are there
        if (layer->enabled() && layer->timeAware()) {
            layer->prepareTimes(*this);
        }
    }
    togglePlay();
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
    int first = 0, last = static_cast<int>(ticks.size()) - 1;
    loopBounds(first, last);
    if (timeSlider->value() >= last || timeSlider->value() < first) {
        timeSlider->setValue(first);
    }
    playing = true;
    playButton->setText("Stop");
    playTimer.start();
}

void MasterMapViewer::advance() {
    if (ticks.size() < 2) {
        return;
    }
    int first = 0, last = static_cast<int>(ticks.size()) - 1;
    loopBounds(first, last);
    int next = timeSlider->value() + 1;
    if (next > last || next < first) {
        next = first;   // round again, with a pause at the newest
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
    mapView->useLayoutSpace();   // (the layout gives the map its size, after the controls)
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

// the hours of the model chart as a loop (GIF or video) of the Model Viewer's titled pictures, for the part of the world in view
void MasterMapViewer::saveModelLoop() {
    auto * chart = modelLayer();
    if (chart == nullptr || chart->hourList().empty()) {
        return;
    }
    const auto& hours = chart->hourList();
    QDialog dialog{this};
    dialog.setWindowTitle("Save a loop");
    auto * column = new QVBoxLayout{&dialog};
    column->addWidget(new QLabel{"The forecast hours to put in the loop (each is drawn as the titled picture, for the part of the world in view):", &dialog});
    auto * first = new QComboBox{&dialog};
    auto * last = new QComboBox{&dialog};
    for (const int h : hours) {
        first->addItem("F" + QString::number(h), h);
        last->addItem("F" + QString::number(h), h);
    }
    const int now = chart->hourNow();
    first->setCurrentIndex(std::max(0, first->findData(now)));
    int end = now;
    for (const int h : hours) {
        if (h >= now && h <= now + 24) {
            end = h;
        }
    }
    last->setCurrentIndex(std::max(0, last->findData(end)));
    auto * row = new QHBoxLayout;
    row->addWidget(new QLabel{"From", &dialog});
    row->addWidget(first);
    row->addWidget(new QLabel{"to", &dialog});
    row->addWidget(last);
    column->addLayout(row);
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const int from = first->currentData().toInt(), to = last->currentData().toInt();
    std::vector<int> chosen;
    for (const int h : hours) {
        if (h >= from && h <= to) {
            chosen.push_back(h);
        }
    }
    if (chosen.empty()) {
        return;
    }
    QProgressDialog progress{"Drawing the pictures of the loop...", "Cancel", 0, static_cast<int>(chosen.size()), this};
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    std::vector<QByteArray> frames;
    string problem;
    for (size_t i = 0; i < chosen.size(); i++) {
        progress.setValue(static_cast<int>(i));
        progress.setLabelText(QString{"Drawing F%1 (%2 of %3)..."}.arg(chosen[i]).arg(i + 1).arg(chosen.size()));
        QCoreApplication::processEvents();
        if (progress.wasCanceled()) {
            return;
        }
        QImage image;
        if (!chart->titled(*this, image, problem, chosen[i])) {
            QMessageBox::warning(this, "Save a loop", QString::fromStdString("F" + std::to_string(chosen[i]) + ": " + problem));
            return;
        }
        QByteArray bytes;
        QBuffer buffer{&bytes};
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        frames.push_back(bytes);
    }
    progress.setValue(static_cast<int>(chosen.size()));
    const int interval = speedCombo->currentIndex() == 0 ? 1200 : speedCombo->currentIndex() == 1 ? 650 : 280;
    const auto name = QString::fromStdString(chart->modelName() + "_loop_F" + std::to_string(from) + "-F" + std::to_string(to));
    UtilityAnimationExport::saveWithDialog(this, frames.size() >= 2 ? frames : std::vector<QByteArray>{}, interval, frames.front(), name, QByteArray{}, false, name);
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

// the whole world on an equal-area projection as one picture (the layers painted into a skin of the world, then projected), with the header, the legend and the credits
void MasterMapViewer::exportWorld() {
    QDialog dialog{this};
    dialog.setWindowTitle("Export the whole world");
    auto * column = new QVBoxLayout{&dialog};
    column->addWidget(new QLabel{"The layers as they are on the map, over the whole world, with nothing stretched toward the poles as on Mercator: Robinson is the compromise the United Nations' world maps use; Equal Earth and Mollweide keep areas true.", &dialog});
    auto * projectionBox = new QComboBox{&dialog};
    projectionBox->addItem("Robinson (the United Nations' world maps)", static_cast<int>(GlobeMapView::Projection::Robinson));
    projectionBox->addItem("Equal Earth (equal area)", static_cast<int>(GlobeMapView::Projection::EqualEarth));
    projectionBox->addItem("Mollweide (equal area)", static_cast<int>(GlobeMapView::Projection::Mollweide));
    projectionBox->addItem("North polar (the pole to 20 N: the polar vortex)", static_cast<int>(GlobeMapView::Projection::PolarNorth));
    projectionBox->addItem("South polar (the pole to 20 S)", static_cast<int>(GlobeMapView::Projection::PolarSouth));
    auto * centerBox = new QComboBox{&dialog};
    centerBox->addItem("Centered on Greenwich (0)", 0);
    centerBox->addItem("Centered on the Americas (-100)", -100);
    centerBox->addItem("Centered on the Pacific (180)", 180);
    centerBox->addItem("Centered on Asia (90)", 90);
    auto * widthBox = new QComboBox{&dialog};
    const auto standardWidths = [widthBox] {
        widthBox->clear();
        for (const int w : {2400, 3600, 4800, 6000}) {
            widthBox->addItem(QString::number(w) + " pixels wide", w);
        }
        widthBox->setCurrentIndex(1);
    };
    standardWidths();
    // the polar maps: how far from the pole the sides reach, and a circle (the hemisphere) or a wide frame (as a polar vortex chart)
    auto * reachBox = new QComboBox{&dialog};
    for (const int lat : {10, 20, 30, 40, 50}) {
        reachBox->addItem(QString{"Reaches to %1 degrees latitude"}.arg(lat), lat);
    }
    reachBox->setCurrentIndex(2);
    auto * frameBox = new QComboBox{&dialog};
    frameBox->addItem("Wide frame, filled to the corners (a polar vortex chart)", 1);
    frameBox->addItem("Circle (the whole hemisphere to that latitude)", 0);
    auto * graticuleBox = new QCheckBox{"Lines of latitude and longitude, with their numbers", &dialog};
    graticuleBox->setChecked(true);
    const auto polarControls = [=] {
        const bool polar = GlobeMapView::isPolar(static_cast<GlobeMapView::Projection>(projectionBox->currentData().toInt()));
        reachBox->setVisible(polar);
        frameBox->setVisible(polar);
    };
    QObject::connect(projectionBox, &QComboBox::currentIndexChanged, [=] (int) { polarControls(); });
    column->addWidget(projectionBox);
    column->addWidget(reachBox);
    column->addWidget(frameBox);
    column->addWidget(centerBox);
    column->addWidget(widthBox);
    column->addWidget(graticuleBox);
    polarControls();
    // high resolution: the model chart (or the combo) painted from the model grids themselves, as big as the finest model has detail
    string specChart;
    vector<string> specModels;
    QDateTime specWhen;
    QCheckBox * highBox = nullptr;
    int nativeWidth = 0;
    if (modelSpec(specChart, specModels, specWhen)) {
        double finest = 1e9;
        for (const auto& name : specModels) {
            if (const double km = ComboChartLayer::kmOf(name); km > 0.0) {
                finest = std::min(finest, km);
            }
        }
        // one pixel for a grid cell along the equator: the width beyond which zooming in shows nothing new
        nativeWidth = finest < 1e8 ? std::clamp(static_cast<int>(std::lround(360.0 * 111.32 / finest / 500.0)) * 500, 4000, 16000) : 10000;
        highBox = new QCheckBox{QString{"High resolution: the model data at its own resolution (finest here %1 km), the model chart only, no other layers"}.arg(finest < 1e8 ? QString::number(finest, 'f', 1) : QString{"?"}), &dialog};
        column->addWidget(highBox);
        QObject::connect(highBox, &QCheckBox::toggled, [=] (bool on) {
            if (!on) {
                standardWidths();
                return;
            }
            widthBox->clear();
            widthBox->addItem(QString{"Native: %1 pixels wide (as much detail as the models have)"}.arg(nativeWidth), nativeWidth);
            for (const int w : {8000, 10000, 12000, 16000}) {
                if (w != nativeWidth) {
                    widthBox->addItem(QString::number(w) + " pixels wide", w);
                }
            }
            widthBox->setCurrentIndex(0);
        });
    }
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    column->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    const auto projection = static_cast<GlobeMapView::Projection>(projectionBox->currentData().toInt());
    const double centre = centerBox->currentData().toDouble();
    const int width = widthBox->currentData().toInt();
    const auto path = QFileDialog::getSaveFileName(this, "Export the whole world", ChartExport::startFolder() + "/" + ChartExport::fileName("world map", "png"), "PNG image (*.png)");
    if (path.isEmpty()) {
        return;
    }
    if (GlobeMapView::isPolar(projection)) {
        GlobeMapView::setPolarFrame(reachBox->currentData().toDouble(), frameBox->currentData().toInt() == 1);
    }
    exportWorldTo(path, projection, centre, width, highBox != nullptr && highBox->isChecked(), graticuleBox->isChecked());
}

// what the model chart (or the combo) on the map is: the chart, the models of the stack, the valid time
bool MasterMapViewer::modelSpec(string& chart, vector<string>& models, QDateTime& when) const {
    chart.clear();
    models.clear();
    for (const auto& layer : layers) {
        if (layer->enabled() && layer->id() == "model/combo") {
            const auto * combo = static_cast<ComboChartLayer *>(layer.get());
            chart = combo->chart();
            models = combo->modelNames();
            when = combo->validTime();
        }
    }
    if (chart.empty()) {
        if (const auto * one = modelLayer()) {
            chart = one->chartId();
            models = {one->modelName()};
            when = one->validTime();
        }
    }
    if (chart.empty()) {
        return false;
    }
    if (!when.isValid()) {
        when = timePick->dateTime().toUTC();
    }
    bool global = false;   // a regional model alone gets a global one under it
    for (const auto& m : models) {
        global = global || ComboChartLayer::kmOf(m) >= 20.0;
    }
    if (!global) {
        for (const char * base : {"IFS", "GFS"}) {
            if (GfsChart::product(chart, base) != nullptr) {
                models.push_back(base);
                break;
            }
        }
    }
    return true;
}

void MasterMapViewer::exportWorldTo(const QString& path, GlobeMapView::Projection projection, double centre, int width, bool highResolution, bool graticule) {
    status("drawing the world ...");
    QCoreApplication::processEvents();
    QImage world;
    string highNote;
    if (highResolution) {
        string chart;
        vector<string> models;
        QDateTime when;
        if (!modelSpec(chart, models, when)) {
            QMessageBox::warning(this, "Export the whole world", "Put the model chart (or the combo) on the map for a high resolution picture.");
            return;
        }
        // the models' grids (a download the first time), then the pixels
        std::vector<GlobeViewer::Source> sources;
        {
            QEventLoop loop;
            status("reading the models' fields ...");
            new FutureVoid{this, [&] { sources = GlobeViewer::loadSources(chart, models, when); }, [&loop] { loop.quit(); }};
            loop.exec();
        }
        if (sources.empty()) {
            QMessageBox::warning(this, "Export the whole world", "No model has this chart for that time.");
            return;
        }
        std::atomic<int> rows{0};
        double halfW, halfH;
        GlobeMapView::extent(projection, halfW, halfH);
        const int totalRows = static_cast<int>(std::lround(width * halfH / halfW));
        {
            QEventLoop loop;
            QTimer ticker;
            QObject::connect(&ticker, &QTimer::timeout, [&] { status("painting the world: " + std::to_string(rows.load() * 100 / std::max(1, totalRows)) + " %"); });
            ticker.start(500);
            new FutureVoid{this, [&] { world = GlobeViewer::renderWorld(sources, static_cast<int>(projection), centre, width, &rows); }, [&loop] { loop.quit(); }};
            loop.exec();
        }
        for (const auto& s : sources) {
            highNote += (highNote.empty() ? "" : ", ") + s.model + " " + s.run.substr(4) + "Z F" + std::to_string(s.hour);
        }
    } else {
        const auto skin = makeSkin(-85.0, 85.0, centre - 180.0, centre + 180.0, std::min(6000, static_cast<int>(width * 1.4)), 1300.0 / 360.0, true);   // (marks spaced as on a world map 1300 pixels wide)
        world = GlobeMapView::projectWorld(skin, projection, centre, width);
    }
    if (graticule) {
        QPainter linePainter{&world};
        if (GlobeMapView::isPolar(projection) && !GlobeMapView::polarWide()) {   // a circle: nothing outside it
            QPainterPath disc;
            disc.addEllipse(QRectF{0.0, 0.0, static_cast<double>(world.width()), static_cast<double>(world.height())});
            linePainter.setClipPath(disc);
        }
        GlobeMapView::drawGraticule(linePainter, projection, centre, world.width(), world.height(), std::max(1.0, world.width() / 2800.0));
    }
    // the pieces around it, as the flat export has them
    QStringList names, times, sources;
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
    vector<MapLegendRow> rows;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            for (auto& row : layer->legend()) {
                rows.push_back(std::move(row));
            }
        }
    }
    const double scale = width / 1000.0;
    const bool combo = std::any_of(layers.begin(), layers.end(), [] (const auto& layer) { return layer->enabled() && layer->id() == "model/combo"; });
    const int more = combo ? static_cast<int>(18 * scale) : 0;   // (a line for the models the picture rests on)
    const int header = static_cast<int>(66 * scale) + more, footer = static_cast<int>(38 * scale), legendHeight = rows.empty() ? 0 : static_cast<int>(40 * scale);
    QImage image(width, header + world.height() + legendHeight + footer, QImage::Format_ARGB32);
    image.fill(QColor{255, 255, 255});
    {
        QPainter painter{&image};
        QFont base{painter.font()};
        base.setPixelSize(static_cast<int>(13 * scale));
        QFont bold{base};
        bold.setBold(true);
        bold.setPixelSize(static_cast<int>(17 * scale));
        const int pad = static_cast<int>(8 * scale);
        painter.setPen(QColor{25, 25, 25});
        painter.setFont(bold);
        painter.drawText(QRect{pad, pad / 2, width - 2 * pad, static_cast<int>(24 * scale)}, Qt::AlignVCenter | Qt::AlignLeft, (projection == GlobeMapView::Projection::PolarNorth || projection == GlobeMapView::Projection::PolarSouth ? QString{} : QString{"World, "}) + GlobeMapView::projectionName(projection));
        painter.setFont(base);
        painter.setPen(QColor{60, 60, 60});
        if (combo) {   // the two models the picture rests on first, the rest as detail
            QFont strong{base};
            strong.setBold(true);
            strong.setPixelSize(static_cast<int>(15 * scale));
            painter.setFont(strong);
            painter.setPen(QColor{20, 60, 120});
            painter.drawText(QRect{pad, static_cast<int>(27 * scale), width - 2 * pad, static_cast<int>(18 * scale)}, Qt::AlignVCenter | Qt::AlignLeft, "World: GFS 25 km     North America: RRFS 3 km");
            painter.setFont(base);
            painter.setPen(QColor{60, 60, 60});
            names.removeAll(QString{"Best resolution combo (every model that has the chart, the finest on top)"});
            names.prepend("finer models where they exist");
        }
        painter.drawText(QRect{pad, static_cast<int>(27 * scale) + more, width - 2 * pad, static_cast<int>(18 * scale)}, Qt::AlignVCenter | Qt::AlignLeft, names.isEmpty() ? QString{"No layers on"} : (highNote.empty() ? "Layers: " + names.join(", ") : QString{"The model data at its own resolution, the finest model on top (the runs are in the line below)"}));
        painter.drawText(QRect{pad, static_cast<int>(45 * scale) + more, width - 2 * pad, static_cast<int>(18 * scale)}, Qt::AlignVCenter | Qt::AlignLeft, times.isEmpty() ? QString{"Valid now"} : "Valid: " + times.join("   "));
        painter.drawImage(0, header, world);
        if (!rows.empty()) {   // the legend, in window units as on the map
            QImage legend(width, legendHeight, QImage::Format_ARGB32_Premultiplied);
            legend.fill(QColor{255, 255, 255});
            {
                QPainter legendPainter{&legend};
                legendPainter.setWindow(QRect{-500, -250, 1000, std::max(1, static_cast<int>(std::lround(1000.0 * legendHeight / width)))});
                MapLegend::draw(legendPainter, rows, 1.0, -250.0 + 1000.0 * legendHeight / width - 2.0);   // (the 1000 unit wide window as a screen 1000 pixels wide: the legend as big as on the map, in proportion)
            }
            painter.drawImage(0, header + world.height(), legend);
        }
        const int footTop = header + world.height() + legendHeight;
        painter.fillRect(QRect{0, footTop, width, footer}, QColor{255, 255, 255});
        QFont small{base};
        small.setPixelSize(static_cast<int>(11 * scale));
        painter.setFont(small);
        painter.setPen(QColor{100, 100, 100});
        painter.drawText(QRect{pad, footTop, width - 2 * pad, footer}, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextWordWrap,
            (highNote.empty() ? QString{} : QString::fromStdString("Runs: " + highNote + "  -  ")) + (sources.isEmpty() ? QString{} : "Data: " + sources.join("; ") + "  -  ") + "saved " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + " UTC  -  exported by wxqt-grib");
    }
    if (!image.save(path, "PNG")) {
        QMessageBox::warning(this, "Export the whole world", "The file could not be written: " + path);
        return;
    }
    ChartExport::remember(path);
    status("Saved " + path.toStdString());
}

void MasterMapViewer::exportView() {
    {   // the view as it is, or the whole world on an equal-area projection
        QMessageBox ask{this};
        ask.setWindowTitle("Export");
        ask.setText("Save the view as it is on the screen, or the whole world on a world map projection (Robinson, Equal Earth, Mollweide: nothing stretched toward the poles)?");
        auto * viewButton = ask.addButton("The view", QMessageBox::AcceptRole);
        auto * worldButton = ask.addButton("The whole world", QMessageBox::ActionRole);
        ask.addButton(QMessageBox::Cancel);
        ask.exec();
        if (ask.clickedButton() == worldButton) {
            exportWorld();
            return;
        }
        if (ask.clickedButton() != viewButton) {
            return;
        }
    }
    // A model chart on the map can be saved as the Model Viewer's own picture of it (its title with the model, chart, run and valid times, the legend and the credit), for what is in view
    for (const auto& layer : layers) {
        if (layer->id() != "model/chart" || !layer->enabled()) {
            continue;
        }
        QMessageBox ask{this};
        ask.setWindowTitle("Export the view");
        ask.setText("A model chart is on the map. Save it as the Model Viewer's picture (with its title, run and valid times and legend), or the map as it is?");
        auto * titledButton = ask.addButton("Model chart, titled", QMessageBox::AcceptRole);
        auto * loopButton = ask.addButton("Loop of hours, titled", QMessageBox::ActionRole);
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
        if (ask.clickedButton() == loopButton) {
            saveModelLoop();
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
    mapView->useLayoutSpace();   // (the layout gives the map its size, after the controls)
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

// ---- views of your own ----
// the preference holds one view a line: name, layers (comma), region (four numbers) and the model chart's settings, tab separated

vector<MasterMapViewer::SavedView> MasterMapViewer::savedViews() const {
    vector<SavedView> out;
    const auto text = QString::fromStdString(Utility::readPref("MASTERMAP_SAVED_VIEWS", ""));
    for (const auto& line : text.split('\n', Qt::SkipEmptyParts)) {
        const auto f = line.split('\t');
        if (f.size() < 4) {
            continue;
        }
        SavedView v;
        v.name = f[0].toStdString();
        for (const auto& id : f[1].split(',', Qt::SkipEmptyParts)) {
            v.layers.push_back(id.toStdString());
        }
        const auto r = f[2].split(',');
        if (r.size() < 4) {
            continue;
        }
        v.minLat = r[0].toDouble();
        v.maxLat = r[1].toDouble();
        v.minLon = r[2].toDouble();
        v.maxLon = r[3].toDouble();
        v.model = f[3].toStdString();
        out.push_back(std::move(v));
    }
    return out;
}

void MasterMapViewer::storeViews(const vector<SavedView>& list) {
    QStringList lines;
    for (const auto& v : list) {
        QStringList ids;
        for (const auto& id : v.layers) {
            ids << QString::fromStdString(id);
        }
        lines << QString::fromStdString(v.name) + '\t' + ids.join(',') + '\t' + QString{"%1,%2,%3,%4"}.arg(v.minLat).arg(v.maxLat).arg(v.minLon).arg(v.maxLon) + '\t' + QString::fromStdString(v.model);
    }
    Utility::writePref("MASTERMAP_SAVED_VIEWS", lines.join('\n').toStdString());
    refreshViewList();
}

// the box of views: yours, then the ones that come with the program
void MasterMapViewer::refreshViewList() {
    vector<string> names{"Your layers"};
    for (const auto& preset : MapCatalog::presets()) {
        names.push_back(preset.name);
    }
    for (const auto& v : savedViews()) {
        names.push_back("* " + v.name);
    }
    comboView.block();
    comboView.setList(names);
    comboView.setIndex(0);
    comboView.unblock();
}

void MasterMapViewer::viewsMenu() {
    QMenu menu{this};
    auto * save = menu.addAction("Keep this view...");
    const auto kept = savedViews();
    QMenu * remove = menu.addMenu("Forget a view");
    remove->setEnabled(!kept.empty());
    for (const auto& v : kept) {
        remove->addAction(QString::fromStdString(v.name))->setData(QString::fromStdString(v.name));
    }
    const auto * chosen = menu.exec(QCursor::pos());
    if (chosen == nullptr) {
        return;
    }
    if (chosen == save) {
        bool ok = false;
        const auto name = QInputDialog::getText(this, "Keep this view", "A name for the view:", QLineEdit::Normal, "", &ok).trimmed().toStdString();
        if (!ok || name.empty()) {
            return;
        }
        SavedView v;
        v.name = name;
        for (const auto& layer : layers) {
            if (layer->enabled()) {
                v.layers.push_back(layer->id());
            }
        }
        const auto * widget = mapView->map();
        const auto topLeft = mapView->toLatLon(QPointF{0.0, 0.0});
        const auto bottomRight = mapView->toLatLon(QPointF{static_cast<double>(widget->width()), static_cast<double>(widget->height())});
        v.maxLat = topLeft.first;
        v.minLat = bottomRight.first;
        v.minLon = topLeft.second;
        v.maxLon = bottomRight.second;
        if (v.maxLon <= v.minLon) {   // across the date line: the longitudes as they go on
            v.maxLon += 360.0;
        }
        if (const auto * chart = modelLayer()) {
            v.model = chart->settings();
        }
        auto list = savedViews();
        list.erase(std::remove_if(list.begin(), list.end(), [&name] (const SavedView& old) { return old.name == name; }), list.end());
        list.push_back(v);
        storeViews(list);
        status("Kept the view " + name);
    } else if (chosen->data().isValid()) {
        const auto name = chosen->data().toString().toStdString();
        auto list = savedViews();
        list.erase(std::remove_if(list.begin(), list.end(), [&name] (const SavedView& old) { return old.name == name; }), list.end());
        storeViews(list);
    }
}

void MasterMapViewer::applySaved(const SavedView& v) {
    building = true;
    for (const auto& layer : layers) {
        const bool want = std::find(v.layers.begin(), v.layers.end(), layer->id()) != v.layers.end();
        if (auto * item = items.count(layer->id()) != 0 ? items[layer->id()] : nullptr) {
            item->setCheckState(0, want ? Qt::Checked : Qt::Unchecked);
        }
        if (want && layer->id() == "model/chart" && !v.model.empty()) {
            static_cast<ModelChartLayer *>(layer.get())->setSettings(v.model, *this);
        }
        setLayerOn(layer->id(), want);
    }
    building = false;
    mapView->showRegion(v.minLat, v.maxLat, v.minLon, v.maxLon);
    updateStatus();
}

// the picture of a region of the world as the layers paint it (Mercator, at a width), for the globe's skin
GlobeMapView::Skin MasterMapViewer::makeSkin(double minLat, double maxLat, double minLon, double maxLon, int width, double referencePixelsPerDegree, bool waitForLayers) {
    GlobeMapView::Skin skin;
    // the labels and marks that thin themselves (claimCell) are spaced as they would be at the reference scale, whatever the picture's size: automatic thinning
    thinning = referencePixelsPerDegree > 0.0 ? std::max(1.0, (width / std::max(1.0, maxLon - minLon)) / referencePixelsPerDegree) : 1.0;
    const auto saved = mapView->saveState();
    mapView->setRenderSize(width, width);
    mapView->showRegion(minLat, maxLat, minLon, maxLon);
    const auto first = mapView->transform();
    const double mercatorSpan = MapView::mercator(std::min(85.0, maxLat)) - MapView::mercator(std::max(-85.0, minLat)), lonSpan = std::max(1.0, maxLon - minLon);
    const int height = std::clamp(static_cast<int>(std::lround(width * (std::abs(first.ay) * mercatorSpan) / (std::abs(first.ax) * lonSpan))), 256, 6000);
    mapView->setRenderSize(width, height);
    mapView->showRegion(minLat, maxLat, minLon, maxLon);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    const auto paintImage = [&] {
        image.fill(RadarPreferences::mapBackgroundColor);
        QPainter painter{&image};
        painter.setWindow(QRect{-500, -250, 1000, std::max(1, static_cast<int>(std::lround(1000.0 * height / width)))});
        paintMap(painter);
    };
    const auto anyWorking = [this] {
        for (const auto& layer : layers) {
            if (layer->enabled() && layer->working()) {
                return true;
            }
        }
        return false;
    };
    paintImage();
    // the model charts are drawn for the view on screen: a picture of the whole world waits until they have drawn the whole world
    for (int round = 0; waitForLayers && round < 2 && anyWorking(); round++) {
        QEventLoop loop;
        QTimer poll;
        QTimer limit;
        limit.setSingleShot(true);
        QObject::connect(&poll, &QTimer::timeout, [&] { if (!anyWorking()) { loop.quit(); } });
        QObject::connect(&limit, &QTimer::timeout, &loop, &QEventLoop::quit);
        poll.start(200);
        limit.start(180000);
        status(round == 0 ? "the models are drawing the whole world ..." : "the models are drawing the whole world (the finer ones) ...");
        loop.exec();
        paintImage();   // (the layers ask again for what the picture still lacks)
    }
    skin.image = std::move(image);
    skin.transform = mapView->transform();
    skin.state = mapView->saveState();
    skin.width = width;
    skin.height = height;
    skin.west = minLon;
    skin.east = maxLon;
    skin.south = minLat;
    skin.north = maxLat;
    mapView->setRenderSize(0, 0);
    mapView->restoreState(saved);
    thinning = 1.0;
    return skin;
}

// the map or the globe
void MasterMapViewer::setGlobe(bool on) {
    globeOn = on;
    buttonGlobe.getView()->setText(on ? "Flat map" : "Globe");
    mapView->map()->setVisible(!on);
    globeView->setVisible(on);
    if (!on) {
        redraw();
        return;
    }
    const auto * widget = mapView->map();
    const auto middle = mapView->toLatLon(QPointF{widget->width() / 2.0, widget->height() / 2.0});
    globeView->makeSkin = [this] (double a, double b, double c, double d, int w, double reference) { return makeSkin(a, b, c, d, w, reference); };
    globeView->pick = [this] (const GlobeMapView::Skin& skin, double lat, double lon) {
        const auto saved = mapView->saveState();
        mapView->restoreState(skin.state);
        mapView->setRenderSize(skin.width, skin.height);
        double lonSkin = lon;
        const double centre = (skin.west + skin.east) / 2.0;
        while (lonSkin - centre > 180.0) {
            lonSkin -= 360.0;
        }
        while (lonSkin - centre < -180.0) {
            lonSkin += 360.0;
        }
        const auto hit = bestHit(mapView->toPixels(lat, lonSkin));
        mapView->setRenderSize(0, 0);
        mapView->restoreState(saved);
        return hit;
    };
    globeView->paintLegend = [this] (QPainter& painter, int width, int height) {
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
        if (rows.empty()) {
            return;
        }
        QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        {
            QPainter legendPainter{&image};
            legendPainter.setWindow(QRect{-500, -250, 1000, std::max(1, static_cast<int>(std::lround(1000.0 * height / width)))});
            MapLegend::draw(legendPainter, rows, 1000.0 / std::max(1, width), -250.0 + 1000.0 * height / std::max(1, width) - 2.0);
        }
        painter.drawImage(0, 0, image);
    };
    globeView->setCenter(middle.first, middle.second);
}

void MasterMapViewer::openCompare() {
    const auto * widget = mapView->map();
    const auto top = mapView->toLatLon(QPointF{widget->width() / 2.0, 0.0});
    const auto bottom = mapView->toLatLon(QPointF{widget->width() / 2.0, static_cast<double>(widget->height())});
    const auto middle = mapView->toLatLon(QPointF{widget->width() / 2.0, widget->height() / 2.0});
    // the view's height in kilometers, as openAt's radius takes it (it shows 2.4 radii tall)
    const double km = std::max(100.0, std::abs(top.first - bottom.first) * 111.0 / 2.4);
    const auto * chart = modelLayer();
    auto when = chart != nullptr ? chart->validTime() : QDateTime{};
    if (!when.isValid()) {
        when = timePick->dateTime().toUTC();
    }
    auto * other = new MasterMapViewer{this};
    other->openAt(when, middle.first, middle.second, km, {"model/chart"});
    // side by side: this window takes the left half of its screen, the other the right half
    if (const auto * screen = this->screen()) {
        const auto area = screen->availableGeometry();
        showNormal();
        setGeometry(area.left(), area.top() + 30, area.width() / 2, area.height() - 40);
        other->setGeometry(area.left() + area.width() / 2, area.top() + 30, area.width() / 2, area.height() - 40);
    }
}

void MasterMapViewer::chooseArea() {
    std::vector<ProductPicker::Entry> entries;
    const auto places = Location::getListLatLons();
    const auto names = Location::listOfNames();
    const double km = std::clamp(Utility::readPrefInt("MODEL_LOCAL_KM", 600), 100, 5000);
    for (size_t i = 0; i < places.size(); i++) {
        const string name = i < names.size() && !names[i].empty() ? names[i] : string{"Saved location"};
        entries.push_back({GfsChart::localId(places[i].lat(), places[i].lon(), km, name), name + ", " + std::to_string(static_cast<int>(km)) + " km square", "Around a saved location"});
    }
    for (const auto& sector : GfsChart::sectors()) {
        entries.push_back({sector.id, sector.id, ProductPicker::sectorGroup(sector.id)});
    }
    static const std::vector<string> order{"Around a saved location", "United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Other"};
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    auto * picker = new ProductPicker{this, "areas", entries, "", {}, {}, {}};
    picker->setAttribute(Qt::WA_DeleteOnClose);
    picker->setWording("areas", "Show this area");
    picker->resize(360, 560);
    picker->onPick = [this] (const string& id) {
        double lat = 0, lon = 0, size = 0;
        string name;
        if (GfsChart::parseLocalId(id, lat, lon, size, name)) {
            const double degrees = size / 111.0 / 2.0;
            const double lonDegrees = degrees / std::max(0.2, std::cos(lat * 3.14159265358979 / 180.0));
            mapView->showRegion(lat - degrees, lat + degrees, lon - lonDegrees, lon + lonDegrees);
        } else if (const auto * sector = GfsChart::sector(id)) {
            mapView->showRegion(sector->south, sector->north, sector->west, sector->east);
        }
        redraw();
    };
    picker->show();
}

bool MasterMapViewer::pointFor(double& lat, double& lon) const {
    lat = pointerLat;
    lon = pointerLon;
    if (!havePointer) {   // no pointer yet: the middle of the map
        const auto * widget = mapView->map();
        const auto middle = mapView->toLatLon(QPointF{widget->width() / 2.0, widget->height() / 2.0});
        lat = middle.first;
        lon = middle.second;
    }
    return havePointer;
}

ModelChartLayer * MasterMapViewer::modelLayer() const {
    for (const auto& layer : layers) {
        if (layer->id() == "model/chart" && layer->enabled()) {
            return static_cast<ModelChartLayer *>(layer.get());
        }
    }
    return nullptr;
}

MasterMapViewer * MasterMapViewer::openModels(Window * parent, const string& modelWanted, const string& paramWanted, const string& sectorWanted, const QDateTime& valid) {
    // the choices the model page kept (its thumbnail on the home screen says what they are) are the starting point
    string model = modelWanted.empty() ? Utility::readPref("NCEP", Utility::readPref("MM_MODELCHART_MODEL", "GFS")) : modelWanted;
    if (GfsModels::find(model) == nullptr || !GfsRender::drawsModel(model)) {
        model = "GFS";
    }
    string param = paramWanted;
    if (param.empty()) {
        const auto last = Utility::readPref("MODELNCEPPARAMLASTUSED", "");
        param = GfsChart::product(last, model) != nullptr ? last : (model == Utility::readPref("MM_MODELCHART_MODEL", "GFS") ? Utility::readPref("MM_MODELCHART_PARAM", "500_wnd_ht") : string{});
    }
    if (GfsChart::product(param, model) == nullptr) {
        param.clear();
        for (const auto& product : GfsChart::products()) {
            if (product.source == model && !product.id.empty() && product.id.compare(0, 4, "gen~") != 0) {
                param = product.id;
                break;
            }
        }
    }
    Utility::writePref("MM_MODELCHART_MODEL", model);
    if (!param.empty()) {
        Utility::writePref("MM_MODELCHART_PARAM", param);
    }
    Utility::writePref("MM_MODELCHART_OVERLAYS", "");
    Utility::writePref("MM_MODELCHART_OVERLAYMODELS", "");
    auto * map = new MasterMapViewer{parent};
    map->onlyLayer("model/chart");
    string sector = sectorWanted.empty() ? Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS") : sectorWanted;
    double lat = 0, lon = 0, km = 0;
    string name;
    if (GfsChart::parseLocalId(sector, lat, lon, km, name)) {
        const double degrees = km / 111.0 / 2.0;
        const double lonDegrees = degrees / std::max(0.2, std::cos(lat * 3.14159265358979 / 180.0));
        map->mapView->showRegion(lat - degrees, lat + degrees, lon - lonDegrees, lon + lonDegrees);
    } else if (const auto * found = GfsChart::sector(sector)) {
        map->mapView->showRegion(found->south, found->north, found->west, found->east);
    } else {
        map->mapView->showRegion(20.0, 55.0, -130.0, -60.0);
    }
    if (valid.isValid()) {
        map->jumpTo(valid.toUTC());
    }
    return map;
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
    textStatus.getView()->setToolTip(QString::fromStdString(text));
    // what layers did to the choices made (amber, in the side panel)
    string notices;
    for (const auto& layer : layers) {
        if (layer->enabled() && !layer->notice().empty()) {
            notices += (notices.empty() ? "" : "\n") + layer->notice();
        }
    }
    if (noticeLabel != nullptr) {
        noticeLabel->setText(QString::fromStdString(notices));
        noticeLabel->setVisible(!notices.empty());
    }
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
    spacing *= thinning;   // a picture made for the globe or an export is finer than the screen: the marks keep the spacing, in degrees, they have on screen
    const long long columns = static_cast<long long>(std::ceil(mapView->pixelWidth() / spacing)) + 2;
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

// The world repeats east and west, as a web map's does: the layers and the coastlines are drawn once for each copy of the world that shows
void MasterMapViewer::paintMap(QPainter& painter) {
    cells.clear();
    const auto middle = mapView->transform();
    const double a = middle(0.0, -180.0).x(), b = middle(0.0, 180.0).x();
    const double low = std::min(a, b), width = std::max(1.0, std::abs(b - a));
    const int first = std::clamp(static_cast<int>(std::ceil((-540.0 - (low + width)) / width)), -8, 8), last = std::clamp(static_cast<int>(std::floor((540.0 - low) / width)), -8, 8);
    for (int copy = first; copy <= last; copy++) {
        mapView->setWorldCopy(copy);
        paintMapCopy(painter);
    }
    mapView->setWorldCopy(0);
}

void MasterMapViewer::paintMapCopy(QPainter& painter) {
    const auto t = mapView->transform();
    const double px = mapView->unitsPerPixel();
    painter.setRenderHint(QPainter::Antialiasing, true);
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

MapHit MasterMapViewer::bestHit(const QPointF& at) const {
    // the pointer may be over any copy of the world: the layers are asked about the same place in the middle one
    QPointF pixels = at;
    {
        const double lonRaw = mapView->toLatLonRaw(at).second;
        const int copy = static_cast<int>(std::floor((lonRaw + 180.0) / 360.0));
        if (copy != 0) {
            const auto middle = mapView->transform();
            const double signedWorld = middle(0.0, 360.0).x() - middle(0.0, 0.0).x();   // units, east to the right or not as the projection has it
            pixels.setX(at.x() - copy * signedWorld / 1000.0 * std::max(1, mapView->pixelWidth()));
        }
    }
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
    // (nothing to compute: the map takes the room the layout leaves it; the time bar gives up its calendar when the window is narrow)
    if (timePick != nullptr) {
        const bool room = width() >= 1000;
        timePick->setVisible(room);
        timeGo->setVisible(room);
        rangeButton->setVisible(room);
        timeLabel->setMinimumWidth(room ? 180 : 40);
    }
}
