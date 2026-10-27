// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTHOVER_H
#define CHARTHOVER_H

#include <algorithm>
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
    void hide() {
        box->hide();
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
        // the box is as wide as its longest line, which may be a long chart title: held to the width of the pane it is in (the long lines wrap), and kept inside it, down as well as across
        QWidget * pane = box->parentWidget();
        const int room = pane != nullptr ? std::max(120, pane->width() - 2 * 8) : QWIDGETSIZE_MAX;
        box->setMinimumSize(0, 0);
        box->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        box->setWordWrap(false);
        box->setText(text);
        box->adjustSize();
        if (box->width() > room) {
            box->setWordWrap(true);
            box->setFixedWidth(room);
            box->setFixedHeight(box->heightForWidth(room) + 6);
        }
        if (pane != nullptr) {
            x = std::clamp(x, 8, std::max(8, pane->width() - box->width() - 8));
            y = std::clamp(y, 4, std::max(4, pane->height() - box->height() - 4));
        }
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
