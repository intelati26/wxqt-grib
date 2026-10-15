// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/HodographGlyph.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include "sounding/SoundingIndices.h"

namespace {
    constexpr double heights[] = {0.0, 250.0, 500.0, 1000.0, 1500.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 7000.0, 8000.0, 9000.0};   // m above ground
    constexpr int count = 13;
    constexpr double scaleKt = 70.0;   // the glyph's radius in knots
}

HodographGlyph::HodographGlyph(QWidget * parent, const QString& station) : QWidget{parent}, id{station} {
    setFixedSize(84, 84);
    setCursor(Qt::PointingHandCursor);
    setToolTip(station + "  hodograph 0-9 km: pink 0-1, red 1-3, green 3-6, yellow 6-9 km AGL; rings at 20 and 40 kt\nClick for the sounding");
}

bool HodographGlyph::measure(const SoundingProfile& profile, std::vector<QPointF>& tips, QPointF& rightMover, double& srh03) {
    tips.clear();
    if (profile.size() < 8) {
        return false;
    }
    for (int i = 0; i < count; i++) {
        const double p = i == 0 ? profile.sfcPres() : profile.interpPresAtHght(profile.toMsl(heights[i]));
        double u = 0.0, v = 0.0;
        if (p < 0.0 || !profile.interpComponents(p, u, v)) {
            if (i == 0) {
                return false;
            }
            break;   // the profile ends below 6 km: what there is is drawn
        }
        tips.emplace_back(u, v);
    }
    if (tips.size() < 4) {
        return false;
    }
    const auto storm = SoundingIndices::bunkersRight(profile);
    if (storm.valid()) {
        rightMover = QPointF{storm.u, storm.v};
        srh03 = SoundingIndices::helicity(profile, 0.0, 3000.0, storm);
    } else {
        rightMover = QPointF{};
        srh03 = -9999.0;
    }
    return true;
}

void HodographGlyph::set(const std::vector<QPointF>& points, const QPointF& storm, double srh03) {
    tips = points;
    rightMover = storm;
    srh = srh03;
    setToolTip(id + "  hodograph 0-9 km: pink 0-1, red 1-3, green 3-6, yellow 6-9 km AGL; rings at 20 and 40 kt; orange dot: right-mover motion" +
               (srh > -9000.0 ? QString{"\n0-3 km SRH %1 m2/s2"}.arg(std::lround(srh)) : QString{}) + "\nClick for the sounding");
    update();
}

void HodographGlyph::paint(QPainter& painter, const QRectF& box, const std::vector<QPointF>& tips, const QPointF& rightMover) {
    // the look of Pivotal Weather's hodograph maps: brown rings at 20 and 40 kt, the layers pink (0-1 km), red (1-3), green (3-6) and yellow (6-9 km), each line edged in dark
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing);
    const double s = box.width() / 84.0;   // the widget is 84 pixels; a saved picture scales the same drawing
    const QPointF centre = box.center();
    const double radius = box.width() / 2.0 - 2.0 * s;
    const double perKt = radius / scaleKt;
    painter.setBrush(Qt::NoBrush);
    for (const auto& pen : {QPen{QColor{255, 255, 255, 150}, 3.0 * s}, QPen{QColor{140, 110, 90, 230}, 1.2 * s}}) {
        painter.setPen(pen);
        painter.drawEllipse(centre, 20.0 * perKt, 20.0 * perKt);
        painter.drawEllipse(centre, 40.0 * perKt, 40.0 * perKt);
        painter.drawLine(QPointF{centre.x() - 40.0 * perKt, centre.y()}, QPointF{centre.x() + 40.0 * perKt, centre.y()});
        painter.drawLine(QPointF{centre.x(), centre.y() - 40.0 * perKt}, QPointF{centre.x(), centre.y() + 40.0 * perKt});
    }
    const auto at = [&] (const QPointF& wind) {
        const double x = std::clamp(wind.x(), -scaleKt, scaleKt), y = std::clamp(wind.y(), -scaleKt, scaleKt);
        return QPointF{centre.x() + x * perKt, centre.y() - y * perKt};
    };
    const auto colorOf = [] (double top) {
        return top <= 1000.0 ? QColor{245, 170, 235} : top <= 3000.0 ? QColor{225, 25, 25} : top <= 6000.0 ? QColor{30, 175, 40} : QColor{240, 200, 20};
    };
    for (const bool edge : {true, false}) {
        for (size_t i = 0; i + 1 < tips.size(); i++) {
            painter.setPen(edge ? QPen{QColor{20, 20, 20, 230}, 4.6 * s, Qt::SolidLine, Qt::RoundCap} : QPen{colorOf(heights[i + 1]), 2.6 * s, Qt::SolidLine, Qt::RoundCap});
            painter.drawLine(at(tips[i]), at(tips[i + 1]));
        }
    }
    if (std::abs(rightMover.x()) + std::abs(rightMover.y()) > 0.0) {
        painter.setPen(QPen{Qt::black, 1.0 * s});
        painter.setBrush(QColor{255, 140, 0});
        painter.drawEllipse(at(rightMover), 2.6 * s, 2.6 * s);   // the right-mover's motion
    }
    painter.restore();
}

void HodographGlyph::paintEvent(QPaintEvent *) {
    QPainter painter{this};
    paint(painter, QRectF{0.0, 0.0, static_cast<double>(width()), static_cast<double>(height())}, tips, rightMover);
}

void HodographGlyph::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(id);
    }
}
