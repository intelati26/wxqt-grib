// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HafsIntensity.h"
#include <cctype>
#include <cstdlib>
#include <memory>
#include <tuple>
#include <utility>
#include <QPainter>
#include <algorithm>
#include "gfs/GfsChart.h"
#include "hurricane/ChartKit.h"
#include "ui/ChartExport.h"
#include "objects/FutureVoid.h"
#include "ui/ActivityLabel.h"
#include "util/Utility.h"

namespace {
    // the basin letter of a model id: 09l the Atlantic, 18e the East Pacific
    string basinName(char letter) {
        switch (letter) {
        case 'l': return "Atlantic";
        case 'e': return "East Pacific";
        case 'c': return "Central Pacific";
        case 'w': return "West Pacific";
        case 's': return "South Hemisphere";
        case 'a': return "Arabian Sea";
        case 'b': return "Bay of Bengal";
        default: return "";
        }
    }
}

string Hafs::modelId(const string& nhcId) {
    if (nhcId.size() < 4) {
        return nhcId;
    }
    const auto basin = nhcId.substr(0, 2);
    return nhcId.substr(2, 2) + (basin == "al" ? "l" : basin == "ep" ? "e" : basin == "cp" ? "c" : basin == "wp" ? "w" : "x");
}

bool Hafs::isStormId(const string& id) {
    return id.size() == 3 && std::isdigit(static_cast<unsigned char>(id[0])) && std::isdigit(static_cast<unsigned char>(id[1])) && std::islower(static_cast<unsigned char>(id[2]));
}

string Hafs::label(const string& id) {
    if (!isStormId(id)) {
        return id;
    }
    return id.substr(0, 2) + static_cast<char>(std::toupper(static_cast<unsigned char>(id[2]))) + "  " + basinName(id[2]);
}

HafsIntensityChart::HafsIntensityChart(QWidget * parent) : QWidget{parent} {
    setMinimumSize(700, 560);
    ChartExport::install(this, "HAFS intensity");
}

void HafsIntensityChart::setData(const std::vector<GfsChart::TrackPoint>& a, const std::vector<GfsChart::TrackPoint>& b, const string& newTitle) {
    trackA = a;
    trackB = b;
    title = newTitle;
    update();
}

void HafsIntensityChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{250, 250, 250});
    QFont base = p.font();
    base.setPixelSize(12);
    p.setFont(base);
    const double left = 60, right = width() - 24, top = 54, gap = 46;
    const double plotHeight = (height() - top - 36 - gap) / 2.0;
    int hours = 24, peak = 60, lowest = 1010, highest = 1010;
    for (const auto * track : {&trackA, &trackB}) {
        for (const auto& t : *track) {
            hours = std::max(hours, t.hour);
            peak = std::max(peak, t.wind);
            if (t.pressure > 800) {
                lowest = std::min(lowest, t.pressure);
                highest = std::max(highest, t.pressure);
            }
        }
    }
    hours = (hours + 11) / 12 * 12;
    const double windTop = std::ceil((peak + 10) / 20.0) * 20.0;
    const double pressureLow = std::floor((lowest - 5) / 10.0) * 10.0, pressureHigh = std::ceil((highest + 2) / 10.0) * 10.0;
    p.setPen(QColor{30, 30, 30});
    QFont big = base;
    big.setPixelSize(15);
    big.setBold(true);
    p.setFont(big);
    p.drawText(QPointF{left, 24}, QString::fromStdString(title));
    p.setFont(base);
    const struct { const std::vector<GfsChart::TrackPoint> * track; QColor color; const char * name; Qt::PenStyle style; } series[] = {
        {&trackA, QColor{20, 20, 20}, "HAFS-A", Qt::SolidLine}, {&trackB, QColor{20, 90, 220}, "HAFS-B", Qt::DashLine}};
    double x = left;
    for (const auto& s : series) {
        if (s.track->empty()) {
            continue;
        }
        p.setPen(QPen{s.color, 2.5, s.style});
        p.drawLine(QPointF{x, 40}, QPointF{x + 26, 40});
        p.setPen(QColor{30, 30, 30});
        p.drawText(QPointF{x + 32, 44}, s.name);
        x += 110;
    }
    for (int panel = 0; panel < 2; panel++) {
        const bool wind = panel == 0;
        const ChartKit::Axes axes{QRectF{left, top + 18 + panel * (plotHeight + gap), right - left, plotHeight - 18}, 0.0, static_cast<double>(hours), wind ? 0.0 : pressureLow, wind ? windTop : pressureHigh};
        ChartKit::frame(p, axes, wind ? "Maximum wind" : "Minimum pressure", wind ? "kt" : "mb", wind ? 20.0 : 10.0, 12.0);
        if (wind) {
            ChartKit::categoryLines(p, axes);
        }
        for (const auto& s : series) {
            QPolygonF line;
            for (const auto& t : *s.track) {
                if (wind || t.pressure > 800) {
                    line << axes.at(t.hour, wind ? t.wind : t.pressure);
                }
            }
            p.setPen(QPen{s.color, 2.5, s.style, Qt::RoundCap, Qt::RoundJoin});
            p.setBrush(Qt::NoBrush);
            p.drawPolyline(line);
        }
    }
}

HafsIntensityViewer::HafsIntensityViewer(Window * parent, const string& storm, const std::vector<GfsChart::TrackPoint>& a, const std::vector<GfsChart::TrackPoint>& b, const string& cycle)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("HAFS intensity - " + storm);
    auto * chart = new HafsIntensityChart;
    chart->setData(a, b, "HAFS forecast intensity, storm " + storm + (cycle.empty() ? string{} : ", run " + cycle));
    box.addWidgetReal(chart);
    box.getAndShow(this);
}

void HafsIntensityViewer::open(Window * parent, const string& storm, std::function<void(const string&)> say) {
    auto result = std::make_shared<std::tuple<std::vector<GfsChart::TrackPoint>, std::vector<GfsChart::TrackPoint>, string>>();
    say("Reading the HAFS-A and HAFS-B tracks...");
    new FutureVoid{parent, [storm, result] {
                       std::get<0>(*result) = GfsRender::hafsTrack("HAFSA", storm, std::get<2>(*result));
                       string other;
                       std::get<1>(*result) = GfsRender::hafsTrack("HAFSB", storm, other);
                   },
                   [parent, storm, result, say] {
                       if (std::get<0>(*result).empty() && std::get<1>(*result).empty()) {
                           say("The model has no track for this storm.");
                           return;
                       }
                       say("");
                       new HafsIntensityViewer{parent, storm, std::get<0>(*result), std::get<1>(*result), std::get<2>(*result)};
                   }};
}
