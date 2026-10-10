// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTMAP_H
#define DROUGHTMAP_H

#include <functional>
#include <memory>
#include <vector>
#include <QImage>
#include <QPainterPath>
#include <QString>
#include <QWidget>
#include "drought/UtilityDrought.h"

// The map of the drought screen, drawn from the Monitor's shapes (sharp at any zoom): the states as land, the drought categories over them, the borders of the states (and the counties
// when zoomed in), the chosen area outlined with the rest dimmed, and, for a change between two weeks, the cells that moved a category in place of the categories. Wheel to zoom at the
// pointer, drag to pan, double click to fit the area; the pointer's position reads out the category.
class DroughtMap : public QWidget {
public:
    struct ChangeLayer {
        QImage image;                    // one pixel a cell of the grid; transparent where nothing changed
        double west{0}, north{0}, step{0.05};
        std::vector<int8_t> moved;       // the categories each cell moved (for the pointer's read-out)
        int columns{0}, rows{0};
    };
    explicit DroughtMap(QWidget * parent = nullptr);
    void setLand(std::shared_ptr<const std::vector<UtilityDrought::Area>> states);
    void setCounties(std::shared_ptr<const std::vector<UtilityDrought::Area>> counties);
    void setMonitor(std::shared_ptr<const UtilityDrought::Monitor> monitor);
    void setChange(std::shared_ptr<const ChangeLayer> change);   // null: show the categories
    void setArea(std::vector<UtilityDrought::Area> area, bool fit);   // empty: the whole country
    void setCaption(const QString& text);
    void fitArea();

private:
    void paintEvent(QPaintEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void mouseDoubleClickEvent(QMouseEvent *) override;
    QTransform worldToScreen() const;
    QPointF toWorld(const QPointF& screen) const;
    QString readout(const QPointF& world) const;
    std::shared_ptr<const std::vector<UtilityDrought::Area>> land, counties;
    std::shared_ptr<const UtilityDrought::Monitor> monitor;
    std::shared_ptr<const ChangeLayer> change;
    std::vector<UtilityDrought::Area> area;
    QPainterPath categoryPath[5];       // world coordinates (longitude, latitude)
    QPainterPath landPath, bordersPath, countyPath, areaPath, outsidePath;
    QString caption;
    double centerLon{-96.0}, centerLat{38.0}, pixelsPerDegree{16.0};   // pixels for a degree of latitude; a degree of longitude is shorter by the cosine of the center latitude
    bool dragging{false};
    QPointF dragFrom;
};

#endif  // DROUGHTMAP_H
