// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/TornadoViewer.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/Coast.h"
#include "objects/FutureVoid.h"
#include "radar/MapLegend.h"
#include "settings/Location.h"
#include "tornado/TornadoStatsViewer.h"
#include "util/UtilityUI.h"

namespace {
    using T = UtilityTornado::Tornado;

    QString dateText(const T& t) {
        static const char * names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
        return QString::number(t.day) + " " + names[std::clamp(t.month, 1, 12) - 1] + " " + QString::number(t.year);
    }
}

QColor TornadoViewer::colorOf(int mag) {
    switch (mag) {
        case 0: return QColor{110, 220, 255};
        case 1: return QColor{80, 220, 90};
        case 2: return QColor{255, 235, 60};
        case 3: return QColor{255, 150, 30};
        case 4: return QColor{250, 60, 60};
        case 5: return QColor{255, 70, 230};
        default: return QColor{150, 150, 160};
    }
}

QString TornadoViewer::describe(const T& t) {
    return dateText(t) + "  " + QString::fromStdString(t.state) + "  " + (t.preliminary ? QString{"prelim. report"} : QString::fromStdString(UtilityTornado::rating(t)) + "  " + QString::number(t.length, 'f', t.length < 10 ? 1 : 0) + " mi") +
        (t.fatalities > 0 ? "  " + QString::number(t.fatalities) + " dead" : QString{});
}

QString TornadoViewer::details(const T& t) {
    QString text = dateText(t) + "  " + QString::fromStdString(t.time.substr(0, 5)) + (t.timeZone == 3 ? " CST" : t.timeZone == 9 ? " GMT" : "") + "   " + QString::fromStdString(t.state) + "\n" +
        QString::fromStdString(UtilityTornado::rating(t)) + ",  " + QString::number(t.length, 'f', 1) + " miles long, " + QString::number(static_cast<int>(t.width)) + " yards wide\n" +
        QString::number(t.fatalities) + " deaths, " + QString::number(t.injuries) + " injuries";
    if (t.preliminary) {
        return dateText(t) + "  " + QString::fromStdString(t.time.substr(0, 5)) + " UTC   " + QString::fromStdString(t.state) + "\nA preliminary report (SPC daily reports): a point, not a surveyed track;\nrating " +
            (t.mag >= 0 ? QString::fromStdString(UtilityTornado::rating(t)) : QString{"not given"}) + ". It may count a tornado more than once.";
    }
    if (t.states > 1) {
        text += "\n(crossed " + QString::number(t.states) + " states; counted in the state of touchdown)";
    }
    return text;
}

TornadoViewer::TornadoViewer(Window * parent)
    : Window{parent}
    , comboSpan{this, {"The years chosen", "Last week of the data", "Last month of the data", "Last 12 months of the data", "Last decade of the data"}}
    , comboFrom{this, {"2016"}}
    , comboTo{this, {"2025"}}
    , comboRating{this, {"All tornadoes", "EF1 or stronger", "EF2 or stronger", "EF3 or stronger", "EF4 or stronger", "EF5 only", "Unrated only"}}
    , comboState{this, {"All states"}}
    , comboKind{this, {"All", "Fatal ones", "With injuries or deaths"}}
    , comboNear{this, {"Anywhere"}}
    , comboRadius{this, {"within 25 km", "within 50 km", "within 100 km", "within 200 km"}}
    , buttonArea{this, None, "Search an area"}
    , buttonCharts{this, None, "Charts..."}
    , textStatus{this, "Loading the SPC tornado database..."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tornado history - SPC database, tracks by year");
    textStatus.setWordWrap(false);
    std::vector<string> places{"Anywhere"};
    for (int i = 0; i < Location::getNumLocations(); i++) {
        places.push_back("Near " + Location::getName(static_cast<size_t>(i)));
    }
    comboNear.setList(places);
    comboRadius.setIndex(1);
    comboRating.setIndex(1);
    const auto dimens = UtilityUI::getScreenBounds();
    const auto side = std::max(320, std::min(dimens[0] - 360, dimens[1] - 230));
    view = std::make_unique<MapView>(this, side);
    auto * map = view->map();
    map->dataLayer = [] (QPainter& painter) { painter.fillRect(QRectF{-1.0e6, -1.0e6, 2.0e6, 2.0e6}, QColor{16, 26, 42}); };
    map->topLayer = [this] (QPainter& painter) { paintMap(painter); };
    view->onPointer = [this] (const QPointF& at) { showHover(at); };
    view->onLeave = [this] { hoverLabel->hide(); if (hovered != nullptr) { hovered = nullptr; view->map()->update(); } };
    view->showRegion(24.0, 50.0, -125.0, -66.0);
    hoverLabel = new QLabel{map};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 220); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    list = new QListWidget{this};
    list->setFixedWidth(310);
    list->setFixedHeight(side);
    QObject::connect(list, &QListWidget::currentRowChanged, [this] (int row) {
        selected = row >= 0 && static_cast<size_t>(row) < listed.size() ? listed[static_cast<size_t>(row)] : nullptr;
        showSelected();
    });
    area = std::make_unique<AreaSearch>(view.get(), &buttonArea, [this] {
        static const double radii[] = {25.0, 50.0, 100.0, 200.0};
        return radii[std::clamp(comboRadius.getIndex(), 0, 3)];
    }, [this] { applyFilters(); });
    for (auto * combo : {&comboSpan, &comboFrom, &comboTo, &comboRating, &comboState, &comboKind, &comboNear, &comboRadius}) {
        combo->connect([this] { applyFilters(); });
    }
    buttonCharts.connect([this] {
        if (db && db->error.empty()) {
            new TornadoStatsViewer{this, db};
        }
    });
    rowTop.addWidget(comboSpan);
    rowTop.addWidget(comboFrom);
    rowTop.addWidget(comboTo);
    rowTop.addWidget(comboRating);
    rowTop.addWidget(comboState);
    rowTop.addWidget(comboKind);
    rowTop.addStretch();
    rowMore.addWidget(comboNear);
    rowMore.addWidget(comboRadius);
    rowMore.addWidget(buttonArea);
    rowMore.addWidget(buttonCharts);
    rowMore.addStretch();
    rowMain.addWidgetReal(map, 0, Qt::AlignTop | Qt::AlignLeft);
    rowMain.addWidgetReal(list, 1, Qt::AlignTop | Qt::AlignLeft);
    box.addLayout(rowTop);
    box.addLayout(rowMore);
    box.addWidget(textStatus);
    box.addLayout(rowMain);
    box.addStretch();
    box.getAndShow(this);
    load();
}

void TornadoViewer::load() {
    auto loaded = std::make_shared<std::shared_ptr<const TornadoData::Database>>();
    new FutureVoid{this, [loaded] { *loaded = TornadoData::load(); }, [this, loaded] {
        if (closed) {
            return;
        }
        db = *loaded;
        if (!db->error.empty()) {
            textStatus.setText(db->error);
            return;
        }
        std::vector<string> years;
        for (int y = db->firstYear; y <= db->lastYear; y++) {
            years.push_back(std::to_string(y));
        }
        std::set<string> states;
        for (const auto& t : db->tornadoes) {
            states.insert(t.state);
        }
        std::vector<string> stateList{"All states"};
        stateList.insert(stateList.end(), states.begin(), states.end());
        filling = true;
        comboFrom.setList(years);
        comboTo.setList(years);
        comboFrom.setIndex(static_cast<size_t>(std::max(0, db->lastYear - db->firstYear - 9)));
        comboTo.setIndex(years.size() - 1);
        comboState.setList(stateList);
        filling = false;
        applyFilters();
    }};
}

void TornadoViewer::applyFilters() {
    if (filling || !db) {
        return;
    }
    const bool spanned = comboSpan.getIndex() > 0;
    comboFrom.setVisible(!spanned);
    comboTo.setVisible(!spanned);
    int first = std::atoi(comboFrom.getValue().c_str());
    int last = std::atoi(comboTo.getValue().c_str());
    long cutoff = 0;   // with a span: from this day on
    if (spanned) {
        static const int days[] = {0, 7, 31, 365, 3652};
        cutoff = TornadoData::ordinal(db->lastYear, db->lastMonth, db->lastDay) - days[comboSpan.getIndex()] + 1;
        first = 0;
        last = 9999;
    }
    const int rating = comboRating.getIndex();
    const string state = comboState.getIndex() > 0 ? comboState.getValue() : string{};
    const int kind = comboKind.getIndex();
    const int place = comboNear.getIndex() - 1;
    static const double radii[] = {25.0, 50.0, 100.0, 200.0};
    const double radius = radii[std::clamp(comboRadius.getIndex(), 0, 3)];
    double lat = 0.0;
    double lon = 0.0;
    if (place >= 0 && place < Location::getNumLocations()) {
        lat = Location::getLatLon(place).lat();
        lon = Location::getLatLon(place).lon();
    }
    shown.clear();
    selected = nullptr;
    for (const auto& t : db->tornadoes) {
        if (!t.counts() || t.year < first || t.year > last) {
            continue;
        }
        if (spanned && TornadoData::ordinal(t.year, t.month, t.day) < cutoff) {
            continue;
        }
        if (!UtilityTornado::passesRating(t, rating)) continue;
        if (!state.empty() && t.state != state) continue;
        if (kind == 1 && t.fatalities <= 0) continue;
        if (kind == 2 && t.fatalities + t.injuries <= 0) continue;
        if (place >= 0 && UtilityTornado::distanceToTrack(t, lat, lon) > radius) continue;
        if (area->active() && !(t.hasEnd() ? area->hitsSegment(t.startLat, t.startLon, t.endLat, t.endLon) : area->hitsPoint(t.startLat, t.startLon))) continue;
        shown.push_back(&t);
    }
    // the list: the strongest, the deadliest and the longest first, as many as are useful
    listed = shown;
    std::sort(listed.begin(), listed.end(), [] (const T * a, const T * b) {
        if (a->mag != b->mag) return a->mag > b->mag;
        if (a->fatalities != b->fatalities) return a->fatalities > b->fatalities;
        return a->length > b->length;
    });
    if (listed.size() > 400) {
        listed.resize(400);
    }
    list->blockSignals(true);
    list->clear();
    for (const auto * t : listed) {
        list->addItem(describe(*t));
    }
    list->blockSignals(false);
    int deaths = 0;
    int injuries = 0;
    int strong = 0;
    for (const auto * t : shown) {
        deaths += t->fatalities;
        injuries += t->injuries;
        strong += t->mag >= 3 ? 1 : 0;
    }
    string text = QLocale{QLocale::English}.toString(static_cast<qlonglong>(shown.size())).toStdString() + " tornadoes (" + std::to_string(strong) + " rated 3 or more), " + std::to_string(deaths) + " deaths, " +
        std::to_string(injuries) + " injuries";
    text += spanned ? "   -   the " + string{comboSpan.getValue()} : "   -   " + std::to_string(first) + " to " + std::to_string(last);
    text += "   -   " + db->file + (db->preliminaryCount > 0 ? " + " + std::to_string(db->preliminaryCount) + " preliminary reports since" : string{}) + " (to " + std::to_string(db->lastDay) + "/" + std::to_string(db->lastMonth) + "/" + std::to_string(db->lastYear) + ")";
    if (area->active()) {
        text += "   -   " + area->describe().toStdString();
    }
    text += shown.size() > hoverLimit ? "   -   narrow it to " + std::to_string(hoverLimit) + " or fewer to hover a track; or click one in the list" : string{"   -   hover a track for its row"};
    textStatus.setText(text);
    view->map()->update();
}

void TornadoViewer::showSelected() {
    if (selected != nullptr) {
        double minLat = selected->startLat, maxLat = selected->startLat, minLon = selected->startLon, maxLon = selected->startLon;
        if (selected->hasEnd()) {
            minLat = std::min(minLat, selected->endLat);
            maxLat = std::max(maxLat, selected->endLat);
            minLon = std::min(minLon, selected->endLon);
            maxLon = std::max(maxLon, selected->endLon);
        }
        const double pad = std::max(0.35, std::max(maxLat - minLat, maxLon - minLon) * 0.6);
        view->showRegion(minLat - pad, maxLat + pad, minLon - pad, maxLon + pad);
        textStatus.setText(describe(*selected).toStdString() + "   (" + selected->id + ", " + selected->time + ")");
    }
    view->map()->update();
}

void TornadoViewer::resizeEventCustom() {
    if (view == nullptr) {
        return;
    }
    const int above = rowTop.getView()->sizeHint().height() * 2 + textStatus.getView()->sizeHint().height();
    view->fit(width() - 310 - 40, height() - above - 40);
    list->setFixedHeight(view->map()->height());
}

void TornadoViewer::paintMap(QPainter& painter) {
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
    if (!db) {
        return;
    }
    // the tracks by rating (the strongest last, on top): the lines in one path and the start points in one list for each rating; only those in view
    QPainterPath lines[7];
    QPolygonF dots[7];
    const bool one = selected != nullptr;
    const auto inView = [] (const QPointF& p) { return p.x() > -540.0 && p.x() < 540.0 && p.y() > -290.0 && p.y() < 790.0; };
    for (const auto * tornado : shown) {
        if (one && tornado != selected) {
            continue;
        }
        const auto a = t(tornado->startLat, tornado->startLon);
        const int rank = tornado->mag < 0 ? 0 : tornado->mag + 1;
        if (tornado->hasEnd()) {
            const auto b = t(tornado->endLat, tornado->endLon);
            if (!inView(a) && !inView(b)) {
                continue;
            }
            lines[rank].moveTo(a);
            lines[rank].lineTo(b);
        } else if (!inView(a)) {
            continue;
        }
        dots[rank] << a;
    }
    const double scale = shown.size() > 5000 ? 0.75 : 1.0;
    for (int rank = 0; rank < 7; rank++) {
        const QColor color = colorOf(rank - 1);
        const double width = (1.3 + 0.45 * (rank - 1 < 0 ? 0 : rank - 1)) * scale * (one ? 2.2 : 1.0);
        painter.setPen(QPen{color, width * px, Qt::SolidLine, Qt::RoundCap});
        painter.drawPath(lines[rank]);
        painter.setPen(QPen{color, (width + 2.0) * px, Qt::SolidLine, Qt::RoundCap});
        painter.drawPoints(dots[rank]);
    }
    if (hovered != nullptr && !one) {
        painter.setPen(QPen{QColor{255, 255, 255}, 3.5 * px, Qt::SolidLine, Qt::RoundCap});
        const auto a = t(hovered->startLat, hovered->startLon);
        if (hovered->hasEnd()) {
            painter.drawLine(a, t(hovered->endLat, hovered->endLon));
        } else {
            painter.drawPoint(a);
        }
    }
    area->paint(painter, t, px);
    MapLegendRow row;
    row.title = "Rating:";
    static const char * names[] = {"F/EF0", "1", "2", "3", "4", "5", "unrated"};
    for (int m = 0; m < 6; m++) {
        row.entries.push_back({MapLegendEntry::Circle, colorOf(m), names[m]});
    }
    row.entries.push_back({MapLegendEntry::Circle, colorOf(-1), names[6]});
    MapLegend::draw(painter, {row}, px);
}

void TornadoViewer::showHover(const QPointF& pixels) {
    if (!db || selected != nullptr) {
        return;
    }
    // with many tornadoes the search for the one under the pointer is slow and the choice not meaningful
    if (shown.size() > hoverLimit) {
        if (hovered != nullptr) {
            hovered = nullptr;
            view->map()->update();
        }
        hoverLabel->hide();
        return;
    }
    const T * best = nullptr;
    double bestDistance = 10.0;
    for (const auto * tornado : shown) {
        const auto a = view->toPixels(tornado->startLat, tornado->startLon);
        double d = std::hypot(a.x() - pixels.x(), a.y() - pixels.y());
        if (tornado->hasEnd()) {
            const auto b = view->toPixels(tornado->endLat, tornado->endLon);
            const QPointF delta = b - a;
            const double length2 = delta.x() * delta.x() + delta.y() * delta.y();
            const double f = length2 > 0.0 ? std::clamp(((pixels.x() - a.x()) * delta.x() + (pixels.y() - a.y()) * delta.y()) / length2, 0.0, 1.0) : 0.0;
            const QPointF nearest = a + delta * f;
            d = std::min(d, std::hypot(nearest.x() - pixels.x(), nearest.y() - pixels.y()));
        }
        if (d < bestDistance) {
            bestDistance = d;
            best = tornado;
        }
    }
    if (best != hovered) {
        hovered = best;
        view->map()->update();
    }
    if (best == nullptr) {
        hoverLabel->hide();
        return;
    }
    hoverLabel->setText(details(*best));
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}
