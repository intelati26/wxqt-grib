// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MrmsLayer.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <numbers>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QPushButton>
#include <QTreeWidget>
#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include "settings/Location.h"
#include "util/Utility.h"

MrmsLayer::MrmsLayer() : productList{UtilityMrms::groupedProducts({})} {
    productId = Utility::readPref("MM_MRMS_PRODUCT", productList.front().id);
}

const UtilityMrms::Product& MrmsLayer::product() const {
    const auto found = std::find_if(productList.begin(), productList.end(), [this] (const auto& p) { return p.id == productId; });
    return found == productList.end() ? productList.front() : *found;
}

// what the server lists beyond the hand-set products (some two hundred), read once in the background
void MrmsLayer::discover(MapHost& host) {
    if (discovering || productList.size() > UtilityMrms::products().size()) {
        return;
    }
    discovering = true;
    auto more = std::make_shared<std::vector<UtilityMrms::Product>>();
    host.background(
        [more] { string error; UtilityMrms::discoverMore(*more, error); },
        [this, more] {
            discovering = false;
            if (!more->empty()) {
                productList = UtilityMrms::groupedProducts(*more);
            }
        });
}

// "Home: 0.82 in   Springfield: none": the scan on view at the saved locations
string MrmsLayer::locationsText() const {
    if (!have || indices.isEmpty()) {
        return {};
    }
    const auto names = Location::listOfNames();
    const auto places = Location::getListLatLons();
    const auto& grid = frame.grid;
    string out;
    for (size_t i = 0; i < places.size(); i++) {
        const int column = static_cast<int>(std::floor((places[i].lon() - grid.west) / grid.cell));
        const int row = static_cast<int>(std::floor((grid.north - places[i].lat()) / grid.cell));
        if (column < 0 || column >= grid.columns || row < 0 || row >= grid.rows) {
            continue;
        }
        const int index = static_cast<uchar>(indices[static_cast<qsizetype>(row) * grid.columns + column]);
        const string name = i < names.size() ? names[i] : "Location " + std::to_string(i + 1);
        out += (out.empty() ? "" : "   ") + name + ": " +
            (index == 0 ? string{"none"} : QString::number(UtilityMrms::shown(product(), frame.valueAt(index), us), 'g', 3).toStdString() + " " + UtilityMrms::unitsShown(product(), us));
    }
    return out;
}

string MrmsLayer::summary() const {
    if (loading && !have) return "reading the MRMS scan...";
    if (!error.empty()) return error;
    if (!have) return {};
    const auto places = locationsText();
    return "MRMS " + product().label + " " + frame.utc.toString("HH:mm").toStdString() + "Z" + (places.empty() ? string{} : "\n" + places);
}

void MrmsLayer::rebuildColors() {
    colors = UtilityMrms::colorTable(product(), frame);
    for (auto& c : colors) {
        c = qRgba(qRed(c), qGreen(c), qBlue(c), qAlpha(c) * opacity / 100);
    }
    // premultiplied, as the image format is
    for (auto& c : colors) {
        const int a = qAlpha(c);
        c = qRgba(qRed(c) * a / 255, qGreen(c) * a / 255, qBlue(c) * a / 255, a);
    }
}

void MrmsLayer::display(const UtilityMrms::Frame& f) {
    frame = f;
    indices = frame.indices();
    rebuildColors();
    have = true;
}

vector<long> MrmsLayer::times() const {
    vector<long> out;
    for (const auto& [seconds, f] : frames) {
        out.push_back(seconds);
    }
    return out;
}

string MrmsLayer::timeText() const {
    return have ? frame.utc.toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  MRMS" : string{};
}

void MrmsLayer::showTime(long seconds, MapHost& host) {
    shownSeconds = seconds;
    if (frames.empty()) {
        return;
    }
    auto it = frames.end();
    if (seconds == 0) {
        it = std::prev(frames.end());
    } else {
        it = frames.upper_bound(seconds);   // the first after: the one before it is at or before
        it = it == frames.begin() ? it : std::prev(it);
    }
    if (!have || it->second.utc != frame.utc) {
        display(it->second);
        host.redraw();
    }
}

void MrmsLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int mine = ++generation;
    struct Loaded {
        std::vector<UtilityMrms::Scan> scans;
        UtilityMrms::Frame frame;
        string error;
    };
    auto result = std::make_shared<Loaded>();
    const auto chosen = product();
    host.background(
        [chosen, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error) && !result->scans.empty()) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            }
        },
        [this, &host, result, mine] {
            loading = false;
            if (mine != generation) {
                return;
            }
            if (!result->error.empty()) {
                error = result->error;
            } else {
                error.clear();
                scans = result->scans;
                frames[result->frame.utc.toSecsSinceEpoch()] = result->frame;
                while (frames.size() > 80) {
                    frames.erase(frames.begin());
                }
                if (shownSeconds == 0 || !have) {
                    display(result->frame);   // following the newest scan
                } else {
                    showTime(shownSeconds, host);
                }
                if (loopWanted) {
                    loopWanted = false;
                    prepareTimes(host);
                }
            }
            host.timesChanged();
            host.redraw();
        });
}

// the scans of a loop: the newest few, read side by side (each a small download and a GRIB decode)
void MrmsLayer::prepareTimes(MapHost& host) {
    if (preparing) {
        return;
    }
    if (scans.empty()) {
        loopWanted = true;   // the list first
        refresh(host);
        return;
    }
    std::vector<UtilityMrms::Scan> wanted;
    const size_t first = scans.size() > static_cast<size_t>(loopLength) ? scans.size() - static_cast<size_t>(loopLength) : 0;
    for (size_t i = first; i < scans.size(); i++) {
        if (frames.count(scans[i].utc.toSecsSinceEpoch()) == 0) {
            wanted.push_back(scans[i]);
        }
    }
    if (wanted.empty()) {
        host.timesChanged();
        return;
    }
    preparing = true;
    const int mine = generation;
    auto loaded = std::make_shared<std::vector<UtilityMrms::Frame>>();
    const auto chosen = product();
    host.background(
        [chosen, wanted, loaded] {
            // six at a time
            for (size_t start = 0; start < wanted.size(); start += 6) {
                std::vector<std::future<std::pair<bool, UtilityMrms::Frame>>> jobs;
                for (size_t i = start; i < std::min(wanted.size(), start + 6); i++) {
                    jobs.push_back(std::async(std::launch::async, [chosen, scan = wanted[i]] {
                        UtilityMrms::Frame f;
                        string message;
                        const bool ok = UtilityMrms::frame(chosen, scan, f, message);
                        return std::make_pair(ok, f);
                    }));
                }
                for (auto& job : jobs) {
                    auto [ok, f] = job.get();
                    if (ok) {
                        loaded->push_back(std::move(f));
                    }
                }
            }
        },
        [this, &host, loaded, mine] {
            preparing = false;
            if (mine == generation) {
                for (auto& f : *loaded) {
                    frames[f.utc.toSecsSinceEpoch()] = std::move(f);
                }
                while (frames.size() > 80) {
                    frames.erase(frames.begin());
                }
            }
            host.timesChanged();
        });
}

// every device pixel looks its cell up in the scan's grid: the picture is as sharp at any zoom as the data
void MrmsLayer::paint(QPainter& painter, MapHost& host) {
    if (!have || indices.isEmpty()) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const auto& grid = frame.grid;
    const double ratio = painter.device()->devicePixelRatioF();
    const int pixelWidth = std::max(1, static_cast<int>(std::lround(view.map()->width() * ratio)));
    const int pixelHeight = std::max(1, static_cast<int>(std::lround(view.map()->height() * ratio)));
    vector<int> columnOf(static_cast<size_t>(pixelWidth));
    for (int i = 0; i < pixelWidth; i++) {
        const double u = (i + 0.5) / ratio * 1000.0 / view.map()->width() - 500.0;
        const double lon = ((u - t.xPos) / t.zoom - t.bx) / t.ax;
        const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
        columnOf[static_cast<size_t>(i)] = column >= 0 && column < grid.columns ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    const auto * cells = reinterpret_cast<const uchar *>(indices.constData());
    for (int j = 0; j < pixelHeight; j++) {
        const double v = (j + 0.5) / ratio * 1000.0 / view.map()->width() - 250.0;
        const double mercator = ((v - t.yPos) / t.zoom - t.by) / t.ay;
        const double lat = std::atan(std::sinh(mercator * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        if (row < 0 || row >= grid.rows) {
            std::fill(out, out + pixelWidth, qRgba(0, 0, 0, 0));
            continue;
        }
        const auto * source = cells + static_cast<qsizetype>(row) * grid.columns;
        for (int i = 0; i < pixelWidth; i++) {
            const int column = columnOf[static_cast<size_t>(i)];
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : colors[source[column]];
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRectF{-500.0, -250.0, 1000.0, 1000.0 * view.map()->height() / std::max(1, view.map()->width())}, image, QRectF{image.rect()});
}

MapHit MrmsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;   // under every mark
    if (!have || indices.isEmpty()) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const auto& grid = frame.grid;
    const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
    const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
    if (column < 0 || column >= grid.columns || row < 0 || row >= grid.rows) {
        return hit;
    }
    const int index = static_cast<uchar>(indices[static_cast<qsizetype>(row) * grid.columns + column]);
    if (index == 0) {
        return hit;
    }
    const double value = UtilityMrms::shown(product(), frame.valueAt(index), us);
    hit.distance = 0.0;
    hit.text = QString::fromStdString(product().label) + ": " + QString::number(value, 'g', 3) + " " + QString::fromStdString(UtilityMrms::unitsShown(product(), us)) + "\n" +
        QString::number(lat, 'f', 2) + " N, " + QString::number(-lon, 'f', 2) + " W   " + frame.utc.toString("HH:mm") + "Z";
    return hit;
}

vector<MapLegendRow> MrmsLayer::legend() const {
    MapLegendRow row;
    row.title = QString::fromStdString(product().label) + " (" + QString::fromStdString(UtilityMrms::unitsShown(product(), us)) + "):";
    const auto& stops = product().stops;
    const double lo = frame.validMin;
    const double span = frame.step * 254.0;
    for (size_t i = 0; i < stops.size(); i++) {
        const double value = product().autoRange ? lo + stops[i].value * span : stops[i].value;
        row.entries.push_back({MapLegendEntry::Square, QColor{stops[i].r, stops[i].g, stops[i].b}, QString::number(UtilityMrms::shown(product(), value, us), 'g', 3)});
    }
    return {row};
}

QWidget * MrmsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Product:", widget});
    auto * choose = new QPushButton{QString::fromStdString(product().group + " - " + product().label), widget};
    choose->setToolTip("The products in groups (reflectivity, hail, rotation, precipitation, storm structure ...)");
    layout->addWidget(choose);
    layout->addWidget(new QLabel{"Frames in a loop (about two minutes apart):", widget});
    auto * loop = new QComboBox{widget};
    loop->addItems({"12", "24", "36", "48"});
    loop->setCurrentIndex(loopLength / 12 - 1);
    QObject::connect(loop, &QComboBox::currentIndexChanged, [this] (int index) { loopLength = 12 * (index + 1); });
    layout->addWidget(loop);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    auto * units = new QCheckBox{"US units (inches, kft)", widget};
    units->setChecked(us);
    layout->addWidget(units);
    QObject::connect(choose, &QPushButton::clicked, [this, choose, changed] {
        // the tree of groups, as a popup under the button
        auto * tree = new QTreeWidget{choose};
        tree->setWindowFlags(Qt::Popup);
        tree->setAttribute(Qt::WA_DeleteOnClose);
        tree->setHeaderHidden(true);
        tree->setIndentation(14);
        std::vector<string> groups;
        for (const auto& p : productList) {
            if (std::find(groups.begin(), groups.end(), p.group) == groups.end()) {
                groups.push_back(p.group);
            }
        }
        for (const auto& group : groups) {
            auto * top = new QTreeWidgetItem{tree, QStringList{QString::fromStdString(group)}};
            QFont bold = top->font(0);
            bold.setBold(true);
            top->setFont(0, bold);
            top->setData(0, Qt::UserRole, QString{});
            bool holdsShown = false;
            for (const auto& p : productList) {
                if (p.group != group) {
                    continue;
                }
                auto * item = new QTreeWidgetItem{top, QStringList{QString::fromStdString(p.label)}};
                item->setData(0, Qt::UserRole, QString::fromStdString(p.id));
                if (p.id == productId) {
                    holdsShown = true;
                    tree->setCurrentItem(item);
                }
            }
            top->setExpanded(holdsShown);
        }
        QObject::connect(tree, &QTreeWidget::itemClicked, tree, [this, tree, choose, changed] (QTreeWidgetItem * item, int) {
            const auto id = item->data(0, Qt::UserRole).toString().toStdString();
            if (id.empty()) {
                item->setExpanded(!item->isExpanded());   // a group: open or close it
                return;
            }
            tree->close();
            productId = id;
            Utility::writePref("MM_MRMS_PRODUCT", id);
            choose->setText(QString::fromStdString(product().group + " - " + product().label));
            have = false;
            indices.clear();
            frames.clear();
            scans.clear();
            shownSeconds = 0;
            error.clear();
            loading = false;   // a scan of the old product still on its way is dropped when it arrives (the generation moved on)
            ++generation;
            reloadNeeded = true;
            changed();
        });
        tree->resize(std::max(choose->width(), 360), 460);
        tree->move(choose->mapToGlobal(QPoint{0, choose->height()}));
        tree->show();
        tree->setFocus();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; if (have) rebuildColors(); changed(); });
    QObject::connect(units, &QCheckBox::toggled, [this, changed] (bool on) { us = on; changed(); });
    return widget;
}
