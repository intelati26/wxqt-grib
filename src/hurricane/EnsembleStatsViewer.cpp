// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/EnsembleStatsViewer.h"
#include "hurricane/ChartKit.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>

namespace {
    using namespace ChartKit;
}

void EnsembleChart::setData(const std::shared_ptr<HurricaneData::StormData>& newStorm, const std::shared_ptr<HurricaneData::EnsembleData>& newEnsembles) {
    storm = newStorm;
    ensembles = newEnsembles;
    series.clear();
    for (const auto& set : ensembles->sets) {
        if (!isEnsemble(set.label)) {
            continue;
        }
        Series s;
        s.label = set.label;
        s.cycle = set.cycle;
        s.color = set.label == "AIFS ENS" ? QColor{0, 150, 100} : set.label == "IFS ENS" ? QColor{230, 110, 20} : QColor{120, 70, 200};
        for (const auto& m : set.storm.members) {
            s.members += m.type >= 2 ? 1 : 0;
        }
        const auto all = UtilityEnsembleStats::compute(set.storm, 6);
        int last = 0;
        for (const auto& h : all) {
            if (h.alive >= 5) {
                last = h.hour;   // beyond this the few members left make the numbers jump about
            }
        }
        for (const auto& h : all) {
            if (h.hour <= std::min(168, std::max(last, 24))) {   // a week is as far as the picture is useful
                s.hours.push_back(h);
            }
        }
        series.push_back(std::move(s));
    }
    update();
}

void EnsembleChart::setShown(const string& family, bool on) {
    hidden[family] = !on;
    update();
}

bool EnsembleChart::shown(const string& family) const {
    const auto found = hidden.find(family);
    return found == hidden.end() || !found->second;
}

void EnsembleChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (series.empty() || !ensembles) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No ensemble members for this storm");
        return;
    }
    int lastHour = 24;
    for (const auto& s : series) {
        if (!shown(s.label)) {
            continue;
        }
        lastHour = std::max(lastHour, s.hours.empty() ? 0 : s.hours.back().hour);
    }
    const double xStep = lastHour > 240 ? 48.0 : lastHour > 120 ? 24.0 : 12.0;
    const double marginLeft = 40.0;
    const double gutter = 12.0;
    const double legendHeight = 48.0;
    const double width = (this->width() - marginLeft - 12.0 - gutter) / 2.0;
    const double height = (this->height() - legendHeight - 2 * 34.0) / 2.0;
    const auto panel = [&] (int column, int row) {
        return QRectF{marginLeft + column * (width + gutter), 22.0 + row * (height + 34.0), width, height};
    };
    // ranges, from every ensemble and from NHC's forecast
    double windMax = 60.0;
    double pressMin = 1000.0;
    double pressMax = 1012.0;
    double radiusMax = 150.0;
    for (const auto& s : series) {
        if (!shown(s.label)) {
            continue;
        }
        for (const auto& h : s.hours) {
            if (has(h.wind90)) windMax = std::max(windMax, h.wind90);
            if (has(h.press10)) pressMin = std::min(pressMin, h.press10);
            if (has(h.pressMax)) pressMax = std::max(pressMax, h.pressMax);
            if (has(h.radius90)) radiusMax = std::max(radiusMax, h.radius90);
        }
    }
    const auto& reference = ensembles->sets.front();
    if (shown("nhc") && storm && !storm->official.fixes.empty()) {
        const double offset = UtilityAtcf::hoursBetween(reference.cycle, storm->official.cycle);
        for (const auto& f : storm->official.fixes) {
            if (offset + f.tau >= 0 && offset + f.tau <= lastHour) {
                windMax = std::max(windMax, static_cast<double>(f.wind));
                if (f.pressure > 0) {
                    pressMin = std::min(pressMin, static_cast<double>(f.pressure));
                    pressMax = std::max(pressMax, static_cast<double>(f.pressure));
                }
            }
        }
    }
    windMax = std::ceil(windMax / 20.0) * 20.0;
    pressMin = std::floor(pressMin / 10.0) * 10.0;
    pressMax = std::ceil(pressMax / 10.0) * 10.0;
    radiusMax = std::ceil(radiusMax / 100.0) * 100.0;
    const Axes wind{panel(0, 0), 0.0, static_cast<double>(lastHour), 0.0, windMax};
    const Axes press{panel(1, 0), 0.0, static_cast<double>(lastHour), pressMin, pressMax};
    const Axes spread{panel(0, 1), 0.0, static_cast<double>(lastHour), 0.0, radiusMax};
    const Axes prob{panel(1, 1), 0.0, static_cast<double>(lastHour), 0.0, 100.0};

    frame(p, wind, "Max 10 m wind", "kt", windMax > 120 ? 40.0 : 20.0, xStep);
    frame(p, press, "Min pressure", "mb", pressMax - pressMin > 60 ? 20.0 : 10.0, xStep);
    frame(p, spread, "Track spread (circle around the mean position)", "km", radiusMax > 800 ? 200.0 : 100.0, xStep);
    frame(p, prob, "Share of members", "%", 25.0, xStep);
    categoryLines(p, wind);
    for (const auto& s : series) {
        if (!shown(s.label)) {
            continue;
        }
        auto soft = s.color;
        soft.setAlpha(60);
        const QPen mid{s.color, 2.2};
        const QPen edge{s.color, 1.0, Qt::DashLine};
        band(p, wind, s.hours, [] (const Stats::Hour& h) { return h.wind25; }, [] (const Stats::Hour& h) { return h.wind75; }, soft);
        line(p, wind, s.hours, [] (const Stats::Hour& h) { return h.wind90; }, edge);
        line(p, wind, s.hours, [] (const Stats::Hour& h) { return h.windMedian; }, mid);
        band(p, press, s.hours, [] (const Stats::Hour& h) { return h.press25; }, [] (const Stats::Hour& h) { return h.press75; }, soft);
        line(p, press, s.hours, [] (const Stats::Hour& h) { return h.press10; }, edge);
        line(p, press, s.hours, [] (const Stats::Hour& h) { return h.pressMedian; }, mid);
        line(p, spread, s.hours, [] (const Stats::Hour& h) { return h.radius90; }, QPen{s.color, 1.6, Qt::DashLine});
        line(p, spread, s.hours, [] (const Stats::Hour& h) { return h.radius50; }, QPen{s.color, 2.2});
        const auto percent = [] (auto member) { return [member] (const Stats::Hour& h) { return (h.*member) * 100.0; }; };
        line(p, prob, s.hours, percent(&Stats::Hour::probAlive), QPen{s.color, 1.2, Qt::DotLine});
        line(p, prob, s.hours, percent(&Stats::Hour::probTs), QPen{s.color, 2.2});
        line(p, prob, s.hours, percent(&Stats::Hour::probHurricane), QPen{s.color, 1.8, Qt::DashLine});
    }
    // the unperturbed IFS and AIFS runs, and NHC's forecast, over the wind and pressure
    for (const auto& other : ensembles->sets) {
        if (!shown("runs") || (isEnsemble(other.label) && other.label != "AIFS ENS" && other.label != "IFS ENS")) {
            continue;
        }
        const bool single = !isEnsemble(other.label);
        for (const auto& member : other.storm.members) {
            if (member.type >= 2 || (!single && member.type != 0)) {
                continue;   // an ensemble's own high-resolution run (type 0), or a single run
            }
            const auto color = other.label.rfind("AIFS", 0) == 0 ? QColor{0, 90, 60} : QColor{150, 60, 0};
            const QPen pen{color, 1.2, Qt::SolidLine};
            trackLine(p, wind, member.steps, [] (const UtilityEcmwfTracks::Step& s) { return s.wind; }, pen);
            trackLine(p, press, member.steps, [] (const UtilityEcmwfTracks::Step& s) { return s.pressure; }, pen);
        }
    }
    if (shown("nhc") && storm && !storm->official.fixes.empty()) {
        const double offset = UtilityAtcf::hoursBetween(reference.cycle, storm->official.cycle);
        p.setPen(QPen{QColor{20, 20, 20}, 1.5});
        p.setBrush(QColor{255, 255, 255});
        for (const auto& f : storm->official.fixes) {
            const double hour = offset + f.tau;
            if (hour < 0 || hour > lastHour) continue;
            if (f.wind >= 0) p.drawEllipse(wind.at(hour, f.wind), 3.2, 3.2);
            if (f.pressure > 0) p.drawEllipse(press.at(hour, f.pressure), 3.2, 3.2);
        }
    }

    // one legend: the ensembles by colour, the line styles, and where the numbers come from
    QFont small{p.font()};
    small.setPixelSize(10);
    p.setFont(small);
    double y = this->height() - 34.0;
    double x = marginLeft;
    for (const auto& s : series) {
        if (!shown(s.label)) {
            continue;
        }
        p.setPen(QPen{s.color, 3.0});
        p.drawLine(QPointF{x, y - 4}, QPointF{x + 16, y - 4});
        p.setPen(QColor{40, 40, 40});
        const QString text = QString::fromStdString(s.label) + " " + QString::fromStdString(UtilityAtcf::formatTime(s.cycle)) + " (" + QString::number(s.members) + ")";
        p.drawText(QPointF{x + 20, y}, text);
        x += 34.0 + QFontMetricsF{small}.horizontalAdvance(text);
    }
    p.setPen(QColor{40, 40, 40});
    p.drawText(QPointF{x, y}, "dark thin: AIFS / IFS runs     white dots: NHC");
    y += 14.0;
    p.setPen(QColor{80, 80, 80});
    p.drawText(QPointF{marginLeft, y},
               "Wind and pressure: median line, shaded 25-75 %, dashed 90th percentile of wind / 10th of pressure.  Spread: solid 50 %, dashed 90 % of members.  Share: dotted alive, solid 34 kt+, dashed 64 kt+.");
    y += 12.0;
    p.drawText(QPointF{marginLeft, y}, "Model winds (ECMWF 10 m, GEFS via NHC's ATCF files), not NHC's 1-minute sustained wind.  Contains ECMWF open data, CC BY 4.0.");
}

EnsembleStatsViewer::EnsembleStatsViewer(Window * parent, const std::shared_ptr<HurricaneData::StormData>& storm,
                                         const std::shared_ptr<HurricaneData::EnsembleData>& ensembles)
    : Window{parent}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Ensemble members - distribution - " + HurricaneData::idLabel(storm->id) + " " + storm->name);
    textStatus.setWordWrap(false);
    chart = new EnsembleChart{this};
    // the families to draw
    struct Family {
        const char * id;
        const char * text;
        QColor color;
    };
    const Family families[] = {{"AIFS ENS", "AIFS ENS", QColor{0, 150, 100}}, {"IFS ENS", "IFS ENS", QColor{230, 110, 20}}, {"GEFS", "GEFS", QColor{120, 70, 200}},
                               {"runs", "AIFS / IFS runs", QColor{90, 90, 90}}, {"nhc", "NHC forecast", QColor{255, 255, 255}}};
    for (const auto& family : families) {
        bool present = string{family.id} == "runs" || string{family.id} == "nhc";
        for (const auto& set : ensembles->sets) {
            present = present || set.label == family.id;
        }
        if (!present) {
            continue;   // no such ensemble for this storm
        }
        auto * check = new QCheckBox{family.text, this};
        QPixmap swatch{14, 14};
        swatch.fill(family.color);
        check->setIcon(QIcon{swatch});
        check->setChecked(true);
        const string id = family.id;
        QObject::connect(check, &QCheckBox::toggled, [this, id] (bool on) { chart->setShown(id, on); });
        rowFamilies.addWidgetReal(check);
    }
    rowFamilies.addStretch();
    box.addLayout(rowFamilies);
    box.addWidget(textStatus);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 640);
    chart->setData(storm, ensembles);
    string text = "Storm " + (ensembles->sets.empty() ? string{} : ensembles->sets.front().storm.id) + ":";
    for (const auto& set : ensembles->sets) {
        int members = 0;
        for (const auto& m : set.storm.members) {
            members += m.type >= 2 ? 1 : 0;
        }
        if (members > 0) {
            text += "  " + set.label + " " + std::to_string(members) + " members";
        }
    }
    textStatus.setText(text);
}
