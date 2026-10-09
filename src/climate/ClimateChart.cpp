// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "climate/ClimateChart.h"
#include "ui/ChartExport.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPaintEvent>
#include <QString>

namespace {
    const char * const monthNames[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
}

ClimateChart::ClimateChart(const UtilityClimate::IndexInfo& info, const UtilityClimate::Series& all, int years, QWidget * parent)
    : QWidget{parent}
    , info{info}
{
    setFixedSize(480, 180);
    ChartExport::install(this, "Climate index");
    if (all.empty()) {
        return;
    }
    const auto& last = all.back();
    const auto firstYear = last.year - years;
    for (const auto& reading : all) {
        if (reading.year > firstYear || (reading.year == firstYear && reading.month >= last.month)) {
            series.push_back(reading);
        }
    }
    // a three-month index is named for its season (the series holds the middle month)
    static const char * const seasons[] = {"", "DJF", "JFM", "FMA", "MAM", "AMJ", "MJJ", "JJA", "JAS", "ASO", "SON", "OND", "NDJ"};
    const auto seasonal = info.key == "roni" || info.key == "oni";
    newest = std::string{seasonal ? seasons[last.month] : monthNames[last.month]} + " " + std::to_string(last.year) + ": " +
             QString::number(last.value, 'f', 2).toStdString() + (info.unit.empty() ? "" : " " + info.unit);
}

void ClimateChart::paintEvent(QPaintEvent *) {
    ChartPainter p{this};
    p.setRenderHint(QPainter::Antialiasing, false);
    p.fillRect(rect(), palette().window());
    const QRect plot{44, 26, width() - 54, height() - 48};
    if (series.size() < 2) {
        p.drawText(rect(), Qt::AlignCenter, "No data");
        return;
    }
    double lo = 0.0;
    double hi = 0.0;
    for (const auto& reading : series) {
        lo = std::min(lo, reading.value);
        hi = std::max(hi, reading.value);
    }
    // a round scale: a step of 0.5, 1, 2 or 5 covering the data
    const auto span = std::max(hi - lo, 0.5);
    double step = 0.5;
    for (const double candidate : {0.5, 1.0, 2.0, 5.0, 10.0}) {
        step = candidate;
        if (span / candidate <= 5.0) {
            break;
        }
    }
    const auto top = std::ceil(hi / step) * step;
    const auto bottom = std::floor(lo / step) * step;
    const auto range = std::max(top - bottom, step);
    const auto yOf = [&](double value) { return plot.bottom() - (value - bottom) / range * plot.height(); };

    // grid and the axis labels
    p.setPen(QColor{128, 128, 128, 70});
    QFont small = p.font();
    small.setPixelSize(10);
    p.setFont(small);
    for (double v = bottom; v <= top + step / 2; v += step) {
        const auto y = yOf(v);
        p.setPen(QColor{128, 128, 128, 70});
        p.drawLine(plot.left(), static_cast<int>(y), plot.right(), static_cast<int>(y));
        p.setPen(palette().text().color());
        p.drawText(QRect{0, static_cast<int>(y) - 7, plot.left() - 4, 14}, Qt::AlignRight | Qt::AlignVCenter,
                   QString::number(v, 'f', step < 1.0 ? 1 : 0));
    }
    const auto firstX = series.front().year + (series.front().month - 0.5) / 12.0;
    const auto lastX = series.back().year + (series.back().month - 0.5) / 12.0;
    const auto xRange = std::max(lastX - firstX, 1.0 / 12.0);
    const auto xOf = [&](const UtilityClimate::Reading& r) {
        return plot.left() + (r.year + (r.month - 0.5) / 12.0 - firstX) / xRange * plot.width();
    };
    // year ticks
    const auto years = static_cast<int>(std::ceil(xRange));
    const int tickEvery = years > 30 ? 10 : years > 14 ? 5 : years > 6 ? 2 : 1;
    for (int year = (static_cast<int>(firstX) / tickEvery + 1) * tickEvery; year <= lastX; year += tickEvery) {
        const auto x = plot.left() + (year - firstX) / xRange * plot.width();
        p.setPen(QColor{128, 128, 128, 70});
        p.drawLine(static_cast<int>(x), plot.top(), static_cast<int>(x), plot.bottom());
        p.setPen(palette().text().color());
        p.drawText(QRect{static_cast<int>(x) - 20, plot.bottom() + 3, 40, 14}, Qt::AlignCenter, QString::number(year));
    }

    // the bars
    const auto barWidth = std::max(1.0, plot.width() / static_cast<double>(series.size()) - 0.4);
    const auto zero = yOf(0.0);
    for (const auto& reading : series) {
        const auto x = xOf(reading);
        const auto y = yOf(reading.value);
        const QColor colour = reading.value >= 0 ? QColor{214, 70, 50} : QColor{50, 110, 200};
        p.fillRect(QRectF{x - barWidth / 2, std::min(y, zero), barWidth, std::abs(y - zero)}, colour);
    }
    // zero line and the El Niño / La Niña thresholds
    p.setPen(QColor{110, 110, 110});
    p.drawLine(plot.left(), static_cast<int>(zero), plot.right(), static_cast<int>(zero));
    if (info.threshold > 0.0) {
        QPen dashed{QColor{110, 110, 110}};
        dashed.setStyle(Qt::DashLine);
        p.setPen(dashed);
        for (const double v : {info.threshold, -info.threshold}) {
            if (v < top && v > bottom) {
                p.drawLine(plot.left(), static_cast<int>(yOf(v)), plot.right(), static_cast<int>(yOf(v)));
            }
        }
    }
    p.setPen(palette().text().color());
    p.drawRect(plot);

    // title and the newest value
    QFont bold = p.font();
    bold.setPixelSize(12);
    bold.setBold(true);
    p.setFont(bold);
    p.drawText(QRect{4, 2, width() - 8, 20}, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString(info.label));
    p.setFont(small);
    p.drawText(QRect{4, 2, width() - 8, 20}, Qt::AlignRight | Qt::AlignBottom, QString::fromStdString(newest));
}
