// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/IntensityViewer.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include "hurricane/ChartKit.h"

namespace {
    using namespace ChartKit;

    QColor ensembleColor(const string& label) {
        return label == "AIFS ENS" ? QColor{0, 150, 100} : label == "IFS ENS" ? QColor{230, 110, 20} : QColor{120, 70, 200};
    }

    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
    }

    // the hours of a yyyymmddhh time since the base, and seconds since 1970 (the vortex messages and the HDOBs carry those) as hours since the base
    double hoursSince(const string& base, const string& t) {
        return UtilityAtcf::hoursBetween(base, t);
    }

    double hoursOfSeconds(const string& base, long seconds) {
        const double baseSeconds = static_cast<double>(daysFromCivil(std::stoi(base.substr(0, 4)), std::stoi(base.substr(4, 2)), std::stoi(base.substr(6, 2))) * 86400L +
                                                       std::stoi(base.substr(8, 2)) * 3600L);
        return (static_cast<double>(seconds) - baseSeconds) / 3600.0;
    }
}

void IntensityChart::setData(const std::shared_ptr<HurricaneData::StormData>& s, const std::shared_ptr<HurricaneData::EnsembleData>& e,
                             const std::shared_ptr<HurricaneData::ShipsData>& sh, const std::shared_ptr<HurricaneData::VdmData>& v,
                             const std::shared_ptr<HurricaneData::ReconData>& r) {
    storm = s;
    ensembles = e;
    ships = sh;
    vdm = v;
    recon = r;
    update();
}

void IntensityChart::setShown(const string& family, bool on) {
    hidden[family] = !on;
    update();
}

bool IntensityChart::shown(const string& family) const {
    const auto found = hidden.find(family);
    return found == hidden.end() || !found->second;
}

void IntensityChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!storm || storm->best.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No track data for this storm");
        return;
    }
    // the time axis: hours from the first best-track fix (but not more than three days before the newest one), to the end of the forecasts
    const string newest = storm->best.back().time;
    string base = storm->best.front().time;
    if (UtilityAtcf::hoursBetween(base, newest) > 96.0) {
        base = UtilityAtcf::addHours(newest, -96);
    }
    const double now = hoursSince(base, newest);
    double end = now + 24.0;
    const auto reach = [&] (double hours) { end = std::max(end, hours); };
    if (shown("nhc") && !storm->official.fixes.empty()) {
        reach(hoursSince(base, storm->official.cycle) + storm->official.fixes.back().tau);
    }
    if (shown("ships") && ships && ships->ships.ok) {
        const double at = hoursSince(base, ships->ships.cycle);
        const auto * land = ships->ships.row("V (KT) LAND");
        for (size_t i = 0; land != nullptr && i < land->size() && i < ships->ships.hours.size(); i++) {
            if (UtilityShips::has((*land)[i])) {
                reach(at + ships->ships.hours[i]);
            }
        }
    }
    struct Set {
        const HurricaneData::EnsembleSet * set;
        vector<UtilityEnsembleStats::Hour> hours;
        double offset;
    };
    vector<Set> sets;
    if (ensembles) {
        for (const auto& set : ensembles->sets) {
            if ((set.label == "AIFS ENS" || set.label == "IFS ENS" || set.label == "GEFS") && shown(set.label)) {
                Set s{&set, UtilityEnsembleStats::compute(set.storm, 6), hoursSince(base, set.cycle)};
                int last = 0;
                for (const auto& h : s.hours) {
                    if (h.alive >= 5) {
                        last = h.hour;
                    }
                }
                s.hours.erase(std::remove_if(s.hours.begin(), s.hours.end(), [last] (const auto& h) { return h.hour > last || h.alive < 5; }), s.hours.end());
                reach(s.offset + last);
                sets.push_back(std::move(s));
            }
        }
    }
    end = std::min(end, now + 180.0);
    const double xStep = end > 200 ? 48.0 : end > 100 ? 24.0 : 12.0;
    const auto label = [&base] (double h) {
        const auto t = UtilityAtcf::addHours(base, static_cast<int>(std::lround(h)));
        return QString::fromStdString(UtilityAtcf::formatTime(t));
    };
    // ranges
    double windMax = 60.0;
    double pressLow = 1000.0;
    double pressHigh = 1012.0;
    for (const auto& f : storm->best) {
        windMax = std::max(windMax, static_cast<double>(f.wind));
        if (f.pressure > 0) {
            pressLow = std::min(pressLow, static_cast<double>(f.pressure));
            pressHigh = std::max(pressHigh, static_cast<double>(f.pressure));
        }
    }
    if (!storm->official.fixes.empty()) {
        for (const auto& f : storm->official.fixes) {
            windMax = std::max(windMax, static_cast<double>(f.wind));
        }
    }
    for (const auto& s : sets) {
        for (const auto& h : s.hours) {
            if (has(h.wind90)) windMax = std::max(windMax, h.wind90);
            if (has(h.press10)) pressLow = std::min(pressLow, h.press10);
            if (has(h.pressMax)) pressHigh = std::max(pressHigh, h.pressMax);
        }
    }
    if (shown("ships") && ships && ships->ships.ok) {
        if (const auto * land = ships->ships.row("V (KT) LAND")) {
            for (const auto v : *land) {
                if (UtilityShips::has(v)) windMax = std::max(windMax, v);
            }
        }
    }
    if (shown("recon") && vdm) {
        for (const auto& m : vdm->messages) {
            if (UtilityVdm::has(m.maxFlightWind())) windMax = std::max(windMax, m.maxFlightWind());
            if (UtilityVdm::has(m.pressure)) pressLow = std::min(pressLow, m.pressure);
        }
    }
    windMax = std::ceil((windMax + 5.0) / 20.0) * 20.0;
    pressLow = std::floor((pressLow - 3.0) / 10.0) * 10.0;
    pressHigh = std::ceil(pressHigh / 10.0) * 10.0;
    const double marginLeft = 46.0;
    const double width = this->width() - marginLeft - 14.0;
    const double height = (this->height() - 76.0 - 30.0) / 2.0;
    const Axes wind{QRectF{marginLeft, 22.0, width, height}, 0.0, end, 0.0, windMax};
    const Axes press{QRectF{marginLeft, 22.0 + height + 30.0, width, height}, 0.0, end, pressLow, pressHigh};
    frame(p, wind, "Maximum wind", "kt", windMax > 120 ? 40.0 : 20.0, xStep, label);
    frame(p, press, "Minimum pressure", "mb", pressHigh - pressLow > 80 ? 20.0 : 10.0, xStep, label);
    p.setPen(QPen{QColor{120, 120, 120}, 1.0, Qt::DotLine});
    for (const double threshold : {34.0, 64.0, 96.0}) {
        if (threshold < windMax) {
            p.drawLine(wind.at(0, threshold), wind.at(end, threshold));
        }
    }
    // now
    for (const auto * axes : {&wind, &press}) {
        p.setPen(QPen{QColor{200, 60, 60}, 1.0, Qt::DashLine});
        p.drawLine(axes->at(now, axes->yMin), axes->at(now, axes->yMax));
    }
    // ensembles: band and median
    for (const auto& s : sets) {
        auto soft = ensembleColor(s.set->label);
        soft.setAlpha(55);
        const auto shift = [&s] (const UtilityEnsembleStats::Hour& h) { return s.offset + h.hour; };
        const auto bandOf = [&] (const Axes& a, auto low, auto high, auto mid) {
            QPainterPath poly;
            vector<QPointF> up;
            vector<QPointF> down;
            QPainterPath line;
            bool started = false;
            for (const auto& h : s.hours) {
                if (has(low(h)) && has(high(h))) {
                    down.push_back(a.at(shift(h), low(h)));
                    up.push_back(a.at(shift(h), high(h)));
                }
                if (has(mid(h))) {
                    const auto pt = a.at(shift(h), mid(h));
                    started ? line.lineTo(pt) : line.moveTo(pt);
                    started = true;
                }
            }
            if (up.size() >= 2) {
                poly.moveTo(down.front());
                for (const auto& pt : down) poly.lineTo(pt);
                for (auto it = up.rbegin(); it != up.rend(); ++it) poly.lineTo(*it);
                poly.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(soft);
                p.drawPath(poly);
            }
            p.setPen(QPen{ensembleColor(s.set->label), 2.0});
            p.setBrush(Qt::NoBrush);
            p.drawPath(line);
        };
        bandOf(wind, [] (const auto& h) { return h.wind25; }, [] (const auto& h) { return h.wind75; }, [] (const auto& h) { return h.windMedian; });
        bandOf(press, [] (const auto& h) { return h.press25; }, [] (const auto& h) { return h.press75; }, [] (const auto& h) { return h.pressMedian; });
    }
    // the single runs
    if (ensembles && shown("runs")) {
        for (const auto& set : ensembles->sets) {
            const bool single = set.label == "AIFS" || set.label == "IFS HRES";
            const bool control = set.label == "AIFS ENS" || set.label == "IFS ENS";
            if (!single && !control) {
                continue;
            }
            const auto color = set.label.rfind("AIFS", 0) == 0 ? QColor{0, 90, 60} : QColor{150, 60, 0};
            const double offset = hoursSince(base, set.cycle);
            for (const auto& member : set.storm.members) {
                if (member.type >= 2 || (control && member.type != 0)) {
                    continue;
                }
                const auto draw = [&] (const Axes& a, auto value) {
                    p.setPen(QPen{color, 1.2});
                    p.setBrush(Qt::NoBrush);
                    QPainterPath path;
                    bool started = false;
                    for (const auto& s : member.steps) {
                        const double v = value(s);
                        if (!has(v) || offset + s.hour > end) {
                            started = false;
                            continue;
                        }
                        const auto pt = a.at(offset + s.hour, v);
                        started ? path.lineTo(pt) : path.moveTo(pt);
                        started = true;
                    }
                    p.drawPath(path);
                };
                draw(wind, [] (const UtilityEcmwfTracks::Step& s) { return s.wind; });
                draw(press, [] (const UtilityEcmwfTracks::Step& s) { return s.pressure; });
            }
        }
    }
    // SHIPS and LGEM
    if (shown("ships") && ships && ships->ships.ok) {
        const double at = hoursSince(base, ships->ships.cycle);
        const auto row = [&] (const char * name, const QPen& pen) {
            const auto * values = ships->ships.row(name);
            if (values == nullptr) {
                return;
            }
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool started = false;
            for (size_t i = 0; i < values->size() && i < ships->ships.hours.size(); i++) {
                if (!UtilityShips::has((*values)[i]) || at + ships->ships.hours[i] > end) {
                    started = false;
                    continue;
                }
                const auto pt = wind.at(at + ships->ships.hours[i], (*values)[i]);
                started ? path.lineTo(pt) : path.moveTo(pt);
                started = true;
            }
            p.drawPath(path);
        };
        row("V (KT) LGEM", QPen{QColor{40, 130, 200}, 1.6, Qt::DashLine});
        row("V (KT) LAND", QPen{QColor{20, 20, 20}, 1.8, Qt::DashDotLine});
    }
    // NHC's official forecast
    if (shown("nhc") && !storm->official.fixes.empty()) {
        const double at = hoursSince(base, storm->official.cycle);
        p.setPen(QPen{QColor{20, 20, 20}, 1.4});
        p.setBrush(QColor{255, 255, 255});
        QPainterPath wl;
        QPainterPath pl;
        bool ws = false;
        bool ps = false;
        for (const auto& f : storm->official.fixes) {
            if (f.wind >= 0) {
                const auto pt = wind.at(at + f.tau, f.wind);
                ws ? wl.lineTo(pt) : wl.moveTo(pt);
                ws = true;
            }
            if (f.pressure > 0) {
                const auto pt = press.at(at + f.tau, f.pressure);
                ps ? pl.lineTo(pt) : pl.moveTo(pt);
                ps = true;
            }
        }
        p.setBrush(Qt::NoBrush);
        p.drawPath(wl);
        p.drawPath(pl);
        p.setBrush(QColor{255, 255, 255});
        for (const auto& f : storm->official.fixes) {
            if (f.tau % 12 != 0) {
                continue;
            }
            if (f.wind >= 0) p.drawEllipse(wind.at(at + f.tau, f.wind), 3.5, 3.5);
            if (f.pressure > 0) p.drawEllipse(press.at(at + f.tau, f.pressure), 3.5, 3.5);
        }
    }
    // the best track so far
    if (shown("best")) {
        QPainterPath wl;
        QPainterPath pl;
        bool ws = false;
        bool ps = false;
        for (const auto& f : storm->best) {
            const double h = hoursSince(base, f.time);
            if (h < 0) continue;
            if (f.wind >= 0) {
                const auto pt = wind.at(h, f.wind);
                ws ? wl.lineTo(pt) : wl.moveTo(pt);
                ws = true;
            }
            if (f.pressure > 0) {
                const auto pt = press.at(h, f.pressure);
                ps ? pl.lineTo(pt) : pl.moveTo(pt);
                ps = true;
            }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen{QColor{10, 10, 10}, 3.0});
        p.drawPath(wl);
        p.drawPath(pl);
    }
    // recon: the strongest flight-level wind and the minimum pressure of each vortex message
    if (shown("recon") && vdm) {
        for (const auto& m : vdm->messages) {
            const double h = hoursOfSeconds(base, m.seconds);
            if (h < 0 || h > end) continue;
            if (UtilityVdm::has(m.maxFlightWind())) {
                p.setPen(QPen{QColor{20, 60, 190}, 1.2});
                p.setBrush(QColor{80, 120, 240});
                const auto pt = wind.at(h, m.maxFlightWind());
                p.drawRect(QRectF{pt.x() - 3.5, pt.y() - 3.5, 7, 7});
            }
            if (UtilityVdm::has(m.pressure)) {
                p.setPen(QPen{QColor{180, 30, 30}, 1.4});
                p.setBrush(m.extrapolated ? QColor{255, 255, 255} : QColor{220, 60, 60});
                p.drawEllipse(press.at(h, m.pressure), 3.8, 3.8);
            }
        }
    }
    // key
    QFont small{p.font()};
    small.setPixelSize(10);
    p.setFont(small);
    p.setPen(QColor{70, 70, 70});
    p.drawText(QPointF{marginLeft, this->height() - 20.0}, "Black: best track.  Dashed red line: now.  White dots: NHC forecast.  Dash-dot: SHIPS, dashed blue: LGEM.  Bands: 25-75 %, line: median of each ensemble.");
    p.drawText(QPointF{marginLeft, this->height() - 7.0}, "Squares: strongest recon flight-level wind; red dots: recon minimum pressure (hollow: extrapolated).  ECMWF / GEFS winds are model winds, not 1-minute sustained.");
}

IntensityViewer::IntensityViewer(Window * parent, const std::shared_ptr<HurricaneData::StormData>& storm, const std::shared_ptr<HurricaneData::EnsembleData>& ensembles,
                                 const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::VdmData>& vdm,
                                 const std::shared_ptr<HurricaneData::ReconData>& recon)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Intensity history and forecast - " + HurricaneData::idLabel(storm->id) + " " + storm->name);
    chart = new IntensityChart{this};
    struct Family {
        const char * id;
        const char * text;
        QColor color;
    };
    const Family families[] = {{"best", "Best track", QColor{10, 10, 10}}, {"nhc", "NHC forecast", QColor{255, 255, 255}}, {"ships", "SHIPS / LGEM", QColor{40, 130, 200}},
                               {"AIFS ENS", "AIFS ENS", QColor{0, 150, 100}}, {"IFS ENS", "IFS ENS", QColor{230, 110, 20}}, {"GEFS", "GEFS", QColor{120, 70, 200}},
                               {"runs", "AIFS / IFS runs", QColor{90, 90, 90}}, {"recon", "Recon", QColor{220, 60, 60}}};
    for (const auto& family : families) {
        const string id = family.id;
        bool present = true;
        if (id == "AIFS ENS" || id == "IFS ENS" || id == "GEFS") {
            present = false;
            if (ensembles) {
                for (const auto& set : ensembles->sets) {
                    present = present || set.label == id;
                }
            }
        } else if (id == "ships") {
            present = ships && ships->ships.ok;
        } else if (id == "recon") {
            present = vdm && !vdm->messages.empty();
        }
        if (!present) {
            continue;
        }
        auto * check = new QCheckBox{family.text, this};
        QPixmap swatch{14, 14};
        swatch.fill(family.color);
        check->setIcon(QIcon{swatch});
        check->setChecked(true);
        QObject::connect(check, &QCheckBox::toggled, [this, id] (bool on) { chart->setShown(id, on); });
        rowFamilies.addWidgetReal(check);
    }
    rowFamilies.addStretch();
    box.addLayout(rowFamilies);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 700);
    chart->setData(storm, ensembles, ships, vdm, recon);
}
