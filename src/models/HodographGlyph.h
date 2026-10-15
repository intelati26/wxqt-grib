// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HODOGRAPHGLYPH_H
#define HODOGRAPHGLYPH_H

#include <string>
#include <vector>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>
#include "sounding/SoundingProfile.h"

// A small hodograph of one model sounding, to sit on a map at its station, drawn as Pivotal Weather's are: the wind vector tips from the surface to 9 km (pink to 1 km, red to
// 3, green to 6, yellow above), rings at 20 and 40 kt, the right-mover storm motion as a dot; no box round it. The 0-3 km helicity is in its tooltip. A click asks for the sounding.
class HodographGlyph : public QWidget {
    Q_OBJECT

public:
    explicit HodographGlyph(QWidget * parent, const QString& station);
    // the numbers drawn, from a profile (surface to 6 km above ground)
    static bool measure(const SoundingProfile& profile, std::vector<QPointF>& tips, QPointF& rightMover, double& srh03);
    void set(const std::vector<QPointF>& tips, const QPointF& rightMover, double srh03);
    QString station() const { return id; }
    // the drawing, into any painter: a square `box` of it (drawn for 84 pixels, scaled to the box)
    static void paint(class QPainter&, const QRectF& box, const std::vector<QPointF>& tips, const QPointF& rightMover);

signals:
    void clicked(const QString& station);

protected:
    void paintEvent(QPaintEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;

private:
    QString id;
    std::vector<QPointF> tips;   // knots, u east and v north, at 0 0.25 0.5 1 1.5 2 3 4 5 6 7 8 9 km
    QPointF rightMover;
    double srh{-9999.0};
};

#endif  // HODOGRAPHGLYPH_H
