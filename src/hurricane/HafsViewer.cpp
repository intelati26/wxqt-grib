// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HafsViewer.h"
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

    std::vector<string> hours() {
        std::vector<string> out;
        for (int h = 0; h <= 126; h += 3) {
            out.push_back((h < 100 ? (h < 10 ? "00" : "0") : "") + std::to_string(h));
        }
        return out;
    }

    std::vector<string> productLabels(std::vector<string>& ids) {
        std::vector<string> labels;
        ids.clear();
        for (const auto& p : GfsChart::products()) {
            if (p.source == "HAFSA") {
                ids.push_back(p.id);
                labels.push_back(p.label);
            }
        }
        return labels;
    }
}

string HafsViewer::modelId(const string& nhcId) {
    if (nhcId.size() < 4) {
        return nhcId;
    }
    const auto basin = nhcId.substr(0, 2);
    return nhcId.substr(2, 2) + (basin == "al" ? "l" : basin == "ep" ? "e" : basin == "cp" ? "c" : basin == "wp" ? "w" : "x");
}

HafsViewer::HafsViewer(Window * parent, const string& storm, const string& name)
    : Window{parent}
    , image{this}
    , comboModel{this, {"HAFS-A", "HAFS-B"}}
    , comboStorm{this, {"Looking for storms..."}}
    , comboProduct{this, productLabels(productIds)}
    , comboTime{this, hours()}
    , backForward{this, [this] { step(-1); }, [this] { step(1); }}
    , buttonIntensity{this, None, "Intensity..."}
    , textStatus{this, ""}
    , first{modelId(storm)}
    , firstName{name}
    , session{std::make_shared<GfsRender::Session>()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("HAFS hurricane model" + (name.empty() ? string{} : " - " + name));
    comboTime.setIndex(8);   // 24 hours
    comboModel.connect([this] { loadStorms(); });
    comboStorm.connect([this] { draw(); });
    comboProduct.connect([this] { draw(); });
    comboTime.connect([this] { draw(); });
    row.addWidget(comboModel);
    row.addWidget(comboStorm);
    row.addWidget(comboProduct);
    row.addWidget(comboTime);
    row.addLayout(backForward);
    radiiCheck = new QCheckBox{"Wind field (34 / 50 / 64 kt)", this};
    radiiCheck->setChecked(Utility::readPref("HAFS_RADII", "true").compare(0, 1, "t") == 0);   // remembered
    QObject::connect(radiiCheck, &QCheckBox::toggled, [this] (bool on) {
        Utility::writePref("HAFS_RADII", on ? "true" : "false");
        draw();
    });
    row.addWidgetReal(radiiCheck);
    row.addWidget(buttonIntensity);
    buttonIntensity.connect([this] { showIntensity(); });
    box.addLayout(row);
    box.addWidget(textStatus);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);
    loadStorms();
}

void HafsViewer::loadStorms() {
    const int mine = ++loading;
    textStatus.setText(string{"Looking for the storms the model is running..."});
    const auto name = model();
    auto result = std::make_shared<std::pair<std::vector<string>, string>>();
    new FutureVoid{this, [name, result] { result->first = GfsRender::hafsStorms(name, result->second); },
                   [this, result, mine] {
                       if (mine == loading) {
                           fillStorms(result->first, result->second);
                       }
                   }};
}

void HafsViewer::fillStorms(const std::vector<string>& found, const string& cycle) {
    const string keep = !storms.empty() && comboStorm.getIndex() >= 0 && comboStorm.getIndex() < static_cast<int>(storms.size()) ? storms[static_cast<size_t>(comboStorm.getIndex())] : first;
    storms = found;
    std::vector<string> labels;
    for (const auto& s : storms) {
        labels.push_back(s + "  " + basinName(s.empty() ? ' ' : s.back()) + (s == first && !firstName.empty() ? "  " + firstName : ""));
    }
    comboStorm.block();
    if (storms.empty()) {
        comboStorm.setList({"No active storms in the model"});
        comboStorm.unblock();
        textStatus.setText(string{model() == "HAFSB" ? "HAFS-B has no storm in its newest cycle." : "HAFS-A has no storm in its newest cycle."});
        return;
    }
    comboStorm.setList(labels);
    size_t index = 0;
    for (size_t i = 0; i < storms.size(); i++) {
        if (storms[i] == keep) {
            index = i;
        }
    }
    comboStorm.setIndex(index);
    comboStorm.unblock();
    textStatus.setText("Newest cycle " + cycle + ", " + std::to_string(storms.size()) + (storms.size() == 1 ? " storm." : " storms."));
    draw();
}

void HafsViewer::step(int by) {
    const int next = comboTime.getIndex() + by;
    if (next < 0 || next >= 43) {
        return;
    }
    comboTime.block();
    comboTime.setIndex(static_cast<size_t>(next));
    comboTime.unblock();
    draw();
}

void HafsViewer::draw() {
    const int s = comboStorm.getIndex(), p = comboProduct.getIndex();
    if (storms.empty() || s < 0 || s >= static_cast<int>(storms.size()) || p < 0 || p >= static_cast<int>(productIds.size())) {
        return;
    }
    const int mine = ++drawing;
    const auto name = model(), storm = storms[static_cast<size_t>(s)], param = productIds[static_cast<size_t>(p)];
    const int hour = comboTime.getIndex() * 3;
    setTitle(name + " " + storm + " +" + std::to_string(hour) + " h");
    auto shared = session;
    auto result = std::make_shared<std::pair<QByteArray, string>>();
    auto probe = std::make_shared<GfsChart::Probe>();
    new FutureVoid{this, [=] { result->first = GfsRender::png(*shared, name, param, storm, "", hour, {}, result->second, probe.get()); },
                   [this, result, mine, probe, name, storm, param] {
                       if (mine != drawing) {
                           return;
                       }
                       if (!result->first.isEmpty()) {
                           const string chart = name + "|" + storm + "|" + param;
                           if (chart == shownChart && image.hasImage()) {   // another hour of the chart: the zoom and the place stay
                               image.setBytesKeepView(result->first);
                           } else {
                               image.setBytes(result->first);
                           }
                           shownChart = chart;
                           if (!hover) {
                               hover = std::make_unique<ChartHover>(&image);
                           }
                           hover->set(probe);
                       } else {
                           textStatus.setText("HAFS: " + (result->second.empty() ? string{"nothing could be drawn"} : result->second));
                       }
                   }};
}

void HafsViewer::showIntensity() {
    const int s = comboStorm.getIndex();
    if (storms.empty() || s < 0 || s >= static_cast<int>(storms.size())) {
        return;
    }
    const auto storm = storms[static_cast<size_t>(s)];
    auto result = std::make_shared<std::tuple<std::vector<GfsChart::TrackPoint>, std::vector<GfsChart::TrackPoint>, string>>();
    textStatus.setText(string{"Reading the HAFS-A and HAFS-B tracks..."});
    new FutureVoid{this, [storm, result] {
                       std::get<0>(*result) = GfsRender::hafsTrack("HAFSA", storm, std::get<2>(*result));
                       string other;
                       std::get<1>(*result) = GfsRender::hafsTrack("HAFSB", storm, other);
                   },
                   [this, storm, result] {
                       if (std::get<0>(*result).empty() && std::get<1>(*result).empty()) {
                           textStatus.setText(string{"The model has no track for this storm."});
                           return;
                       }
                       textStatus.setText(string{});
                       new HafsIntensityViewer{this, storm, std::get<0>(*result), std::get<1>(*result), std::get<2>(*result)};
                   }};
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
    setTitle("HAFS intensity " + storm);
    auto * chart = new HafsIntensityChart;
    chart->setData(a, b, "HAFS forecast intensity, storm " + storm + (cycle.empty() ? string{} : ", run " + cycle));
    box.addWidgetReal(chart);
    box.getAndShow(this);
}
