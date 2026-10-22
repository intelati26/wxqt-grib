// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MarkText.h"
#include "mapkit/BufkitStationsLayer.h"
#include <algorithm>
#include <cmath>
#include <set>
#include <QFontMetricsF>
#include <QCheckBox>
#include <QTimeZone>
#include <QVBoxLayout>
#include "models/HodographGlyph.h"
#include "models/SoundingViewer.h"
#include "objects/FutureVoid.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "radar/MapView.h"
#include "sounding/UtilityBufkit.h"

namespace {
    constexpr double spacing = 34.0;   // pixels between two buttons (more for the hodographs)
    constexpr size_t most = 220;
}

// the stations in view that get a button: offices first, then the rest, none within `spacing` pixels of another
std::vector<BufkitStationsLayer::Shown> BufkitStationsLayer::choose(MapHost& host) const {
    std::vector<Shown> out;
    auto& view = host.view();
    const auto * widget = view.map();
    if (widget == nullptr) {
        return out;
    }
    const auto& offices = UtilityBufkit::officeStations();
    std::vector<const UtilityBufkit::Station *> order;
    for (const auto& station : UtilityBufkit::stations()) {
        order.push_back(&station);
    }
    std::stable_sort(order.begin(), order.end(), [&offices] (const auto * a, const auto * b) { return offices.count(a->id) > offices.count(b->id); });
    std::set<std::pair<int, int>> taken;
    const double w = view.pixelWidth(), h = view.pixelHeight();
    for (const auto * station : order) {
        const auto p = view.toPixels(station->latitude, station->longitude);
        if (p.x() < 0 || p.y() < 0 || p.x() > w || p.y() > h) {
            continue;
        }
        const double gap = hodographs ? 88.0 : spacing;
        const int cx = static_cast<int>(p.x() / gap), cy = static_cast<int>(p.y() / gap);
        bool near = false;
        for (int dx = -1; dx <= 1 && !near; dx++) {
            for (int dy = -1; dy <= 1 && !near; dy++) {
                near = taken.count({cx + dx, cy + dy}) > 0;
            }
        }
        if (near) {
            continue;
        }
        taken.insert({cx, cy});
        out.push_back({station, view.transform()(station->latitude, station->longitude), p, station->id, station->latitude, station->longitude});
        if (out.size() >= most) {
            break;
        }
    }
    return out;
}

void BufkitStationsLayer::paint(QPainter& painter, MapHost& host) {
    const double perPixel = host.view().unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(10.0 * perPixel))));
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics{font};
    for (const auto& s : choose(host)) {
        if (hodographs) {
            std::vector<QPointF> tips;
            QPointF storm;
            double srh = -9999.0;
            if (glyph(s.id, tips, storm, srh)) {
                if (host.deferMarks()) {   // a projected world picture: the glyph is painted there, upright
                    host.deferMark(s.lat, s.lon, [tips, storm] (QPainter& p, double unit, double) {
                        const double size = 80.0 * unit;
                        HodographGlyph::paint(p, QRectF{-size / 2.0, -size / 2.0, size, size}, tips, storm);
                    });
                    continue;
                }
                const double size = 80.0 * perPixel;
                HodographGlyph::paint(painter, QRectF{s.units.x() - size / 2.0, s.units.y() - size / 2.0, size, size}, tips, storm);
                continue;
            }
            pump(host);
        }
        const double r = 4.5 * perPixel;
        painter.setPen(QPen{QColor{255, 255, 255}, 1.4 * perPixel});
        painter.setBrush(QColor{200, 70, 30});
        painter.drawEllipse(s.units, r, r);
        MarkText::tag(host, painter, perPixel, s.lat, s.lon, QPointF{r / perPixel + 1.5, 0.0}, QString::fromStdString(s.id), QColor{20, 20, 28, 170}, QColor{240, 240, 240}, 2.0, 2.0, 2.0);
    }
}

MapHit BufkitStationsLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 12.0;
    hit.priority = 20;
    for (const auto& s : choose(host)) {
        const double distance = std::hypot(s.pixels.x() - pixels.x(), s.pixels.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(s.id + "\nClick for the RRFS sounding");
            std::vector<QPointF> tips;
            QPointF storm;
            double srh = -9999.0;
            if (hodographs && glyph(s.id, tips, storm, srh) && srh > -9000.0) {
                hit.text = QString::fromStdString(s.id) + QString{"\n0-3 km helicity %1 m2/s2 (right mover)\nClick for the RRFS sounding"}.arg(srh, 0, 'f', 0);
            }
            const string id = s.id;
            hit.open = [id] (Window * parent) { new SoundingViewer{parent, "RRFS", id, true}; };
        }
    }
    return hit;
}

string BufkitStationsLayer::summary() const {
    return std::to_string(UtilityBufkit::stations().size()) + " stations";
}

BufkitStationsLayer::BufkitStationsLayer() {
    hodographs = Utility::readPref("MM_STATIONS_HODOGRAPHS", "false").compare(0, 1, "t") == 0;
}

QWidget * BufkitStationsLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * box = new QCheckBox{"Hodographs (the RRFS winds, surface to 9 km)", widget};
    box->setChecked(hodographs);
    box->setToolTip("Each station draws a small hodograph of the RRFS forecast winds for the hour on the time bar (pink to 1 km, red to 3, green to 6, yellow above; the dot is the right-mover storm motion). The station files are read as the stations come into view");
    layout->addWidget(box);
    QObject::connect(box, &QCheckBox::toggled, [this, changed] (bool on) {
        hodographs = on;
        Utility::writePref("MM_STATIONS_HODOGRAPHS", on ? "true" : "false");
        changed();
    });
    return widget;
}

// the seconds of a BUFKIT valid time ("261015/1800")
static long secondsOf(const string& text) {
    return static_cast<long>(QDateTime::fromString(QString::fromStdString("20" + text), "yyyyMMdd/HHmm").toSecsSinceEpoch());
}

long BufkitStationsLayer::validNow() const {
    return shown != 0 ? shown : QDateTime::currentSecsSinceEpoch();
}

vector<long> BufkitStationsLayer::times() const {
    vector<long> out;
    for (const auto& [id, run] : runs) {   // the first file that is in: all the stations of a run have the same hours
        for (const auto& v : run->validTimes) {
            out.push_back(secondsOf(v));
        }
        break;
    }
    return out;
}

string BufkitStationsLayer::timeText() const {
    return QDateTime::fromSecsSinceEpoch(validNow(), QTimeZone::utc()).toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  RRFS hodographs";
}

bool BufkitStationsLayer::glyph(const string& station, std::vector<QPointF>& tips, QPointF& storm, double& srh) const {
    const auto run = runs.find(station);
    if (run == runs.end()) {
        return false;
    }
    const long want = validNow();
    long best = -1;
    size_t at = 0;
    for (size_t i = 0; i < run->second->validTimes.size(); i++) {
        const long t = secondsOf(run->second->validTimes[i]);
        if (t <= want + 1800 && t > best) {
            best = t;
            at = i;
        }
    }
    if (best < 0 || want - best > 2 * 3600 || at >= run->second->profiles.size()) {
        return false;
    }
    return HodographGlyph::measure(run->second->profiles[at], tips, storm, srh);
}

// the files of the stations in view, four at a time
void BufkitStationsLayer::pump(MapHost& host) const {
    for (const auto& s : choose(host)) {
        if (runs.count(s.id) == 0 && asked.insert(s.id).second) {
            queue.push_back(s.id);
        }
    }
    while (running < 4 && !queue.empty()) {
        const string id = queue.front();
        queue.pop_front();
        running++;
        auto run = std::make_shared<UtilityBufkit::Run>();
        auto ok = std::make_shared<bool>(false);
        auto * self = const_cast<BufkitStationsLayer *>(this);
        host.background(
            [run, ok, id] {
                string error;
                *ok = UtilityBufkit::fetchLatest("RRFS", id, *run, error);
            },
            [self, &host, run, ok, id, alive = alive] {
                if (!*alive) {
                    return;
                }
                self->running--;
                if (*ok) {
                    const bool first = self->runs.empty();
                    self->runs[id] = run;
                    if (first) {
                        host.timesChanged();
                    }
                    host.redraw();
                }
                self->pump(host);
            });
    }
}
