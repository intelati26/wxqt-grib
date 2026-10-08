// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HistoryViewer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/Coast.h"
#include "radar/MapLegend.h"
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityEnsembleStats.h"
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "util/UtilityUI.h"

namespace {
    int maxCategory(const UtilityHurdat::Track& t) {
        return UtilityAtcf::categoryOf(t.peakWind);
    }

    QString dates(const UtilityHurdat::Track& t) {
        if (t.points.empty()) {
            return {};
        }
        static const char * names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        const auto one = [] (const string& time) {
            const int month = std::atoi(time.substr(4, 2).c_str());
            return QString{names[std::clamp(month, 1, 12) - 1]} + " " + QString::number(std::atoi(time.substr(6, 2).c_str()));
        };
        return one(t.points.front().time) + " - " + one(t.points.back().time);
    }
}

QColor HistoryViewer::colorOf(int wind) {
    static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 235, 230}, QColor{255, 255, 204}, QColor{255, 231, 117},
                                    QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
    return colors[std::clamp(UtilityAtcf::categoryOf(wind), 0, 6)];
}

HistoryViewer::HistoryViewer(Window * parent)
    : Window{parent}
    , comboBasin{this, {"Atlantic", "East and Central Pacific"}}
    , comboFrom{this, {"1851"}}
    , comboTo{this, {"2025"}}
    , comboCategory{this, {"All storms", "Tropical storm or stronger", "Hurricane (Cat 1 or more)", "Major hurricane (Cat 3 or more)", "Cat 5"}}
    , comboNear{this, {"Anywhere"}}
    , comboRadius{this, {"within 100 km", "within 200 km", "within 300 km", "within 500 km"}}
    , entrySearch{this}
    , buttonArea{this, None, "Search an area"}
    , textStatus{this, "Loading the HURDAT2 database..."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Historical tropical cyclone tracks (HURDAT2)");
    textStatus.setWordWrap(false);
    std::vector<string> places{"Anywhere"};
    for (int i = 0; i < Location::getNumLocations(); i++) {
        places.push_back("Near " + Location::getName(static_cast<size_t>(i)));
    }
    comboNear.setList(places);
    comboRadius.setIndex(1);
    entrySearch.getView()->setPlaceholderText("name or id");
    entrySearch.getView()->setFixedWidth(130);
    const auto dimens = UtilityUI::getScreenBounds();
    const auto side = std::max(320, std::min(dimens[0] - 360, dimens[1] - 200));
    view = std::make_unique<MapView>(this, side);
    auto * map = view->map();
    map->dataLayer = [] (QPainter& painter) { painter.fillRect(QRectF{-1.0e6, -1.0e6, 2.0e6, 2.0e6}, QColor{16, 26, 42}); };
    map->topLayer = [this] (QPainter& painter) { paintMap(painter); };
    view->onPointer = [this] (const QPointF& at) { showHover(at); };
    view->onLeave = [this] { hoverLabel->hide(); if (hovered >= 0) { hovered = -1; view->map()->update(); } };
    view->showRegion(5.0, 50.0, -100.0, -10.0);
    hoverLabel = new QLabel{map};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 220); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    list = new QListWidget{this};
    list->setFixedWidth(300);
    list->setFixedHeight(side);
    QObject::connect(list, &QListWidget::currentRowChanged, [this] (int row) {
        selected = row >= 0 && static_cast<size_t>(row) < shown.size() ? static_cast<int>(shown[static_cast<size_t>(row)]) : -1;
        showSelected();
    });
    comboBasin.connect([this] { load(); });
    for (auto * combo : {&comboFrom, &comboTo, &comboCategory, &comboNear, &comboRadius}) {
        combo->connect([this] { applyFilters(); });
    }
    entrySearch.connect([this] { applyFilters(); });
    area = std::make_unique<AreaSearch>(view.get(), &buttonArea, [this] {
        static const double radii[] = {100.0, 200.0, 300.0, 500.0};
        return radii[std::clamp(comboRadius.getIndex(), 0, 3)];
    }, [this] { applyFilters(); });
    rowTop.addWidget(comboBasin);
    rowTop.addWidget(comboFrom);
    rowTop.addWidget(comboTo);
    rowTop.addWidget(comboCategory);
    rowTop.addWidget(comboNear);
    rowTop.addWidget(comboRadius);
    rowTop.addWidget(entrySearch);
    rowTop.addWidget(buttonArea);
    rowTop.addStretch();
    rowMain.addWidgetReal(map, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(list, 1, Qt::AlignTop | Qt::AlignLeft);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addLayout(rowMain);
    box.addStretch();
    box.getAndShow(this);
    load();
}

void HistoryViewer::load() {
    const int mine = ++generation;
    textStatus.setText(string{"Loading the HURDAT2 database..."});
    auto fresh = std::make_shared<HurricaneData::TrackData>();
    const auto basin = basinCode();
    new FutureVoid{this,
        [fresh, basin] { HurricaneData::loadTracks(*fresh, basin); },
        [this, mine, fresh] {
            if (closed || mine != generation) {
                return;
            }
            if (!fresh->error.empty()) {
                textStatus.setText(fresh->error);
                return;
            }
            data = fresh;
            int first = 9999;
            int last = 0;
            for (const auto& t : data->tracks) {
                first = std::min(first, t.year);
                last = std::max(last, t.year);
            }
            std::vector<string> years;
            for (int y = first; y <= last; y++) {
                years.push_back(std::to_string(y));
            }
            filling = true;
            comboFrom.setList(years);
            comboTo.setList(years);
            // the default: the last 30 seasons, the region of the basin
            comboFrom.setIndex(static_cast<size_t>(std::max(0, last - first - 29)));
            comboTo.setIndex(years.size() - 1);
            filling = false;
            if (basinCode() == "al") {
                view->showRegion(5.0, 55.0, -100.0, -5.0);
            } else {
                view->showRegion(0.0, 40.0, -180.0, -85.0);
            }
            applyFilters();
        }};
}

void HistoryViewer::applyFilters() {
    if (filling || !data) {
        return;
    }
    const int first = std::atoi(comboFrom.getValue().c_str());
    const int last = std::atoi(comboTo.getValue().c_str());
    const int category = comboCategory.getIndex();
    string needle = entrySearch.getText();
    std::transform(needle.begin(), needle.end(), needle.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const int place = comboNear.getIndex() - 1;   // -1: anywhere
    static const double radii[] = {100.0, 200.0, 300.0, 500.0};
    const double radius = radii[std::clamp(comboRadius.getIndex(), 0, 3)];
    double lat = 0.0;
    double lon = 0.0;
    if (place >= 0 && place < Location::getNumLocations()) {
        lat = Location::getLatLon(place).lat();
        lon = Location::getLatLon(place).lon();
    }
    shown.clear();
    selected = -1;
    for (size_t i = 0; i < data->tracks.size(); i++) {
        const auto& t = data->tracks[i];
        if (t.year < first || t.year > last) {
            continue;
        }
        if (category == 1 && !t.stormStrength) continue;
        if (category == 2 && !(t.stormStrength && t.peakWind >= 64)) continue;
        if (category == 3 && !(t.stormStrength && t.peakWind >= 96)) continue;
        if (category == 4 && !(t.stormStrength && t.peakWind >= 137)) continue;
        if (!needle.empty()) {
            string name = t.name + " " + t.id;
            if (name.find(needle) == string::npos) continue;
        }
        if (place >= 0) {
            bool near = false;
            for (size_t k = 0; k < t.points.size() && !near; k++) {
                const auto& p = t.points[k];
                // a cheap box test first
                if (std::abs(p.lat - lat) < radius / 111.0 + 1.0 && UtilityEnsembleStats::kilometers(lat, lon, p.lat, p.lon) <= radius) {
                    near = true;
                }
                // between two 6-hourly points the storm is moving 100 km or more: test the middle too
                if (!near && k + 1 < t.points.size()) {
                    const auto& q = t.points[k + 1];
                    near = UtilityEnsembleStats::kilometers(lat, lon, (p.lat + q.lat) / 2.0, (p.lon + q.lon) / 2.0) <= radius;
                }
            }
            if (!near) continue;
        }
        if (area->active() && !passesArea(t)) continue;
        shown.push_back(i);
    }
    // the list: the strongest first
    std::vector<size_t> order = shown;
    std::stable_sort(order.begin(), order.end(), [this] (size_t a, size_t b) { return data->tracks[a].peakWind > data->tracks[b].peakWind; });
    shown = order;
    list->blockSignals(true);
    list->clear();
    for (const auto i : shown) {
        list->addItem(describe(data->tracks[i]));
    }
    list->blockSignals(false);
    int storms = 0;
    int hurricanes = 0;
    int major = 0;
    for (const auto i : shown) {
        const auto& t = data->tracks[i];
        storms += t.stormStrength ? 1 : 0;
        hurricanes += t.stormStrength && t.peakWind >= 64 ? 1 : 0;
        major += t.stormStrength && t.peakWind >= 96 ? 1 : 0;
    }
    string text = std::to_string(shown.size()) + " tracks (" + std::to_string(storms) + " named storms, " + std::to_string(hurricanes) + " hurricanes, " + std::to_string(major) + " major) from " +
        std::to_string(first) + " to " + std::to_string(last) + "   -   " + data->file + "   -   " +
        (shown.size() > hoverLimit ? "narrow it to " + std::to_string(hoverLimit) + " tracks or fewer (years, category, an area) to hover a track for its name; or click one in the list" : string{"hover a track for its name, click one in the list to see only it"});
    if (area->active()) {
        text += "   -   " + area->describe().toStdString();
    }
    if (basinCode() == "ep") {
        text += "   (Pacific records before about 1971 are incomplete)";
    }
    textStatus.setText(text);
    view->map()->update();
}

// whether a storm's track goes through the area: a point inside it, or (for the 6-hourly points of a fast storm) the straight piece between two points
bool HistoryViewer::passesArea(const UtilityHurdat::Track& t) const {
    for (size_t k = 0; k < t.points.size(); k++) {
        if (area->hitsPoint(t.points[k].lat, t.points[k].lon)) {
            return true;
        }
        if (k > 0 && area->hitsSegment(t.points[k - 1].lat, t.points[k - 1].lon, t.points[k].lat, t.points[k].lon)) {
            return true;
        }
    }
    return false;
}

QString HistoryViewer::describe(const UtilityHurdat::Track& t) const {
    return QString::number(t.year) + "  " + QString::fromStdString(t.id.substr(0, 4)) + " " + QString::fromStdString(t.name) + "  " + QString::fromStdString(UtilityAtcf::windLabel(t.peakWind)) + (t.minPressure > 0 ? ", " + QString::number(t.minPressure) + " mb" : QString{});
}

void HistoryViewer::showSelected() {
    if (selected >= 0 && data) {
        const auto& t = data->tracks[static_cast<size_t>(selected)];
        double minLat = 90, maxLat = -90, minLon = 400, maxLon = -400;
        for (const auto& p : t.points) {
            minLat = std::min(minLat, p.lat);
            maxLat = std::max(maxLat, p.lat);
            minLon = std::min(minLon, p.lon);
            maxLon = std::max(maxLon, p.lon);
        }
        const double padLat = std::max(4.0, (maxLat - minLat) * 0.2);
        const double padLon = std::max(6.0, (maxLon - minLon) * 0.2);
        view->showRegion(minLat - padLat, maxLat + padLat, minLon - padLon, maxLon + padLon);
        textStatus.setText(describe(t).toStdString() + "  " + dates(t).toStdString() + "   (" + t.id + ")");
    }
    view->map()->update();
}

void HistoryViewer::resizeEventCustom() {
    if (view == nullptr) {
        return;
    }
    const int above = rowTop.getView()->sizeHint().height() + textStatus.getView()->sizeHint().height();
    view->fit(width() - 300 - 40, height() - above - 40);
    list->setFixedHeight(view->map()->height());
}

void HistoryViewer::paintMap(QPainter& painter) {
    const auto t = view->transform();
    const double px = view->unitsPerPixel();
    painter.setRenderHint(QPainter::Antialiasing, true);
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
    if (!data) {
        return;
    }
    // every track in the colour of the wind at the start of each piece, the pieces of one colour in one path
    QPainterPath paths[7];
    const bool one = selected >= 0;
    for (const auto i : shown) {
        if (one && static_cast<int>(i) != selected) {
            continue;
        }
        const auto& points = data->tracks[i].points;
        for (size_t k = 0; k + 1 < points.size(); k++) {
            const auto a = t(points[k].lat, points[k].lon);
            const auto b = t(points[k + 1].lat, points[k + 1].lon);
            if (std::abs(points[k].lon - points[k + 1].lon) > 100.0) {
                continue;   // across the date line
            }
            if ((a.x() < -600 && b.x() < -600) || (a.x() > 600 && b.x() > 600) || (a.y() < -350 && b.y() < -350) || (a.y() > 850 && b.y() > 850)) {
                continue;
            }
            auto& path = paths[std::clamp(UtilityAtcf::categoryOf(points[k].wind), 0, 6)];
            path.moveTo(a);
            path.lineTo(b);
        }
    }
    const double width = one ? 3.0 : (shown.size() > 600 ? 1.1 : shown.size() > 150 ? 1.5 : 2.0);
    for (int c = 0; c < 7; c++) {
        auto color = colorOf(c == 0 ? 20 : c == 1 ? 40 : c == 2 ? 70 : c == 3 ? 85 : c == 4 ? 100 : c == 5 ? 120 : 140);
        color.setAlpha(one ? 255 : (shown.size() > 600 ? 120 : 170));
        painter.setPen(QPen{color, width * px, Qt::SolidLine, Qt::RoundCap});
        painter.drawPath(paths[c]);
    }
    if (hovered >= 0 && !one) {
        const auto& points = data->tracks[static_cast<size_t>(hovered)].points;
        QPainterPath path;
        for (size_t k = 0; k < points.size(); k++) {
            const auto p = t(points[k].lat, points[k].lon);
            (k == 0 || std::abs(points[k].lon - points[k - 1].lon) > 100.0) ? path.moveTo(p) : path.lineTo(p);
        }
        painter.setPen(QPen{QColor{255, 255, 255}, 3.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        painter.drawPath(path);
    }
    if (one) {   // the places of the selected storm: a dot per 6-hourly position with the date at each day
        const auto& points = data->tracks[static_cast<size_t>(selected)].points;
        QFont font{painter.font()};
        font.setPixelSize(static_cast<int>(10 * px));
        painter.setFont(font);
        for (const auto& p : points) {
            const auto at = t(p.lat, p.lon);
            painter.setPen(QPen{QColor{0, 0, 0, 200}, 0.8 * px});
            painter.setBrush(colorOf(p.wind));
            painter.drawEllipse(at, 3.4 * px, 3.4 * px);
            if (p.time.substr(8, 2) == "00") {
                painter.setPen(QColor{230, 230, 230});
                painter.drawText(at + QPointF{6 * px, -5 * px}, QString::fromStdString(p.time.substr(4, 2) + "/" + p.time.substr(6, 2)));
            }
        }
    }
    area->paint(painter, t, px);
    // the legend
    MapLegendRow row;
    row.title = "Intensity along the track:";
    static const char * names[] = {"TD", "TS", "Cat 1", "Cat 2", "Cat 3", "Cat 4", "Cat 5"};
    static const int winds[] = {20, 40, 70, 85, 100, 120, 140};
    for (int c = 0; c <= 6; c++) {
        row.entries.push_back({MapLegendEntry::Circle, colorOf(winds[c]), names[c]});
    }
    MapLegend::draw(painter, {row}, px);
}

void HistoryViewer::showHover(const QPointF& pixels) {
    if (!data || selected >= 0) {
        return;
    }
    // with many tracks the search for the one under the pointer is slow and the choice not meaningful: hovering waits until there are few
    if (shown.size() > hoverLimit) {
        if (hovered >= 0) {
            hovered = -1;
            view->map()->update();
        }
        hoverLabel->hide();
        return;
    }
    int best = -1;
    double bestDistance = 9.0;
    for (const auto i : shown) {
        const auto& points = data->tracks[i].points;
        QPointF previous;
        for (size_t k = 0; k < points.size(); k++) {
            const auto at = view->toPixels(points[k].lat, points[k].lon);
            double d = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
            if (k > 0 && std::abs(points[k].lon - points[k - 1].lon) < 100.0) {
                const QPointF delta = at - previous;
                const double length2 = delta.x() * delta.x() + delta.y() * delta.y();
                double f = length2 > 0.0 ? ((pixels.x() - previous.x()) * delta.x() + (pixels.y() - previous.y()) * delta.y()) / length2 : 0.0;
                f = std::clamp(f, 0.0, 1.0);
                const QPointF nearest = previous + delta * f;
                d = std::min(d, std::hypot(nearest.x() - pixels.x(), nearest.y() - pixels.y()));
            }
            previous = at;
            if (d < bestDistance) {
                bestDistance = d;
                best = static_cast<int>(i);
            }
        }
    }
    if (best != hovered) {
        hovered = best;
        view->map()->update();
    }
    if (best < 0) {
        hoverLabel->hide();
        return;
    }
    const auto& t = data->tracks[static_cast<size_t>(best)];
    hoverLabel->setText(QString::fromStdString(t.id) + "\n" + describe(t) + "\n" + dates(t));
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}
