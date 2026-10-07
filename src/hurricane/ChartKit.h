// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTKIT_H
#define CHARTKIT_H

#include <cmath>
#include <functional>
#include <vector>
#include <QColor>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QRectF>
#include <QString>
#include "hurricane/UtilityEcmwfTracks.h"
#include "hurricane/UtilityEnsembleStats.h"

// The little chart toolkit the hurricane windows share: axes with a grid, a band between two series, a line through a series, a line through a run's steps.
namespace ChartKit {
    using Stats = UtilityEnsembleStats;

    inline bool has(double v) { return UtilityEcmwfTracks::has(v); }

    struct Axes {
        QRectF area;
        double xMin, xMax, yMin, yMax;
        QPointF at(double x, double y) const {
            return QPointF{area.left() + area.width() * (x - xMin) / (xMax - xMin), area.bottom() - area.height() * (y - yMin) / (yMax - yMin)};
        }
    };

    inline void frame(QPainter& p, const Axes& a, const QString& title, const QString& yUnit, double yStep, double xStep,
                              const std::function<QString(double)>& xLabel = {}) {
        p.setPen(QColor{210, 210, 210});
        p.setBrush(QColor{252, 252, 252});
        p.drawRect(a.area);
        QFont small{p.font()};
        small.setPixelSize(10);
        p.setFont(small);
        for (double y = std::ceil(a.yMin / yStep) * yStep; y <= a.yMax + 1e-9; y += yStep) {
            const auto at = a.at(a.xMin, y);
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{a.area.left(), at.y()}, QPointF{a.area.right(), at.y()});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{a.area.left() - 44, at.y() - 7, 40, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', 0));
        }
        for (double x = a.xMin; x <= a.xMax + 1e-9; x += xStep) {
            const auto at = a.at(x, a.yMin);
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{at.x(), a.area.top()}, QPointF{at.x(), a.area.bottom()});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{at.x() - 40, a.area.bottom() + 2, 80, 14}, Qt::AlignHCenter, xLabel ? xLabel(x) : QString::number(static_cast<int>(x)) + " h");
        }
        QFont bold{p.font()};
        bold.setPixelSize(12);
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(QColor{30, 30, 30});
        p.drawText(QPointF{a.area.left(), a.area.top() - 6}, title + (yUnit.isEmpty() ? QString{} : "  (" + yUnit + ")"));
    }

    // dotted lines at the category thresholds (tropical storm 34 kt, Cat 1 64, Cat 2 83, Cat 3 96, Cat 4 113, Cat 5 137), named at the right edge
    inline void categoryLines(QPainter& p, const Axes& a) {
        static const int thresholds[] = {34, 64, 83, 96, 113, 137};
        QFont small{p.font()};
        small.setPixelSize(9);
        p.setFont(small);
        for (const int w : thresholds) {
            if (w <= a.yMin || w >= a.yMax) {
                continue;
            }
            const auto at = a.at(a.xMin, w);
            p.setPen(QPen{QColor{150, 150, 150}, 1.0, Qt::DotLine});
            p.drawLine(QPointF{a.area.left(), at.y()}, QPointF{a.area.right(), at.y()});
            p.setPen(QColor{110, 110, 110});
            p.drawText(QRectF{a.area.right() - 44, at.y() - 12, 42, 11}, Qt::AlignRight | Qt::AlignBottom, QString::fromStdString(UtilityAtcf::shortCategory(w)));
        }
    }

    // a band between two series; gaps in the data end it
    template <class Low, class High>
    void band(QPainter& p, const Axes& a, const vector<Stats::Hour>& hours, Low low, High high, const QColor& color) {
        QPainterPath path;
        vector<QPointF> upper;
        vector<QPointF> lower;
        const auto flush = [&] {
            if (upper.size() >= 2) {
                QPainterPath poly;
                poly.moveTo(lower.front());
                for (const auto& pt : lower) poly.lineTo(pt);
                for (auto it = upper.rbegin(); it != upper.rend(); ++it) poly.lineTo(*it);
                poly.closeSubpath();
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                p.drawPath(poly);
            }
            upper.clear();
            lower.clear();
        };
        for (const auto& h : hours) {
            const double l = low(h);
            const double u = high(h);
            if (has(l) && has(u)) {
                lower.push_back(a.at(h.hour, l));
                upper.push_back(a.at(h.hour, u));
            } else {
                flush();
            }
        }
        flush();
    }

    template <class Value>
    void line(QPainter& p, const Axes& a, const vector<Stats::Hour>& hours, Value value, const QPen& pen) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        bool started = false;
        for (const auto& h : hours) {
            const double v = value(h);
            if (!has(v)) {
                started = false;
                continue;
            }
            const auto pt = a.at(h.hour, v);
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p.drawPath(path);
    }

    // a run's wind or pressure against forecast hour from the ECMWF cycle
    template <class Value>
    void trackLine(QPainter& p, const Axes& a, const vector<UtilityEcmwfTracks::Step>& steps, Value value, const QPen& pen) {
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        bool started = false;
        for (const auto& s : steps) {
            const double v = value(s);
            if (!has(v) || s.hour > a.xMax) {
                started = false;
                continue;
            }
            const auto pt = a.at(s.hour, v);
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p.drawPath(path);
    }
}

#endif  // CHARTKIT_H
