// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SolarFleetViewer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <ctime>
#include <functional>
#include <future>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPainterPath>
#include <QTimeZone>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QUrl>
#include "misc/ImageViewer.h"
#include "objects/FutureVoid.h"
#include "space/SpaceData.h"
#include "space/SunViewer.h"
#include "space/UtilitySpace.h"
#include "ui/UiStandards.h"
#include "util/UtilityIO.h"
#include "vis/GoesViewer.h"

namespace {
    constexpr double pi = 3.14159265358979323846;
    constexpr double l1Km = 1.5e6;           // the Earth to L1, toward the Sun
    constexpr double auKm = 1.495978707e8;
    constexpr double sunRotation = 2.865e-6; // rad/s, the Sun's rotation as the solar wind sees it (sidereal)
    constexpr double windAssumed = 400.0;    // km/s, the speed the field lines are drawn for

    enum Zone { Helio, L1, Geo, Leo };

    struct Link {
        QString label;
        std::function<void(Window *)> open;
    };
    struct Craft {
        QString name;
        QString role;           // what it is for
        QString instruments;
        Zone zone;
        int horizons;           // JPL Horizons id, heliocentric craft only
        QColor color;
        std::vector<Link> links;
    };
    struct Body {
        QString name;
        int id;
        bool craft;
        std::vector<std::array<double, 3>> track;   // AU, ecliptic, Sun at the origin, one a day, the last is now
    };

    const std::vector<Body>& bodiesWanted() {
        static const std::vector<Body> bodies{{"Mercury", 199, false, {}}, {"Venus", 299, false, {}}, {"Earth", 399, false, {}}, {"STEREO-A", -234, true, {}}, {"Parker Solar Probe", -96, true, {}},
            {"Solar Orbiter", -144, true, {}}};
        return bodies;
    }

    const std::vector<Craft>& fleet() {
        const auto lasco = Link{"LASCO C2 corona", [] (Window * w) { new ImageViewer{w, SpaceData::url("images/animations/lasco-c2/latest.jpg"), "SOHO LASCO C2"}; }};
        const auto suvi304 = Link{"SUVI 304 A", [] (Window * w) { new GoesViewer{w, "", "He304", "SUVI", false}; }};
        const auto suvi195 = Link{"SUVI 195 A", [] (Window * w) { new GoesViewer{w, "", "Fe195", "SUVI", false}; }};
        const auto ccor = Link{"CCOR-1 corona", [] (Window * w) { new ImageViewer{w, SpaceData::url("images/animations/ccor1/latest.jpg"), "GOES CCOR-1"}; }};
        const auto sun = Link{"The Sun (SDO) with its regions", [] (Window * w) { new SunViewer{w}; }};
        static const std::vector<Craft> all{
            {"STEREO-A", "The Sun and CMEs seen from the side", "EUVI (ultraviolet Sun), COR1 and COR2 (corona), HI1 and HI2 (heliospheric imagers), PLASTIC and IMPACT (wind and field)", Helio, -234, QColor{255, 170, 60}, {}},
            {"Parker Solar Probe", "Flies into the corona", "WISPR (white light imager), FIELDS (fields and waves), SWEAP (wind particles), IS\xE2\x98\x89IS (energetic particles)", Helio, -96, QColor{255, 110, 110}, {}},
            {"Solar Orbiter", "The Sun's poles and the wind near it", "EUI and PHI (ultraviolet, magnetograms), Metis and SoloHI (corona), SPICE and STIX (spectra, X-rays), in situ wind and field", Helio, -144, QColor{190, 130, 255}, {}},
            {"SOHO", "At L1: the corona for 30 years", "LASCO C2 and C3 (corona and CMEs), EIT (ultraviolet), SWAN, plus wind particles", L1, 0, QColor{255, 220, 90}, {lasco}},
            {"DSCOVR", "At L1: the wind in front of the Earth", "Faraday cup (wind speed, density, temperature), magnetometer (Bz), the Earth view camera", L1, 0, QColor{110, 220, 255}, {}},
            {"ACE", "At L1: the wind, the field and the particles", "SWEPAM, MAG, EPAM, SIS, CRIS", L1, 0, QColor{120, 230, 160}, {}},
            {"Wind", "At L1: wind, field and waves", "SWE, MFI, 3DP, WAVES", L1, 0, QColor{160, 190, 255}, {}},
            {"SWFO-L1", "At L1: NOAA's new space weather monitor", "CCOR-2 (corona), the solar wind and field sensors", L1, 0, QColor{255, 150, 200}, {}},
            {"GOES-19 (East) and GOES-18 (West)", "Geosynchronous: the Sun's X-rays, the particles and the field at the Earth", "SUVI (ultraviolet Sun), EXIS (X-ray flux), SEISS (protons and electrons), MAG; GOES-19 also CCOR-1 (corona)", Geo, 0, QColor{90, 200, 255},
                {suvi304, suvi195, ccor}},
            {"SDO", "Geosynchronous: the Sun's disc every 12 seconds", "AIA (ultraviolet, 10 bands), HMI (visible light and magnetograms), EVE (ultraviolet flux)", Geo, 0, QColor{255, 190, 70}, {sun}},
            {"Hinode", "Low orbit: the sun's magnetic fields in detail", "SOT (optical), XRT (X-ray telescope), EIS (ultraviolet spectra)", Leo, 0, QColor{200, 200, 120}, {}},
            {"IRIS", "Low orbit: the chromosphere", "Ultraviolet spectrograph and slit-jaw imager", Leo, 0, QColor{170, 220, 170}, {}},
            {"PROBA2", "Low orbit: the Sun's ultraviolet and X-ray output", "SWAP (ultraviolet imager), LYRA (radiometer)", Leo, 0, QColor{220, 170, 140}, {}},
        };
        return all;
    }

    // one body's positions from JPL Horizons: the last thirty days to now, a day apart (a text line is "JD, date, x, y, z, ")
    std::vector<std::array<double, 3>> horizonsTrack(int id, long now) {
        const auto stamp = [] (long t) { return QUrl::toPercentEncoding(QDateTime::fromSecsSinceEpoch(t, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm")); };
        const std::string url = "https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='" + std::to_string(id) + "'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='VECTORS'&CENTER='500@10'&START_TIME='" +
            stamp(now - 30 * 86400).toStdString() + "'&STOP_TIME='" + stamp(now).toStdString() + "'&STEP_SIZE='1d'&VEC_TABLE='1'&REF_PLANE='ECLIPTIC'&OUT_UNITS='AU-D'&CSV_FORMAT='YES'";
        const auto text = UtilityIO::downloadAsByteArray(url);
        const int start = text.indexOf("$$SOE"), end = text.indexOf("$$EOE");
        std::vector<std::array<double, 3>> track;
        if (start < 0 || end < start) {
            return track;
        }
        for (const auto& line : text.mid(start + 5, end - start - 5).split('\n')) {
            const auto parts = line.split(',');
            bool a = false, b = false, c = false;
            if (parts.size() >= 5) {
                const double x = parts[2].trimmed().toDouble(&a), y = parts[3].trimmed().toDouble(&b), z = parts[4].trimmed().toDouble(&c);
                if (a && b && c) {
                    track.push_back({x, y, z});
                }
            }
        }
        return track;
    }

    double longitude(const std::array<double, 3>& p) { return std::atan2(p[1], p[0]); }
    double radius(const std::array<double, 3>& p) { return std::hypot(p[0], p[1]); }
    double wrap(double a) {
        while (a > pi) a -= 2.0 * pi;
        while (a <= -pi) a += 2.0 * pi;
        return a;
    }
}

// the picture: the plan view over the near-Earth strip
class FleetCanvas : public QWidget {
public:
    explicit FleetCanvas(QWidget * parent) : QWidget{parent} {
        setMinimumSize(560, 600);
    }
    void set(const std::vector<Body>& b, double windSpeed) {
        bodies = b;
        wind = windSpeed;
        loaded = true;
        update();
    }
    void choose(int craft) {
        chosen = craft;
        update();
    }
    std::function<void(int)> picked;

protected:
    void mousePressEvent(QMouseEvent * event) override {
        int best = -1;
        double bestDistance = 14.0;
        for (const auto& [at, index] : hits) {
            const double d = std::hypot(at.x() - event->position().x(), at.y() - event->position().y());
            if (d < bestDistance) {
                bestDistance = d;
                best = index;
            }
        }
        if (best >= 0 && picked) {
            picked(best);
        }
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor{9, 13, 28});
        hits.clear();
        QFont base{p.font()};
        base.setPixelSize(11);
        p.setFont(base);
        if (!loaded) {
            p.setPen(QColor{170, 175, 190});
            p.drawText(rect(), Qt::AlignCenter, "Reading the positions (JPL Horizons)...");
            return;
        }
        const double splitY = height() * 0.68;
        paintHelio(p, QRectF{0, 0, static_cast<double>(width()), splitY});
        p.setPen(QColor{50, 58, 90});
        p.drawLine(QPointF{0, splitY}, QPointF{static_cast<double>(width()), splitY});
        paintNearEarth(p, QRectF{0, splitY, static_cast<double>(width()), height() - splitY});
    }

private:
    const Body * find(int id) const {
        for (const auto& b : bodies) {
            if (b.id == id) {
                return &b;
            }
        }
        return nullptr;
    }
    void ring(QPainter& p, const QPointF& at, double r, const QColor& color, bool selected) {
        if (selected) {
            p.setPen(QPen{QColor{255, 255, 255}, 2.0});
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(at, r + 5.0, r + 5.0);
        }
        p.setPen(QPen{QColor{20, 20, 30}, 1.0});
        p.setBrush(color);
        p.drawEllipse(at, r, r);
    }
    void label(QPainter& p, const QPointF& at, const QString& text, const QColor& color, Qt::Alignment align = Qt::AlignLeft) {
        p.setPen(color);
        const int w = 170;
        const QRectF box{align == Qt::AlignRight ? at.x() - w - 8.0 : at.x() + 8.0, at.y() - 8.0, static_cast<double>(w), 16.0};
        p.drawText(box, align | Qt::AlignVCenter, text);
    }
    void paintHelio(QPainter& p, const QRectF& area) {
        const QPointF sun{area.center()};
        const double extent = 1.18;   // AU from the middle to the nearest edge
        const double scale = std::min(area.width(), area.height()) / 2.0 / extent;
        const auto at = [&] (const std::array<double, 3>& v) { return QPointF{sun.x() + v[0] * scale, sun.y() - v[1] * scale}; };
        p.setPen(QColor{200, 205, 225});
        QFont bold{p.font()};
        bold.setBold(true);
        bold.setPixelSize(12);
        p.setFont(bold);
        p.drawText(QPointF{10, 18}, "The inner solar system from above the ecliptic (JPL Horizons, now)");
        QFont small{p.font()};
        small.setBold(false);
        small.setPixelSize(11);
        p.setFont(small);
        // the orbits
        for (const auto& [au, name] : {std::pair{0.387, "Mercury"}, std::pair{0.723, "Venus"}, std::pair{1.0, "Earth's orbit"}}) {
            p.setPen(QPen{QColor{60, 68, 100}, 1.0, Qt::DotLine});
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(sun, au * scale, au * scale);
        }
        // the Sun
        QRadialGradient glow{sun, 22.0};
        glow.setColorAt(0.0, QColor{255, 240, 150, 255});
        glow.setColorAt(0.3, QColor{255, 200, 60, 200});
        glow.setColorAt(1.0, QColor{255, 160, 0, 0});
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(sun, 22.0, 22.0);
        p.setBrush(QColor{255, 225, 110});
        p.drawEllipse(sun, 7.0, 7.0);
        // the field line through each spacecraft and the Earth: a spiral, the wind carrying the Sun's rotation outward
        const double k = sunRotation * auKm / windAssumed;   // radians per AU
        const auto spiral = [&] (const Body& b, const QColor& color, double alpha) {
            if (b.track.empty()) {
                return;
            }
            const auto& now = b.track.back();
            const double r0 = radius(now), lon0 = longitude(now);
            QPainterPath path;
            for (double r = r0; r > 0.06; r -= 0.01) {
                const double lon = lon0 + k * (r0 - r);
                const QPointF point{sun.x() + r * std::cos(lon) * scale, sun.y() - r * std::sin(lon) * scale};
                if (r == r0) {
                    path.moveTo(point);
                } else {
                    path.lineTo(point);
                }
            }
            QColor c = color;
            c.setAlphaF(alpha);
            p.setPen(QPen{c, 1.4, Qt::DashLine});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        if (const auto * earth = find(399)) {
            spiral(*earth, QColor{90, 170, 255}, 0.85);
        }
        for (const auto& craft : fleet()) {
            if (craft.zone == Helio) {
                if (const auto * b = find(craft.horizons)) {
                    spiral(*b, craft.color, 0.55);
                }
            }
        }
        // the tracks and the bodies
        for (const auto& b : bodies) {
            if (b.track.empty()) {
                continue;
            }
            QColor color{170, 175, 190};
            int craftIndex = -1;
            for (size_t i = 0; i < fleet().size(); i++) {
                if (fleet()[i].zone == Helio && fleet()[i].horizons == b.id) {
                    color = fleet()[i].color;
                    craftIndex = static_cast<int>(i);
                }
            }
            if (b.id == 399) {
                color = QColor{90, 170, 255};
            }
            QPainterPath path;
            for (size_t i = 0; i < b.track.size(); i++) {
                const auto point = at(b.track[i]);
                i == 0 ? path.moveTo(point) : path.lineTo(point);
            }
            QColor trail = color;
            trail.setAlphaF(0.7);
            p.setPen(QPen{trail, 2.0});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            const auto here = at(b.track.back());
            ring(p, here, b.craft ? 5.0 : (b.id == 399 ? 6.0 : 4.0), color, craftIndex >= 0 && craftIndex == chosen);
            if (craftIndex >= 0) {
                hits.push_back({here, craftIndex});
            }
            const bool left = here.x() < sun.x();
            label(p, here, b.name, color, left ? Qt::AlignRight : Qt::AlignLeft);
        }
        // the key
        p.setPen(QColor{150, 156, 180});
        p.drawText(QRectF{10, area.bottom() - 34, area.width() - 20, 30}, Qt::AlignLeft | Qt::AlignBottom | Qt::TextWordWrap,
            "Dashed: the magnetic field line each one is on, for a " + QString::number(windAssumed, 'f', 0) + " km/s wind. Orbits run counter-clockwise; the tracks are the last 30 days. Click a spacecraft.");
    }
    // the strip near the Earth: the Sun to the left; L1 to scale, the rest as rings round the Earth
    void paintNearEarth(QPainter& p, const QRectF& area) {
        QFont bold{p.font()};
        bold.setBold(true);
        bold.setPixelSize(12);
        p.setFont(bold);
        p.setPen(QColor{200, 205, 225});
        p.drawText(QPointF{10, area.top() + 18}, "Near the Earth (the distance to L1 is to scale; the Earth's rings are not)");
        QFont small{p.font()};
        small.setBold(false);
        small.setPixelSize(11);
        p.setFont(small);
        const double mid = area.top() + area.height() * 0.56;
        const double earthX = area.right() - 70.0;
        const double perKm = (earthX - 80.0) / (l1Km * 1.12);
        const double l1X = earthX - l1Km * perKm;
        // the Sun's direction
        p.setPen(QColor{255, 215, 90});
        p.drawText(QPointF{10, mid - 14}, "to the Sun");
        p.drawLine(QPointF{10, mid}, QPointF{66, mid});
        p.drawLine(QPointF{10, mid}, QPointF{18, mid - 4});
        p.drawLine(QPointF{10, mid}, QPointF{18, mid + 4});
        // the wind line, the bow shock, the Moon's orbit, the Earth
        p.setPen(QPen{QColor{60, 68, 100}, 1.0, Qt::DotLine});
        p.drawLine(QPointF{70, mid}, QPointF{earthX, mid});
        const double moon = 384000.0 * perKm;
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF{earthX, mid}, moon, moon);
        p.setPen(QPen{QColor{90, 130, 210}, 1.2});
        const double shock = 95000.0 * perKm + 14.0;
        p.drawArc(QRectF{earthX - shock, mid - shock * 0.9, shock * 2, shock * 1.8}, 90 * 16, 180 * 16);
        p.setPen(QColor{90, 130, 210});
        p.drawText(QPointF{earthX - shock - 2, mid - shock * 0.9 - 4}, "bow shock");
        p.setPen(QColor{100, 106, 140});
        p.drawText(QRectF{earthX - moon - 124, mid - 16, 120, 14}, Qt::AlignRight, "the Moon's orbit");
        ring(p, QPointF{earthX, mid}, 4.0, QColor{90, 170, 255}, false);
        // L1
        p.setPen(QPen{QColor{255, 255, 255, 70}, 1.0, Qt::DashLine});
        p.drawLine(QPointF{l1X, mid - 50}, QPointF{l1X, mid + 50});
        p.setPen(QColor{190, 195, 215});
        p.drawText(QRectF{l1X - 90, mid + 52, 180, 14}, Qt::AlignHCenter, "L1: 1.5 million km sunward");
        // the satellites
        int l1Count = 0, leoCount = 0;
        for (const auto& craft : fleet()) {
            l1Count += craft.zone == L1;
            leoCount += craft.zone == Leo;
        }
        int l1Seen = 0, geoSeen = 0, leoSeen = 0;
        for (size_t i = 0; i < fleet().size(); i++) {
            const auto& craft = fleet()[i];
            QPointF here;
            if (craft.zone == L1) {   // spread over the halo orbit
                const double angle = 2.0 * pi * l1Seen / std::max(1, l1Count);
                here = QPointF{l1X + 14.0 * std::cos(angle), mid + 30.0 * std::sin(angle)};
                l1Seen++;
                label(p, here, craft.name, craft.color, std::cos(angle) >= 0 ? Qt::AlignLeft : Qt::AlignRight);
            } else if (craft.zone == Geo) {
                const double angle = pi * (0.7 + 0.6 * geoSeen);
                here = QPointF{earthX + 17.0 * std::cos(angle), mid - 17.0 * std::sin(angle)};
                geoSeen++;
            } else if (craft.zone == Leo) {
                const double angle = 2.0 * pi * (leoSeen + 0.5) / std::max(1, leoCount) + pi * 0.5;
                here = QPointF{earthX + 9.0 * std::cos(angle), mid - 9.0 * std::sin(angle)};
                leoSeen++;
            } else {
                continue;
            }
            ring(p, here, craft.zone == L1 ? 4.5 : 3.0, craft.color, static_cast<int>(i) == chosen);
            hits.push_back({here, static_cast<int>(i)});
        }
        // the labels at the Earth
        p.setPen(QColor{190, 195, 215});
        p.drawText(QRectF{area.right() - 420, mid + 30, 410, 14}, Qt::AlignRight, "At the Earth: GOES and SDO (geosynchronous), Hinode, IRIS, PROBA2 (low orbit)");
        if (wind > 0.0) {
            const int minutes = static_cast<int>(std::lround(l1Km / wind / 60.0));
            p.setPen(QColor{255, 215, 90});
            p.drawText(QRectF{10, area.bottom() - 20, area.width() - 20, 16}, Qt::AlignLeft | Qt::AlignVCenter,
                "The wind now: " + QString::number(wind, 'f', 0) + " km/s. It takes about " + QString::number(minutes) + " minutes from L1 to the Earth: that is the warning the L1 spacecraft give.");
        }
    }
    std::vector<Body> bodies;
    double wind{0.0};
    bool loaded{false};
    int chosen{-1};
    std::vector<std::pair<QPointF, int>> hits;
};

SolarFleetViewer::SolarFleetViewer(Window * parent)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Who is watching the Sun (JPL Horizons, NOAA SWPC)");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    auto * refresh = UiStandards::button(top, "Refresh", "Read the positions again");
    status = new QLabel{"Reading the positions...", top};
    UiStandards::muted(status);
    topLayout->addWidget(refresh);
    topLayout->addWidget(status, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    auto * body = new QWidget{this};
    auto * bodyLayout = new QHBoxLayout{body};
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    canvas = new FleetCanvas{body};
    bodyLayout->addWidget(canvas, 1);
    auto * side = new QWidget{body};
    side->setFixedWidth(330);
    auto * sideLayout = new QVBoxLayout{side};
    sideLayout->setContentsMargins(0, 0, 0, 0);
    list = new QListWidget{side};
    list->setWordWrap(true);
    list->setSpacing(1);
    detail = new QLabel{"Pick a spacecraft in the list or on the picture for its instruments.", side};
    detail->setWordWrap(true);
    detail->setTextFormat(Qt::RichText);
    links = new QWidget{side};
    linksLayout = new QVBoxLayout{links};
    linksLayout->setContentsMargins(0, 0, 0, 0);
    sideLayout->addWidget(list, 3);
    sideLayout->addWidget(detail, 2);
    sideLayout->addWidget(links);
    bodyLayout->addWidget(side);
    box.addWidgetReal(body, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1180, 760);
    for (const auto& craft : fleet()) {
        list->addItem(craft.name + "\n" + craft.role);
    }
    canvas->picked = [this] (int row) { list->setCurrentRow(row); };
    QObject::connect(list, &QListWidget::currentRowChanged, this, [this] (int row) { select(row); });
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { load(); });
    load();
}

void SolarFleetViewer::select(int row) {
    canvas->choose(row);
    while (auto * item = linksLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (row < 0 || row >= static_cast<int>(fleet().size())) {
        return;
    }
    const auto& craft = fleet()[static_cast<size_t>(row)];
    detail->setText("<b>" + craft.name.toHtmlEscaped() + "</b><br>" + craft.role.toHtmlEscaped() + "<br><br>" + craft.instruments.toHtmlEscaped());
    for (const auto& link : craft.links) {
        auto * button = UiStandards::button(links, link.label, "What it sees, open here");
        const auto open = link.open;
        QObject::connect(button, &QPushButton::clicked, this, [this, open] { open(this); });
        linksLayout->addWidget(button);
    }
}

void SolarFleetViewer::load() {
    const int mine = ++generation;
    status->setText("Reading the positions...");
    auto bodies = std::make_shared<std::vector<Body>>(bodiesWanted());
    auto wind = std::make_shared<double>(0.0);
    new FutureVoid{this, [bodies, wind] {
        const long now = static_cast<long>(std::time(nullptr));
        std::vector<std::future<std::vector<std::array<double, 3>>>> tracks;
        for (const auto& b : *bodies) {
            const int id = b.id;
            tracks.push_back(std::async(std::launch::async, [id, now] { return horizonsTrack(id, now); }));
        }
        for (size_t i = 0; i < bodies->size(); i++) {
            (*bodies)[i].track = tracks[i].get();
        }
        const auto points = UtilitySpace::parseWind(UtilityIO::downloadAsByteArray(SpaceData::url("json/rtsw/rtsw_wind_1m.json")).toStdString(), 5);
        for (auto it = points.rbegin(); it != points.rend(); ++it) {
            if (UtilitySpace::has(it->value)) {
                *wind = it->value;
                break;
            }
        }
    }, [this, mine, bodies, wind] {
        if (closed || mine != generation) {
            return;
        }
        const bool any = std::any_of(bodies->begin(), bodies->end(), [] (const Body& b) { return !b.track.empty(); });
        canvas->set(*bodies, *wind);
        if (!any) {
            status->setText("JPL Horizons could not be reached; the near-Earth strip is still drawn.");
            return;
        }
        // where each spacecraft is against the Earth, in words
        QString text;
        const Body * earth = nullptr;
        for (const auto& b : *bodies) {
            if (b.id == 399 && !b.track.empty()) {
                earth = &b;
            }
        }
        for (size_t i = 0; i < fleet().size(); i++) {
            const auto& craft = fleet()[i];
            if (craft.zone != Helio) {
                continue;
            }
            for (const auto& b : *bodies) {
                if (b.id != craft.horizons || b.track.empty()) {
                    continue;
                }
                QString line = QString::number(radius(b.track.back()), 'f', 2) + " AU from the Sun";
                if (earth != nullptr) {
                    const double d = wrap(longitude(b.track.back()) - longitude(earth->track.back())) * 180.0 / pi;
                    line += d >= 0 ? ", " + QString::number(std::lround(d)) + " degrees ahead of the Earth" : ", " + QString::number(std::lround(-d)) + " degrees behind the Earth";
                }
                list->item(static_cast<int>(i))->setText(craft.name + "\n" + craft.role + "\n" + line);
            }
        }
        status->setText("Positions now from JPL Horizons. Pick a spacecraft for its instruments.");
    }};
}
