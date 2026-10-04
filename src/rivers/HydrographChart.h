// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HYDROGRAPHCHART_H
#define HYDROGRAPHCHART_H

#include <vector>
#include <QColor>
#include <QString>
#include <QWidget>
#include "rivers/UtilityRivers.h"

// A river hydrograph: lines against time (UTC) with horizontal flood-stage levels, a mark at "now", and a readout of every line under the
// pointer. Used for the stage and for the flow of a gauge.
class HydrographChart : public QWidget {
public:
    struct Line {
        QString name;
        QColor color;
        Qt::PenStyle style{Qt::SolidLine};
        double width{2.0};
        std::vector<UtilityRivers::Point> points;
    };
    struct Level {
        QString label;
        double value;
        QColor color;
    };
    explicit HydrographChart(QWidget * parent = nullptr);
    // `unit` labels the vertical axis; `now` (seconds since 1970) is marked when it falls inside the time span
    void setData(const std::vector<Line>&, const std::vector<Level>&, const QString& unit, long long now);
    QSize sizeHint() const override { return {820, 360}; }
    QSize minimumSizeHint() const override { return {400, 280}; }

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    QRectF plotArea() const;
    double xOf(long long time) const;
    double yOf(double value) const;
    std::vector<Line> lines;
    std::vector<Level> levels;
    QString unit;
    long long now{0};
    long long tMin{0};
    long long tMax{1};
    double vMin{0.0};
    double vMax{1.0};
    bool hover{false};
    QPointF pointer;
};

#endif  // HYDROGRAPHCHART_H
