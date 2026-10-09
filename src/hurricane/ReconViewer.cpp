// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/ReconViewer.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <QDateTime>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QToolTip>
#include "ui/HoverTip.h"
#include "hurricane/Coast.h"
#include "hurricane/UtilityDropsonde.h"
#include "hurricane/DropsondeViewer.h"
#include "hurricane/UtilityVdm.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "ui/ActivityLabel.h"
#include "ui/ChartExport.h"
#include "ui/WindBarb.h"
#include "util/Utility.h"

namespace {
    struct FloaterChoice {
        const char * folder;
        const char * label;
    };
    const FloaterChoice floaters[] = {
        {"GEOCOLOR", "GeoColor"},
        {"Sandwich", "Sandwich"},
        {"13", "Infrared (band 13)"},
        {"09", "Mid-level water vapour (band 9)"},
        {"AirMass", "Air mass"},
        {"02", "Visible (band 2)"},
    };
    const int refreshSeconds[] = {0, 120, 300, 600};

    bool has(double v) { return v > -9000.0; }

    QString position(double lat, double lon) {
        return QString::number(std::abs(lat), 'f', 1) + (lat < 0 ? "S " : "N ") + QString::number(std::abs(lon), 'f', 1) + (lon < 0 ? "W" : "E");
    }
    QString clock(long seconds) {
        return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).toString("HH:mm") + "Z";
    }
    QString dayClock(long seconds) {
        return QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm") + "Z";
    }
    std::string upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }
    std::string firstWords(const std::string& s, int count) {   // "NOAA9 01BBA SURV OB 32" -> "NOAA9 01BBA"
        std::istringstream in{s};
        std::string word, out;
        for (int i = 0; i < count && (in >> word); i++) {
            out += (i ? " " : "") + word;
        }
        return out;
    }
}

// ---- the map

ReconMap::ReconMap(QWidget * parent) : QWidget{parent} {
    setMinimumSize(520, 520);
    setMouseTracking(true);
    ChartExport::install(this, "Recon flight");
}

void ReconMap::setFlight(const Flight& f) {
    flight = f;
    update();
}

void ReconMap::setImage(const QImage& picture, const FloaterGeo::Geo& g) {
    image = picture;
    geo = g;
    update();
}

QColor ReconMap::windColor(double kt) {
    if (kt < 34) return QColor{"#9ad6e6"};
    if (kt < 64) return QColor{"#6cc496"};
    if (kt < 83) return QColor{"#f1e27a"};
    if (kt < 96) return QColor{"#f4a95a"};
    if (kt < 113) return QColor{"#e0603e"};
    if (kt < 130) return QColor{"#b8303c"};
    return QColor{"#b05ad0"};
}

QRectF ReconMap::baseRect() const {
    const double side = std::min(width(), height());
    return {(width() - side) / 2.0, (height() - side) / 2.0, side, side};
}

QRectF ReconMap::pictureRect() const {
    const auto b = baseRect();
    const auto c = b.center() + pan;
    return {c.x() - b.width() * zoom / 2.0, c.y() - b.height() * zoom / 2.0, b.width() * zoom, b.height() * zoom};
}

void ReconMap::wheelEvent(QWheelEvent * event) {
    const double factor = std::pow(1.0015, event->angleDelta().y());
    const double next = std::clamp(zoom * factor, 1.0, 24.0);
    const auto b = baseRect();
    const auto anchor = event->position();
    // keep the point under the pointer where it is
    const auto old = pictureRect();
    const double fx = (anchor.x() - old.left()) / old.width(), fy = (anchor.y() - old.top()) / old.height();
    zoom = next;
    const double w = b.width() * zoom, h = b.height() * zoom;
    const QPointF centre{anchor.x() - fx * w + w / 2.0, anchor.y() - fy * h + h / 2.0};
    pan = centre - b.center();
    if (zoom == 1.0) {
        pan = {};
    }
    update();
}

void ReconMap::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragFrom = event->position();
        pressAt = event->position();
    }
}

void ReconMap::mouseReleaseEvent(QMouseEvent * event) {
    const bool click = dragging && std::hypot(event->position().x() - pressAt.x(), event->position().y() - pressAt.y()) < 4.0;
    dragging = false;
    if (click && onDrop) {
        if (const auto * drop = dropAt(event->position())) {
            onDrop(*drop);
        }
    }
}

const UtilityDropsonde::Drop * ReconMap::dropAt(const QPointF& at) const {
    const UtilityDropsonde::Drop * found = nullptr;
    double best = 12.0;
    for (const auto& drop : flight.drops) {
        const double lat = has(drop.releaseLat) ? drop.releaseLat : drop.lat, lon = has(drop.releaseLon) ? drop.releaseLon : drop.lon;
        if (!has(lat) || !has(lon)) {
            continue;
        }
        const auto p = toWidget(lon, lat) + QPointF{0, 0};
        const double d = std::hypot(p.x() - at.x(), p.y() - at.y());
        if (d < best) {
            best = d;
            found = &drop;
        }
    }
    return found;
}

void ReconMap::mouseDoubleClickEvent(QMouseEvent *) {
    zoom = 1.0;
    pan = {};
    update();
}

QPointF ReconMap::toWidget(double lon, double lat) const {
    const auto r = pictureRect();
    // longitudes past the date line: put the longitude within 180 of the box's left edge
    while (lon < geo.lonAtLeft - 180.0) {
        lon += 360.0;
    }
    while (lon > geo.lonAtLeft + 180.0) {
        lon -= 360.0;
    }
    return {r.left() + geo.x(lon) / geo.width * r.width(), r.top() + geo.y(lat) / geo.height * r.height()};
}

void ReconMap::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{16, 24, 32});
    const auto r = pictureRect();
    if (!image.isNull() && geo.width > 0) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawImage(r, image);
    } else {
        // no picture: the coastlines on a plain box centred on the storm, so the flight can still be seen
        geo = FloaterGeo::Geo{};
        geo.width = 1000;
        geo.height = 1000;
        geo.pixelsPerDegree = 1000.0 / 18.0;
        geo.lonAtLeft = flight.stormLon - 9.0;
        geo.latAtTop = flight.stormLat + 9.0;
        p.fillRect(r, QColor{30, 48, 66});
        p.setPen(QPen{QColor{150, 170, 190}, 1.0});
        for (const auto& line : Coast::lines()) {
            QPainterPath path;
            bool started = false;
            for (const auto& pt : line) {
                const auto at = toWidget(pt.first, pt.second);
                started ? path.lineTo(at) : path.moveTo(at);
                started = true;
            }
            p.drawPath(path);
        }
    }
    p.setClipRect(baseRect().united(r).intersected(rect()));

    // the storm's own track
    if (flight.stormTrack.size() > 1) {
        QPainterPath path;
        bool started = false;
        for (const auto& [lat, lon] : flight.stormTrack) {
            const auto at = toWidget(lon, lat);
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.setPen(QPen{QColor{255, 255, 255, 190}, 1.4, Qt::DashLine});
        p.drawPath(path);
    }

    // the flight: a segment at a time, coloured by the wind
    const auto& obs = flight.obs;
    const auto wind = [this] (const UtilityHdob::Ob& ob) { return useSfmr ? ob.sfmrWind : ob.windSpeed; };
    for (size_t i = 1; i < obs.size(); i++) {
        if (obs[i].seconds - obs[i - 1].seconds > 900) {   // a gap of a quarter hour or more: the aircraft was away
            continue;
        }
        const double a = wind(obs[i - 1]), b = wind(obs[i]);
        const double kt = has(a) && has(b) ? std::max(a, b) : has(b) ? b : has(a) ? a : -1.0;
        p.setPen(QPen{kt >= 0 ? windColor(kt) : QColor{190, 190, 190}, 3.0, Qt::SolidLine, Qt::RoundCap});
        p.drawLine(toWidget(obs[i - 1].lon, obs[i - 1].lat), toWidget(obs[i].lon, obs[i].lat));
    }
    p.setPen(QPen{QColor{0, 0, 0, 120}, 0.8});   // a thin dark edge under the coloured line is not drawn; the barbs follow
    if (barbs) {
        p.setPen(QPen{QColor{15, 15, 15}, 1.2});
        p.setBrush(QColor{15, 15, 15});
        long lastBarb = 0;
        for (const auto& ob : obs) {
            if (!has(ob.windSpeed) || !has(ob.windDirection) || ob.seconds - lastBarb < 300) {   // one barb every five minutes
                continue;
            }
            lastBarb = ob.seconds;
            WindBarb::draw(p, toWidget(ob.lon, ob.lat), ob.windDirection, ob.windSpeed, 18.0, ob.lat < 0.0);
        }
    }
    // the newest position of the aircraft
    if (!obs.empty()) {
        const auto at = toWidget(obs.back().lon, obs.back().lat);
        p.setPen(QPen{QColor{20, 20, 20}, 1.5});
        p.setBrush(QColor{255, 255, 255});
        p.drawEllipse(at, 6.0, 6.0);
        p.setPen(QColor{255, 255, 255});
        p.drawText(at + QPointF{9, 4}, clock(obs.back().seconds));
    }
    // vortex fixes
    QFont small = p.font();
    small.setPixelSize(11);
    small.setBold(true);
    p.setFont(small);
    for (const auto& fix : flight.fixes) {
        if (!has(fix.lat) || !has(fix.lon)) {
            continue;
        }
        const auto at = toWidget(fix.lon, fix.lat);
        QPolygonF diamond;
        diamond << at + QPointF{0, -7} << at + QPointF{7, 0} << at + QPointF{0, 7} << at + QPointF{-7, 0};
        p.setPen(QPen{QColor{20, 20, 20}, 1.2});
        p.setBrush(QColor{255, 210, 60});
        p.drawPolygon(diamond);
        if (has(fix.pressure)) {
            p.setPen(QColor{255, 232, 140});
            p.drawText(at + QPointF{9, -4}, QString::number(static_cast<int>(std::lround(fix.pressure))) + " mb");
        }
    }
    // dropsondes: where they were released, with the lowest pressure and the strongest wind they measured
    for (const auto& drop : flight.drops) {
        const double lat = has(drop.releaseLat) ? drop.releaseLat : drop.lat, lon = has(drop.releaseLon) ? drop.releaseLon : drop.lon;
        if (!has(lat) || !has(lon)) {
            continue;
        }
        const auto at = toWidget(lon, lat);
        QPolygonF triangle;
        triangle << at + QPointF{0, -7} << at + QPointF{7, 6} << at + QPointF{-7, 6};
        p.setPen(QPen{QColor{20, 20, 20}, 1.2});
        p.setBrush(QColor{120, 220, 255});
        p.drawPolygon(triangle);
        const double pressure = UtilityDropsonde::minimumPressure(drop), maxWind = UtilityDropsonde::maxWind(drop);
        QString label;
        if (has(pressure)) {
            label += QString::number(static_cast<int>(std::lround(pressure)));
        }
        if (has(maxWind)) {
            label += (label.isEmpty() ? "" : " / ") + QString::number(static_cast<int>(std::lround(maxWind))) + " kt";
        }
        if (!label.isEmpty()) {
            p.setPen(QColor{190, 235, 255});
            p.drawText(at + QPointF{9, 14}, label);
        }
    }
    // the storm's own position
    if (flight.haveStorm) {
        const auto at = toWidget(flight.stormLon, flight.stormLat);
        p.setPen(QPen{QColor{255, 80, 80}, 2.0});
        p.drawLine(at + QPointF{-9, 0}, at + QPointF{9, 0});
        p.drawLine(at + QPointF{0, -9}, at + QPointF{0, 9});
    }
    p.setClipping(false);
    // a small key at the top left, on a dark box
    p.setFont([&] { auto f = p.font(); f.setBold(false); return f; }());
    const struct { double kt; const char * text; } key[] = {{20, "<34"}, {50, "34-63"}, {70, "64-82"}, {90, "83-95"}, {100, "96-112"}, {120, "113-129"}, {140, "130+"}};
    double width = 0.0;
    for (const auto& k : key) {
        width += 16 + p.fontMetrics().horizontalAdvance(k.text) + 10;
    }
    const auto keyBox = baseRect();
    p.fillRect(QRectF{keyBox.left() + 4, keyBox.top() + 4, std::max(width + 8, 170.0), 34}, QColor{0, 0, 0, 150});
    double x = keyBox.left() + 8;
    const double y = keyBox.top() + 33;
    p.setPen(QColor{255, 255, 255});
    p.drawText(QPointF{x, y - 14}, useSfmr ? "SFMR surface wind (kt)" : "Flight-level wind (kt)");
    for (const auto& k : key) {
        p.fillRect(QRectF{x, y - 9, 13, 9}, windColor(k.kt));
        p.setPen(QColor{255, 255, 255});
        p.drawText(QPointF{x + 16, y}, k.text);
        x += 16 + p.fontMetrics().horizontalAdvance(k.text) + 10;
    }
}

void ReconMap::mouseMoveEvent(QMouseEvent * event) {
    if (dragging) {
        pan += event->position() - dragFrom;
        dragFrom = event->position();
        update();
        return;
    }
    if (const auto * drop = dropAt(event->position())) {   // over a dropsonde: say so, and that it opens
        setCursor(Qt::PointingHandCursor);
        const double pressure = UtilityDropsonde::minimumPressure(*drop), wind = UtilityDropsonde::maxWind(*drop);
        QString text = "Dropsonde " + dayClock(drop->seconds);
        if (has(pressure)) text += "   lowest pressure " + QString::number(static_cast<int>(std::lround(pressure))) + " mb";
        if (has(wind)) text += "   strongest wind " + QString::number(static_cast<int>(std::lround(wind))) + " kt";
        HoverTip::show(this, event->globalPosition().toPoint(), text + "\nClick for the profile");
        return;
    }
    setCursor(Qt::ArrowCursor);
    double best = 14.0;
    const UtilityHdob::Ob * near = nullptr;
    for (const auto& ob : flight.obs) {
        const auto at = toWidget(ob.lon, ob.lat);
        const double d = std::hypot(at.x() - event->position().x(), at.y() - event->position().y());
        if (d < best) {
            best = d;
            near = &ob;
        }
    }
    if (!near) {
        HoverTip::hide();
        return;
    }
    QString text = dayClock(near->seconds) + "   " + position(near->lat, near->lon);
    if (has(near->windSpeed)) {
        text += "\nflight-level wind " + QString::number(static_cast<int>(std::lround(near->windSpeed))) + " kt" + (has(near->windDirection) ? " from " + QString::number(static_cast<int>(std::lround(near->windDirection))) + " deg" : QString{});
    }
    if (has(near->peakWind)) {
        text += ", peak " + QString::number(static_cast<int>(std::lround(near->peakWind))) + " kt";
    }
    if (has(near->sfmrWind)) {
        text += "\nsurface wind (SFMR) " + QString::number(static_cast<int>(std::lround(near->sfmrWind))) + " kt";
    }
    if (has(near->surfacePressure)) {
        text += "\nextrapolated surface pressure " + QString::number(near->surfacePressure, 'f', 1) + " mb";
    }
    if (has(near->staticPressure)) {
        text += "\nflight level " + QString::number(near->staticPressure, 'f', 1) + " mb" + (has(near->height) ? ", " + QString::number(static_cast<int>(std::lround(near->height))) + " m" : QString{});
    }
    HoverTip::show(this, event->globalPosition().toPoint(), text);
}

// ---- the page

ReconViewer::ReconViewer(Window * parent, const std::string& id, const std::string& stormName)
    : Window{parent}
    , comboMission{this, {"Looking for flights..."}}
    , comboFloater{this, {}}
    , comboColor{this, {"Color by flight-level wind", "Color by surface wind (SFMR)"}}
    , comboRefresh{this, {"Refresh by hand", "Refresh every 2 minutes", "Refresh every 5 minutes", "Refresh every 10 minutes"}}
    , buttonRefresh{this, None, "Refresh now"}
    , textStatus{this, ""}
    , stormId{id}
    , name{stormName}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Recon flight - " + (stormName.empty() ? id : stormName));
    std::vector<std::string> labels;
    for (const auto& f : floaters) {
        labels.push_back(f.label);
    }
    comboFloater.setList(labels);
    comboFloater.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("RECON_FLOATER", 0), 0, static_cast<int>(labels.size()) - 1)));
    comboRefresh.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("RECON_REFRESH", 2), 0, 3)));
    comboColor.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("RECON_COLOR", 0), 0, 1)));
    checkBarbs = new QCheckBox{"Wind barbs", this};
    checkBarbs->setChecked(Utility::readPrefInt("RECON_BARBS", 1) != 0);
    map = new ReconMap{this};
    map->onDrop = [this] (const UtilityDropsonde::Drop& drop) { new DropsondeViewer{this, drop}; };
    map->setColoring(comboColor.getIndex() == 1);
    map->setBarbs(checkBarbs->isChecked());
    log = new QPlainTextEdit{this};
    log->setReadOnly(true);
    log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    log->setLineWrapMode(QPlainTextEdit::WidgetWidth);
    log->setMinimumWidth(380);
    comboMission.connect([this] {
        if (!rebuilding) {
            chooseMission();
        }
    });
    comboFloater.connect([this] {
        Utility::writePrefInt("RECON_FLOATER", comboFloater.getIndex());
        loadImage();
    });
    comboColor.connect([this] {
        Utility::writePrefInt("RECON_COLOR", comboColor.getIndex());
        map->setColoring(comboColor.getIndex() == 1);
    });
    comboRefresh.connect([this] {
        Utility::writePrefInt("RECON_REFRESH", comboRefresh.getIndex());
        setTimer();
    });
    QObject::connect(checkBarbs, &QCheckBox::toggled, [this] (bool on) {
        Utility::writePrefInt("RECON_BARBS", on ? 1 : 0);
        map->setBarbs(on);
    });
    buttonRefresh.connect([this] { refresh(false); });
    row.addWidget(comboMission, 1);
    row.addWidget(buttonRefresh);
    row2.addWidget(comboFloater);
    row2.addWidget(comboColor);
    row2.addWidget(comboRefresh);
    row2.addWidgetReal(checkBarbs);
    row2.addStretch();
    row2.addWidgetReal(new ActivityLabel{this});
    auto * split = new QWidget{this};
    auto * splitLayout = new QHBoxLayout{split};
    splitLayout->setContentsMargins(0, 0, 0, 0);
    splitLayout->addWidget(map, 3);
    splitLayout->addWidget(log, 2);
    box.addLayout(row);
    box.addLayout(row2);
    box.addWidget(textStatus);
    box.addWidgetReal(split, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1280, 760);
    QObject::connect(&timer, &QTimer::timeout, [this] { refresh(false); });
    setTimer();
    refresh(true);
}

void ReconViewer::setTimer() {
    timer.stop();
    const int seconds = refreshSeconds[std::clamp(comboRefresh.getIndex(), 0, 3)];
    if (seconds > 0) {
        timer.setInterval(seconds * 1000);
        timer.start();
    }
}

std::string ReconViewer::basinOf(const std::string& id) {
    return id.size() >= 2 ? id.substr(0, 2) : "al";
}

void ReconViewer::refresh(bool forceImage) {
    const int mine = ++generation;
    textStatus.setText("Loading the recon flights for " + (name.empty() ? stormId : name) + "...");
    auto r = std::make_shared<HurricaneData::ReconData>();
    auto s = std::make_shared<HurricaneData::StormData>();
    const auto id = stormId;
    const auto basin = basinOf(stormId) == "ep" ? "ep" : basinOf(stormId) == "cp" ? "cp" : "al";
    // first the flights and the storm, which put the track on the picture; then the vortex fixes and the dropsondes, which can follow
    new FutureVoid{this,
        [r, s, id, basin] {
            HurricaneData::loadRecon(*r, 60, basin);
            HurricaneData::loadStorm(id, *s);
        },
        [this, mine, r, s, forceImage, id, basin] {
            if (closed || mine != generation) {
                return;
            }
            recon = r;
            storm = s;
            applyData();
            const long now = QDateTime::currentSecsSinceEpoch();
            if (forceImage || image.isNull() || now - imageLoadedAt > 540) {   // the floater is new every ten minutes or so
                loadImage();
            }
            auto v = std::make_shared<HurricaneData::VdmData>();
            auto d = std::make_shared<HurricaneData::DropData>();
            new FutureVoid{this,
                [v, d, id, basin] {
                    HurricaneData::loadVdm(id, *v);
                    HurricaneData::loadDrops(*d, basin, 60);
                },
                [this, mine, v, d] {
                    if (closed || mine != generation) {
                        return;
                    }
                    vdm = v;
                    drops = d;
                    rebuild();
                }};
        }};
}

void ReconViewer::applyData() {
    // the flights: the HDOB missions that name this storm, or that flew within 450 km of it lately
    const auto stormName = upper(name);
    double lat = 0.0, lon = 0.0;
    bool haveStorm = false;
    if (storm && !storm->best.empty()) {
        lat = storm->best.back().lat;
        lon = storm->best.back().lon;
        haveStorm = true;
    }
    std::map<std::string, Mission> found;
    for (const auto& message : recon->messages) {
        const auto key = message.mission;
        if (message.obs.empty() || upper(key).find("TEST") != std::string::npos) {
            continue;
        }
        bool mine = !stormName.empty() && upper(key).find(stormName) != std::string::npos;
        if (!mine && haveStorm) {
            for (const auto& ob : message.obs) {
                const double dy = (ob.lat - lat) * 111.0, dx = (ob.lon - lon) * 111.0 * std::cos(lat * 3.14159265 / 180.0);
                if (std::hypot(dx, dy) < 450.0) {
                    mine = true;
                    break;
                }
            }
        }
        if (!mine) {
            continue;
        }
        auto& m = found[key];
        m.key = key;
        m.first = m.first == 0 ? message.obs.front().seconds : std::min(m.first, message.obs.front().seconds);
        m.last = std::max(m.last, message.obs.back().seconds);
        m.bulletins++;
    }
    missions.clear();
    for (auto& [key, m] : found) {
        m.label = key + "   " + dayClock(m.first).toStdString() + " - " + clock(m.last).toStdString() + "   (" + std::to_string(m.bulletins) + " bulletins)";
        missions.push_back(m);
    }
    std::sort(missions.begin(), missions.end(), [] (const auto& a, const auto& b) { return a.last > b.last; });
    // keep the mission that was chosen; otherwise the newest
    rebuilding = true;
    std::vector<std::string> labels;
    size_t index = 0;
    for (size_t i = 0; i < missions.size(); i++) {
        labels.push_back(missions[i].label);
        if (missions[i].key == selected) {
            index = i;
        }
    }
    if (labels.empty()) {
        labels.push_back("No flight found for this storm in the recent bulletins");
    }
    comboMission.setList(labels);
    comboMission.setIndex(index);
    rebuilding = false;
    selected = missions.empty() ? std::string{} : missions[index].key;
    rebuild();
}

void ReconViewer::chooseMission() {
    const auto index = static_cast<size_t>(std::max(0, comboMission.getIndex()));
    if (index < missions.size()) {
        selected = missions[index].key;
    }
    rebuild();
}

void ReconViewer::rebuild() {
    ReconMap::Flight flight;
    QString text;
    struct Entry {
        long seconds;
        QString text;
    };
    std::vector<Entry> entries;
    const auto aircraft = firstWords(selected, 2);
    if (recon && !selected.empty()) {
        std::set<long> seen;
        for (const auto& message : recon->messages) {
            if (message.mission != selected || message.obs.empty()) {
                continue;
            }
            double peakFlight = -1, peakSfmr = -1, lowestSurface = 9999;
            for (const auto& ob : message.obs) {
                if (seen.insert(ob.seconds).second) {
                    flight.obs.push_back(ob);
                }
                if (has(ob.windSpeed)) peakFlight = std::max(peakFlight, ob.windSpeed);
                if (has(ob.sfmrWind)) peakSfmr = std::max(peakSfmr, ob.sfmrWind);
                if (has(ob.surfacePressure)) lowestSurface = std::min(lowestSurface, ob.surfacePressure);
            }
            const auto& last = message.obs.back();
            QString line = "HDOB " + QString::number(message.number) + "  " + clock(message.obs.front().seconds) + "-" + clock(last.seconds) + "  at " + position(last.lat, last.lon) + "\n    ";
            QStringList parts;
            if (peakFlight >= 0) parts << "flight-level wind up to " + QString::number(static_cast<int>(std::lround(peakFlight))) + " kt";
            if (peakSfmr >= 0) parts << "surface (SFMR) up to " + QString::number(static_cast<int>(std::lround(peakSfmr))) + " kt";
            if (lowestSurface < 9000) parts << "lowest extrapolated surface pressure " + QString::number(lowestSurface, 'f', 1) + " mb";
            entries.push_back({last.seconds, line + (parts.isEmpty() ? QString{"no wind or pressure reported"} : parts.join("; "))});
        }
        std::sort(flight.obs.begin(), flight.obs.end(), [] (const auto& a, const auto& b) { return a.seconds < b.seconds; });
    }
    if (vdm && !aircraft.empty()) {
        for (const auto& fix : vdm->messages) {
            if (firstWords(fix.aircraft, 2) != aircraft) {
                continue;
            }
            flight.fixes.push_back(fix);
            QString line = "VORTEX FIX  " + clock(fix.seconds) + "  centre " + (has(fix.lat) ? position(fix.lat, fix.lon) : QString{"?"}) + "\n    ";
            QStringList parts;
            if (has(fix.pressure)) parts << "minimum pressure " + QString::number(static_cast<int>(std::lround(fix.pressure))) + " mb" + (fix.extrapolated ? " (extrapolated)" : " (dropsonde)");
            if (has(fix.maxFlightWind())) parts << "max flight-level wind " + QString::number(static_cast<int>(std::lround(fix.maxFlightWind()))) + " kt";
            if (has(fix.inboundSurface.kt)) parts << "surface in " + QString::number(static_cast<int>(std::lround(fix.inboundSurface.kt))) + " kt";
            if (has(fix.outboundSurface.kt)) parts << "surface out " + QString::number(static_cast<int>(std::lround(fix.outboundSurface.kt))) + " kt";
            if (!fix.eyeCharacter.empty()) parts << "eye " + QString::fromStdString(fix.eyeCharacter) + (fix.eyeShape.empty() ? "" : " " + QString::fromStdString(fix.eyeShape));
            entries.push_back({fix.seconds, line + parts.join("; ")});
        }
    }
    if (drops && !aircraft.empty()) {
        for (const auto& drop : drops->drops) {
            if (firstWords(drop.mission, 2) != aircraft) {
                continue;
            }
            flight.drops.push_back(drop);
            QString line = "DROPSONDE  " + clock(drop.seconds) + "  at " + (has(drop.lat) ? position(drop.lat, drop.lon) : QString{"?"}) + "\n    ";
            QStringList parts;
            const double pressure = UtilityDropsonde::minimumPressure(drop), wind = UtilityDropsonde::maxWind(drop);
            if (has(pressure)) parts << "pressure " + QString::number(static_cast<int>(std::lround(pressure))) + " mb";
            if (has(wind)) parts << "strongest wind " + QString::number(static_cast<int>(std::lround(wind))) + " kt";
            if (has(drop.mblSpeed)) parts << "mean boundary-layer wind " + QString::number(static_cast<int>(std::lround(drop.mblDirection))) + " deg at " + QString::number(static_cast<int>(std::lround(drop.mblSpeed))) + " kt";
            entries.push_back({drop.seconds, line + (parts.isEmpty() ? QString{"no data"} : parts.join("; "))});
        }
    }
    std::sort(entries.begin(), entries.end(), [] (const auto& a, const auto& b) { return a.seconds > b.seconds; });   // newest first
    for (const auto& e : entries) {
        text += e.text + "\n\n";
    }
    log->setPlainText(entries.empty() ? QString{"Nothing reported yet."} : text.trimmed());
    if (storm && !storm->best.empty()) {
        flight.haveStorm = true;
        flight.stormLat = storm->best.back().lat;
        flight.stormLon = storm->best.back().lon;
        const size_t count = storm->best.size();
        for (size_t i = count > 12 ? count - 12 : 0; i < count; i++) {   // about three days of 6 hourly fixes
            flight.stormTrack.emplace_back(storm->best[i].lat, storm->best[i].lon);
        }
    }
    map->setFlight(flight);
    textStatus.setText((name.empty() ? stormId : name) + (selected.empty() ? "  -  no flight in the recent bulletins" : "  -  " + selected) + "   updated " +
                       QDateTime::currentDateTimeUtc().toString("HH:mm:ss").toStdString() + "Z   (" + std::to_string(flight.obs.size()) + " observations, " +
                       std::to_string(flight.fixes.size()) + " fixes, " + std::to_string(flight.drops.size()) + " dropsondes)");
}

void ReconViewer::loadImage() {
    const auto folder = std::string{floaters[std::clamp(comboFloater.getIndex(), 0, 5)].folder};
    const auto url = "https://cdn.star.nesdis.noaa.gov/FLOATER/data/" + upper(stormId) + "/" + folder + "/1000x1000.jpg";
    new FutureBytes{this, url, [this] (const QByteArray& bytes) {
        if (closed) {
            return;
        }
        QImage picture;
        if (bytes.size() < 500 || !picture.loadFromData(bytes)) {
            image = QImage{};
            geo = FloaterGeo::Geo{};
            map->setImage(image, geo);
            return;
        }
        imageLoadedAt = QDateTime::currentSecsSinceEpoch();
        const auto gray = picture.convertToFormat(QImage::Format_RGB32);
        std::vector<unsigned char> green(static_cast<size_t>(gray.width()) * static_cast<size_t>(gray.height()));
        for (int y = 0; y < gray.height(); y++) {
            const auto * row = reinterpret_cast<const QRgb *>(gray.constScanLine(y));
            for (int x = 0; x < gray.width(); x++) {
                green[static_cast<size_t>(y) * static_cast<size_t>(gray.width()) + static_cast<size_t>(x)] = static_cast<unsigned char>(qGreen(row[x]));
            }
        }
        const double lat = storm && !storm->best.empty() ? storm->best.back().lat : 20.0;
        const double lon = storm && !storm->best.empty() ? storm->best.back().lon : -60.0;
        image = picture;
        geo = FloaterGeo::locate(green.data(), gray.width(), gray.height(), lon, lat);
        map->setImage(image, geo);
    }};
}
