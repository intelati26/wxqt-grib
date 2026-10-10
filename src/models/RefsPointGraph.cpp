// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "models/RefsPointGraph.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QPainter>
#include <QPen>
#include "models/UtilityGrib.h"
#include "objects/FutureVoid.h"
#include "util/To.h"

class PlumeCanvas : public QWidget {
public:
    explicit PlumeCanvas(QWidget * parent) : QWidget{parent} {
        setMinimumSize(520, 320);
    }

    void reset(const QString& newYLabel, bool newHasThreshold, double newThreshold, int newMaxHour) {
        yLabel = newYLabel;
        hasThreshold = newHasThreshold;
        threshold = newThreshold;
        maxHour = newMaxHour;
        hours.clear();
        series.assign(UtilityRefs::memberCount, {});
        update();
    }

    void addHour(int hour, const vector<double>& values) {
        hours.push_back(hour);
        for (int member = 0; member < UtilityRefs::memberCount; member += 1) {
            series[member].push_back(member < static_cast<int>(values.size()) ? values[member] : std::nan(""));
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        ChartPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor{250, 250, 250});
        const QRect plot{58, 16, width() - 58 - 130, height() - 16 - 42};
        painter.setPen(QColor{60, 60, 60});
        painter.drawRect(plot);

        // y range from the data (and the threshold line, if any)
        double lo = std::numeric_limits<double>::infinity();
        double hi = -std::numeric_limits<double>::infinity();
        for (const auto& line : series) {
            for (const auto value : line) {
                if (!std::isnan(value)) {
                    lo = std::min(lo, value);
                    hi = std::max(hi, value);
                }
            }
        }
        if (hasThreshold) {
            lo = std::min(lo, threshold);
            hi = std::max(hi, threshold);
        }
        if (!std::isfinite(lo)) {
            painter.drawText(plot, Qt::AlignCenter, "loading...");
            return;
        }
        if (hi - lo < 1e-6) {
            hi = lo + 1.0;
        }
        // round the axis to "nice" tick values (1/2/5 x 10^n)
        const auto rawStep = (hi - lo) / 5.0;
        const auto magnitude = std::pow(10.0, std::floor(std::log10(rawStep)));
        const auto fraction = rawStep / magnitude;
        const auto step = magnitude * (fraction <= 1.0 ? 1.0 : (fraction <= 2.0 ? 2.0 : (fraction <= 5.0 ? 5.0 : 10.0)));
        lo = std::floor(lo / step) * step;
        hi = std::ceil(hi / step) * step;
        const int tickCount = std::max(1, static_cast<int>(std::lround((hi - lo) / step)));
        const auto xOf = [&] (double hour) {
            return plot.left() + plot.width() * hour / std::max(1, maxHour);
        };
        const auto yOf = [&] (double value) {
            return plot.bottom() - plot.height() * (value - lo) / (hi - lo);
        };

        // grid + axis labels
        QFont font = painter.font();
        font.setPixelSize(11);
        painter.setFont(font);
        const int xStep = maxHour <= 12 ? 1 : (maxHour <= 24 ? 3 : 6);
        for (int hour = 0; hour <= maxHour; hour += xStep) {
            painter.setPen(QColor{225, 225, 225});
            painter.drawLine(QPointF{xOf(hour), static_cast<double>(plot.top())},
                             QPointF{xOf(hour), static_cast<double>(plot.bottom())});
            painter.setPen(QColor{60, 60, 60});
            painter.drawText(QPointF{xOf(hour) - 8, static_cast<double>(plot.bottom() + 15)}, QString::number(hour));
        }
        for (int tick = 0; tick <= tickCount; tick += 1) {
            const auto value = lo + step * tick;
            painter.setPen(QColor{225, 225, 225});
            painter.drawLine(QPointF{static_cast<double>(plot.left()), yOf(value)},
                             QPointF{static_cast<double>(plot.right()), yOf(value)});
            painter.setPen(QColor{60, 60, 60});
            painter.drawText(QRectF{2, yOf(value) - 8, 54, 16}, Qt::AlignRight | Qt::AlignVCenter,
                             QString::number(value, 'g', 6));
        }
        painter.drawText(QRectF{static_cast<double>(plot.left()), static_cast<double>(plot.bottom() + 20),
                                static_cast<double>(plot.width()), 18}, Qt::AlignCenter, "forecast hour");
        painter.save();
        painter.translate(12, plot.center().y());
        painter.rotate(-90);
        painter.drawText(QRectF{-plot.height() / 2.0, -12, static_cast<double>(plot.height()), 14},
                         Qt::AlignCenter, yLabel);
        painter.restore();

        painter.setClipRect(plot.adjusted(-2, -2, 2, 2));
        if (hasThreshold) {
            QPen dashed{QColor{90, 90, 90}, 1.5, Qt::DashLine};
            painter.setPen(dashed);
            painter.drawLine(QPointF{static_cast<double>(plot.left()), yOf(threshold)},
                             QPointF{static_cast<double>(plot.right()), yOf(threshold)});
        }
        // member lines
        for (int member = 0; member < UtilityRefs::memberCount; member += 1) {
            painter.setPen(QPen{UtilityRefs::memberColor(member + 1), 1.8});
            QPointF previous;
            bool havePrevious = false;
            for (size_t i = 0; i < hours.size(); i += 1) {
                const auto value = series[member][i];
                if (std::isnan(value)) {
                    havePrevious = false;
                    continue;
                }
                const QPointF point{xOf(hours[i]), yOf(value)};
                if (havePrevious) {
                    painter.drawLine(previous, point);
                }
                previous = point;
                havePrevious = true;
            }
        }
        // ensemble mean of the members present at each hour
        painter.setPen(QPen{QColor{20, 20, 20}, 3.0});
        QPointF previous;
        bool havePrevious = false;
        for (size_t i = 0; i < hours.size(); i += 1) {
            double sum = 0.0;
            int count = 0;
            for (int member = 0; member < UtilityRefs::memberCount; member += 1) {
                if (!std::isnan(series[member][i])) {
                    sum += series[member][i];
                    count += 1;
                }
            }
            if (count == 0) {
                havePrevious = false;
                continue;
            }
            const QPointF point{xOf(hours[i]), yOf(sum / count)};
            if (havePrevious) {
                painter.drawLine(previous, point);
            }
            previous = point;
            havePrevious = true;
        }
        painter.setClipping(false);

        // legend, right of the plot
        int y = plot.top() + 4;
        const int x = plot.right() + 14;
        for (int member = 1; member <= UtilityRefs::memberCount; member += 1) {
            painter.fillRect(QRect{x, y, 22, 4 + 2}, UtilityRefs::memberColor(member));
            painter.setPen(QColor{40, 40, 40});
            painter.drawText(QPoint{x + 28, y + 8}, QString{"Member %1"}.arg(member));
            y += 20;
        }
        painter.setPen(QPen{QColor{20, 20, 20}, 3.0});
        painter.drawLine(x, y + 3, x + 22, y + 3);
        painter.setPen(QColor{40, 40, 40});
        painter.drawText(QPoint{x + 28, y + 8}, "Mean");
        if (hasThreshold) {
            y += 20;
            painter.setPen(QPen{QColor{90, 90, 90}, 1.5, Qt::DashLine});
            painter.drawLine(x, y + 3, x + 22, y + 3);
            painter.setPen(QColor{40, 40, 40});
            painter.drawText(QPoint{x + 28, y + 8}, "Threshold");
        }
    }

private:
    QString yLabel;
    bool hasThreshold{false};
    double threshold{0.0};
    int maxHour{24};
    vector<int> hours;
    vector<vector<double>> series;
};

RefsPointGraph::RefsPointGraph(Window * parent, const UtilityRefs::MemberBasis& basis, double lon, double lat,
                               const string& runId)
    : Window{parent}
    , basis{basis}
    , lon{lon}
    , lat{lat}
    , runId{runId}
    , textInfo{this}
    , comboHorizon{this, {"12", "24", "36", "48", "60"}}
    , canvas{new PlumeCanvas{this}}
{
    setTitle("RRFS Ensemble plume - " + basis.label);
    comboHorizon.setIndex(1);
    comboHorizon.connect([this] { start(); });
    rowTop.addWidget(textInfo, 1);
    rowTop.addWidget(comboHorizon);
    box.addLayout(rowTop);
    box.addWidgetReal(canvas, 1, Qt::Alignment{});
    box.getAndShow(this);
    setSize(760, 440);
    start();
}

void RefsPointGraph::start() {
    generation += 1;
    const auto horizon = To::Int(comboHorizon.getValue());
    canvas->reset(QString::fromStdString(basis.label + " (" + basis.units + ")"), basis.hasThreshold,
                  basis.threshold, horizon);
    hours.clear();
    for (int hour = 1; hour <= horizon; hour += 1) {
        hours.push_back(hour);
    }
    dateStr.clear();
    cycle.clear();
    textInfo.setText(QString{"%1 N, %2 W  -  resolving run..."}.arg(lat, 0, 'f', 2).arg(-lon, 0, 'f', 2));
    const auto thisGeneration = generation;
    new FutureVoid{this,
        [this] {
            string date;
            string cyc;
            if (UtilityGrib::resolveSynopticRun(runId, date, cyc)) {
                dateStr = date;
                cycle = cyc;
            }
        },
        [this, thisGeneration] {
            if (thisGeneration != generation) {
                return;
            }
            if (dateStr.empty()) {
                textInfo.setText(QString{"no run available"});
                return;
            }
            fetchHour(0, thisGeneration);
        }};
}

void RefsPointGraph::fetchHour(size_t hourIndex, int gen) {
    if (gen != generation || hourIndex >= hours.size()) {
        if (gen == generation) {
            textInfo.setText(QString{"%1 N, %2 W  -  run %3 %4z  -  done"}
                .arg(lat, 0, 'f', 2).arg(-lon, 0, 'f', 2).arg(QString::fromStdString(dateStr))
                .arg(QString::fromStdString(cycle)));
        }
        return;
    }
    const auto hour = hours[hourIndex];
    new FutureVoid{this,
        [this, hour] {
            string status;
            pendingValues.assign(1, UtilityRefs::memberPointValues(basis.memberKey, dateStr, cycle, hour, lon, lat, status));
            pendingStatus = status;
        },
        [this, hourIndex, hour, gen] {
            if (gen != generation) {
                return;
            }
            canvas->addHour(hour, pendingValues.empty() ? vector<double>{} : pendingValues[0]);
            textInfo.setText(QString{"%1 N, %2 W  -  run %3 %4z  -  loaded F%5 of F%6%7"}
                .arg(lat, 0, 'f', 2).arg(-lon, 0, 'f', 2).arg(QString::fromStdString(dateStr))
                .arg(QString::fromStdString(cycle)).arg(hour).arg(hours.back())
                .arg(pendingStatus.empty() ? QString{} : "  (" + QString::fromStdString(pendingStatus) + ")"));
            fetchHour(hourIndex + 1, gen);
        }};
}

void RefsPointGraph::closeEventCustom() {
    generation += 1;   // abandon any in-flight fetch chain
}
