// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ZoomImage.h"
#include <algorithm>
#include <cmath>
#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

namespace {
    constexpr double zoomStep = 1.2;
    constexpr double maxAbsoluteScale = 6.0;   // relative to the image's native pixels

    QPixmap makeCrosshair() {
        constexpr int size = 21;
        constexpr int mid = size / 2;
        constexpr int gap = 3;
        QPixmap pixmap{size, size};
        pixmap.fill(Qt::transparent);
        QPainter painter{&pixmap};
        painter.setRenderHint(QPainter::Antialiasing, true);
        const auto strokes = [&] (const QColor& colour, int width) {
            painter.setPen(QPen{colour, static_cast<qreal>(width)});
            painter.drawLine(mid, 0, mid, mid - gap);
            painter.drawLine(mid, mid + gap, mid, size);
            painter.drawLine(0, mid, mid - gap, mid);
            painter.drawLine(mid + gap, mid, size, mid);
        };
        strokes(QColor{0, 0, 0, 160}, 3);
        strokes(QColor{255, 255, 255}, 1);
        painter.setPen(QPen{QColor{0, 0, 0, 160}, 1});
        painter.setBrush(QColor{255, 80, 80});
        painter.drawEllipse(QPointF{mid + 0.5, mid + 0.5}, 2.0, 2.0);
        painter.end();
        return pixmap;
    }
}

ZoomImage::ZoomImage(Window * parent)
    : QScrollArea{parent}
    , label{new QLabel{this}}
    , marker{new QLabel{label}}
{
    label->setAlignment(Qt::AlignCenter);
    label->setScaledContents(false);
    setWidget(label);
    setWidgetResizable(false);
    setAlignment(Qt::AlignCenter);
    setFrameShape(QFrame::NoFrame);
    label->setMouseTracking(true);
    viewport()->setMouseTracking(true);
    label->installEventFilter(this);
    updateRestingCursor();

    marker->setPixmap(makeCrosshair());
    marker->setFixedSize(marker->pixmap().size());
    marker->setAttribute(Qt::WA_TransparentForMouseEvents);
    marker->hide();
}

void ZoomImage::updateRestingCursor() {
    const auto cursor = crosshairMode ? Qt::CrossCursor : Qt::OpenHandCursor;
    viewport()->setCursor(cursor);
    label->setCursor(cursor);
}

void ZoomImage::setCrosshairMode(bool on) {
    crosshairMode = on;
    if (!dragging) {
        updateRestingCursor();
    }
    repositionMarker();
}

void ZoomImage::setMarker(double fx, double fy) {
    markerActive = true;
    markerFx = std::clamp(fx, 0.0, 1.0);
    markerFy = std::clamp(fy, 0.0, 1.0);
    repositionMarker();
}

void ZoomImage::clearMarker() {
    markerActive = false;
    marker->hide();
}

void ZoomImage::repositionMarker() {
    if (!markerActive || !crosshairMode || label->pixmap().isNull()) {
        marker->hide();
        return;
    }
    const auto x = static_cast<int>(std::lround(markerFx * label->width()));
    const auto y = static_cast<int>(std::lround(markerFy * label->height()));
    marker->move(x - marker->width() / 2, y - marker->height() / 2);
    marker->show();
    marker->raise();
}

bool ZoomImage::eventFilter(QObject * watched, QEvent * event) {
    if (watched == label && !source.isNull()) {
        if (event->type() == QEvent::MouseMove && !dragging) {
            const auto * mouseEvent = static_cast<QMouseEvent *>(event);
            const auto width = label->width();
            const auto height = label->height();
            if (width > 0 && height > 0) {
                emit hovered(
                    std::clamp(static_cast<double>(mouseEvent->pos().x()) / width, 0.0, 1.0),
                    std::clamp(static_cast<double>(mouseEvent->pos().y()) / height, 0.0, 1.0));
            }
        } else if (event->type() == QEvent::Leave) {
            emit hoverEnded();
        }
    }
    return QScrollArea::eventFilter(watched, event);
}

bool ZoomImage::hasImage() const {
    return !source.isNull();
}

double ZoomImage::fitScale() const {
    if (source.isNull()) {
        return 1.0;
    }
    const auto viewportSize = viewport()->size();
    const auto scaleW = static_cast<double>(viewportSize.width()) / source.width();
    const auto scaleH = static_cast<double>(viewportSize.height()) / source.height();
    return std::min({scaleW, scaleH, 1.0});
}

void ZoomImage::setBytes(const QByteArray& bytes) {
    source = QPixmap::fromImage(QImage::fromData(bytes));
    userZoomed = false;
    fitToViewport();
}

void ZoomImage::setBytesKeepView(const QByteArray& bytes) {
    const auto previous = source;
    source = QPixmap::fromImage(QImage::fromData(bytes));
    if (source.isNull()) {
        source = previous;
        return;
    }
    // keep the same magnification and scroll offsets across frames
    if (previous.isNull() || previous.size() != source.size()) {
        userZoomed = false;
        fitToViewport();
        return;
    }
    const auto hValue = horizontalScrollBar()->value();
    const auto vValue = verticalScrollBar()->value();
    applyScale(scale, QPoint{});
    horizontalScrollBar()->setValue(hValue);
    verticalScrollBar()->setValue(vValue);
}

void ZoomImage::fitToViewport() {
    if (source.isNull()) {
        return;
    }
    userZoomed = false;
    scale = fitScale();
    applyScale(scale, QPoint{});
}

void ZoomImage::applyScale(double newScale, const QPoint& anchor) {
    if (source.isNull()) {
        return;
    }
    const auto minScale = fitScale();
    newScale = std::clamp(newScale, minScale, maxAbsoluteScale);

    const auto oldSize = label->size();
    const auto scaled = source.size() * newScale;
    label->setPixmap(source.scaled(scaled, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    label->resize(label->pixmap().size());
    repositionMarker();

    // keep the pixel under `anchor` (viewport coords) stationary
    if (!anchor.isNull() && oldSize.width() > 0 && oldSize.height() > 0) {
        const auto ratioX = static_cast<double>(label->width()) / oldSize.width();
        const auto ratioY = static_cast<double>(label->height()) / oldSize.height();
        const auto contentX = (horizontalScrollBar()->value() + anchor.x()) * ratioX;
        const auto contentY = (verticalScrollBar()->value() + anchor.y()) * ratioY;
        horizontalScrollBar()->setValue(static_cast<int>(contentX - anchor.x()));
        verticalScrollBar()->setValue(static_cast<int>(contentY - anchor.y()));
    }
    scale = newScale;
}

void ZoomImage::wheelEvent(QWheelEvent * event) {
    if (source.isNull()) {
        return;
    }
    const auto steps = event->angleDelta().y() / 120.0;
    if (steps == 0.0) {
        return;
    }
    userZoomed = true;
    const auto factor = steps > 0.0 ? zoomStep : 1.0 / zoomStep;
    applyScale(scale * factor, event->position().toPoint());
    event->accept();
}

void ZoomImage::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStart = event->pos();
        viewport()->setCursor(Qt::ClosedHandCursor);
        label->setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
}

void ZoomImage::mouseMoveEvent(QMouseEvent * event) {
    if (dragging) {
        const auto delta = event->pos() - dragStart;
        dragStart = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
    }
}

void ZoomImage::mouseReleaseEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        dragging = false;
        updateRestingCursor();
        event->accept();
    }
}

void ZoomImage::mouseDoubleClickEvent(QMouseEvent * event) {
    dragging = false;
    emit doubleClicked();
    event->accept();
}

void ZoomImage::resizeEvent(QResizeEvent * event) {
    QScrollArea::resizeEvent(event);
    if (!userZoomed) {
        fitToViewport();
    }
}
