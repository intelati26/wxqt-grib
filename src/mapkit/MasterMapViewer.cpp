// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MasterMapViewer.h"
#include <algorithm>
#include <cmath>
#include <sstream>
#include <QFont>
#include <QPainterPath>
#include "hurricane/Coast.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"
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

MasterMapViewer::MasterMapViewer(Window * parent)
    : Window{parent}
    , comboView{this, {"Your layers"}}
    , buttonRefresh{this, None, "Refresh layers"}
    , buttonSave{this, None, "Save picture..."}
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
    mapView->onPointer = [this] (const QPointF& at) { showHover(at); };
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
    ChartExport::install(map, "Master map");
    hoverLabel = new QLabel{map};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 220); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();

    // the side panel: the tree, the options of the layer chosen in it
    sidePanel = new QWidget{this};
    sidePanel->setFixedWidth(panelWidth);
    auto * column = new QVBoxLayout{sidePanel};
    column->setContentsMargins(4, 0, 0, 0);
    tree = new QTreeWidget{sidePanel};
    tree->setHeaderHidden(true);
    tree->setIndentation(16);
    tree->setMinimumHeight(340);
    column->addWidget(tree, 3);
    optionsTitle = new QLabel{sidePanel};
    optionsTitle->setStyleSheet("font-weight: bold;");
    column->addWidget(optionsTitle);
    optionsBox = new QWidget{sidePanel};
    optionsLayout = new QVBoxLayout{optionsBox};
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    column->addWidget(optionsBox, 1);
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
    buttonSave.connect([this] {
        // the same menu as a right-click on the map
        QContextMenuEvent event{QContextMenuEvent::Mouse, QPoint{10, 10}, mapView->map()->mapToGlobal(QPoint{10, 10})};
        QCoreApplication::sendEvent(mapView->map(), &event);
    });
    rowTop.addWidget(comboView);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonSave);
    rowTop.addStretch();
    rowMain.addWidgetReal(map, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(sidePanel, 1, Qt::AlignTop | Qt::AlignLeft);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addLayout(rowMain);
    box.addStretch();
    box.getAndShow(this);
    resize(1240, 880);
    buildTree();
    // what was on last time
    building = true;
    for (const auto& id : split(Utility::readPref("MASTERMAP_ON", "obs/airports"), ',')) {
        setLayerOn(id, true);
        if (auto * item = items.count(id) != 0 ? items[id] : nullptr) {
            item->setCheckState(0, Qt::Checked);
        }
    }
    building = false;
    updateStatus();
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
        if (auto * widget = layer->options(optionsBox, [this] { redraw(); })) {
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
    saveState();
    updateStatus();
    redraw();
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
    mapView->showRegion(preset.minLat, preset.maxLat, preset.minLon, preset.maxLon);
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
    // the coastlines and borders of the world's tropics and mid-latitudes
    painter.setPen(QPen{QColor{110, 125, 145}, 1.0 * px});
    painter.setBrush(Qt::NoBrush);
    for (const auto& line : Coast::lines()) {
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
    cells.clear();
    for (auto& layer : layers) {
        if (layer->enabled()) {
            painter.save();
            layer->paint(painter, *this);
            painter.restore();
        }
    }
}

void MasterMapViewer::paintLegend(QPainter& painter) {
    const double px = mapView->unitsPerPixel();
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    painter.setRenderHint(QPainter::Antialiasing, true);
    vector<MapLegendRow> rows;
    for (const auto& layer : layers) {
        if (layer->enabled()) {
            for (auto& row : layer->legend()) {
                rows.push_back(std::move(row));
            }
        }
    }
    double y = 735.0;
    if (!rows.empty()) {   // a dark strip behind the rows so that they read over the marks
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{12, 18, 30, 190});
        painter.drawRect(QRectF{-500.0, y - 17.0 * static_cast<double>(rows.size()) + 5.0, 1000.0, 17.0 * static_cast<double>(rows.size()) + 12.0});
    }
    for (auto it = rows.rbegin(); it != rows.rend(); ++it) {
        double x = -490.0;
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QPointF{x, y + 4.0}, it->title);
        x += QFontMetricsF{font}.horizontalAdvance(it->title) + 10.0;
        for (const auto& e : it->entries) {
            painter.setPen(QPen{QColor{255, 255, 255, 200}, 0.9 * px});
            painter.setBrush(e.color);
            switch (e.shape) {
                case MapLegendEntry::Square: painter.drawRect(QRectF{x, y - 5.0, 10.0, 10.0}); break;
                case MapLegendEntry::Diamond: painter.drawPolygon(QPolygonF{{QPointF{x + 5.0, y - 6.0}, QPointF{x + 10.0, y}, QPointF{x + 5.0, y + 6.0}, QPointF{x, y}}}); break;
                case MapLegendEntry::Line: painter.setPen(QPen{e.color, 3.0 * px}); painter.drawLine(QPointF{x, y}, QPointF{x + 12.0, y}); break;
                default: painter.drawEllipse(QPointF{x + 5.0, y}, 5.0, 5.0); break;
            }
            painter.setPen(QColor{235, 235, 235});
            painter.drawText(QPointF{x + 15.0, y + 4.0}, e.label);
            x += 26.0 + QFontMetricsF{font}.horizontalAdvance(e.label);
        }
        y -= 17.0;
    }
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
    const int above = rowTop.getView()->sizeHint().height() + textStatus.getView()->sizeHint().height();
    mapView->fit(width() - panelWidth - 30, height() - above - 40);
}
