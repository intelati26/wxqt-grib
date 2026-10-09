// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTHOVER_H
#define CHARTHOVER_H

#include <memory>
#include <QEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QObject>
#include <QPixmap>
#include <QStyle>
#include "gfs/GfsChart.h"
#include "ui/ZoomImage.h"

// The hover read-out of a chart drawn from GRIB (as the RRFS screen has it): over the picture in `label`, a box in its corner gives the position and the value of each field of the chart
// under the pointer. set() is given what render() kept (GfsChart::Options::probe) each time a new picture is shown.
class ChartHover : public QObject {
public:
    explicit ChartHover(QLabel * label) : QObject{label}, label{label}, box{new QLabel{label}} {
        label->setMouseTracking(true);
        label->installEventFilter(this);
        box->setAttribute(Qt::WA_TransparentForMouseEvents);
        box->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
        box->hide();
    }
    // over a zoomable picture instead: its hover signal gives the point as fractions of the picture, zoomed or not
    explicit ChartHover(ZoomImage * zoom) : QObject{zoom}, label{nullptr}, box{new QLabel{zoom}} {
        box->setAttribute(Qt::WA_TransparentForMouseEvents);
        box->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
        box->hide();
        QObject::connect(zoom, &ZoomImage::hovered, this, [this] (double fx, double fy) { show(fx, fy, 12, 12); });
        QObject::connect(zoom, &ZoomImage::hoverEnded, this, [this] { box->hide(); });
    }
    void set(std::shared_ptr<GfsChart::Probe> p) {
        probe = std::move(p);
        box->hide();
    }
    bool eventFilter(QObject * object, QEvent * event) override {
        if (object != label) {
            return false;
        }
        if (event->type() == QEvent::Leave) {
            box->hide();
        } else if (event->type() == QEvent::MouseMove && probe && probe->valid()) {
            const auto pix = label->pixmap();
            if (pix.isNull()) {
                return false;
            }
            const QSize size = pix.deviceIndependentSize().toSize();
            const QRect shown = QStyle::alignedRect(label->layoutDirection(), label->alignment(), size, label->contentsRect());
            const auto at = static_cast<QMouseEvent *>(event)->position();
            show((at.x() - shown.left()) / shown.width(), (at.y() - shown.top()) / shown.height(), shown.left() + 12, shown.top() + 12);
        }
        return false;
    }
    void show(double fx, double fy, int x, int y) {
        const auto text = probe && probe->valid() ? probe->read(fx, fy) : QString{};
        if (text.isEmpty()) {
            box->hide();
            return;
        }
        box->setText(text);
        box->adjustSize();
        box->move(x, y);
        box->show();
        box->raise();
    }

private:
    QLabel * label;
    QLabel * box;
    std::shared_ptr<GfsChart::Probe> probe;
};

#endif  // CHARTHOVER_H
