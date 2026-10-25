// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include <set>
#include <QPushButton>
#include "ui/ZoomImage.h"
#include "ui/ChartExport.h"
#include <QMenu>
#include <QRegularExpression>
#include "objects/UtilityAnimationExport.h"
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

namespace {
    // the lines of the linked cursor: the whole height and width of the picture through one point, dark under light so that they show on any chart, and a ring at the point
    class LinkedCursor : public QWidget {
    public:
        explicit LinkedCursor(QWidget * parent) : QWidget{parent} {
            setAttribute(Qt::WA_TransparentForMouseEvents);
            setAttribute(Qt::WA_NoSystemBackground);
        }
        QPointF at;
    protected:
        void paintEvent(QPaintEvent *) override {
            QPainter painter{this};
            painter.setRenderHint(QPainter::Antialiasing, false);
            const auto lines = [&] (const QColor& color, double width) {
                painter.setPen(QPen{color, width});
                painter.drawLine(QPointF{0.0, at.y()}, QPointF{static_cast<double>(width_()), at.y()});
                painter.drawLine(QPointF{at.x(), 0.0}, QPointF{at.x(), static_cast<double>(height())});
            };
            lines(QColor{0, 0, 0, 150}, 3.0);
            lines(QColor{255, 255, 255, 235}, 1.0);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 190}, 3.0});
            painter.drawEllipse(at, 7.0, 7.0);
            painter.setPen(QPen{QColor{255, 220, 40}, 1.6});
            painter.drawEllipse(at, 7.0, 7.0);
        }
    private:
        int width_() const { return width(); }
    };
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
    label->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(label, &QLabel::customContextMenuRequested, this, [this] (const QPoint& point) {
        if (!saveMenu || sourceBytes.isEmpty()) {
            return;
        }
        ChartExport::Capabilities caps;   // the one menu (PNG, PDF, the clipboard, more formats); a screen with a loop gives its own save for the last item
        caps.still = sourceBytes;
        QString name = window()->windowTitle();
        name.replace(QRegularExpression{"[^A-Za-z0-9]+"}, "_");
        caps.title = name.left(60).isEmpty() ? QString{"image"} : name.left(60);
        caps.saveLoop = saveLoopHandler;
        ChartExport::popupMenu(this, label->mapToGlobal(point), caps);
    });
    updateRestingCursor();
    const auto changed = [this] { if (!applying && !source.isNull()) { emit viewChanged(); } };
    QObject::connect(horizontalScrollBar(), &QScrollBar::valueChanged, this, changed);
    QObject::connect(verticalScrollBar(), &QScrollBar::valueChanged, this, changed);

    marker->setPixmap(makeCrosshair());
    marker->setFixedSize(marker->pixmap().size());
    marker->setAttribute(Qt::WA_TransparentForMouseEvents);
    marker->hide();
    linked = new LinkedCursor{label};
    linked->hide();
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

void ZoomImage::setLinkedCursor(double fx, double fy) {
    linkedActive = true;
    linkedFx = std::clamp(fx, 0.0, 1.0);
    linkedFy = std::clamp(fy, 0.0, 1.0);
    repositionMarker();
}

void ZoomImage::clearLinkedCursor() {
    linkedActive = false;
    linked->hide();
}

void ZoomImage::clearMarker() {
    markerActive = false;
    marker->hide();
}

void ZoomImage::setPins(const std::vector<Pin>& list) {
    clearPins();
    pins = list;
    for (const auto& pin : pins) {
        auto * button = new QPushButton{label};
        button->setFixedSize(11, 11);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(pin.tip);
        button->setStyleSheet("QPushButton{background:#ffd400;border:1px solid #000;border-radius:5px;} QPushButton:hover{background:#ffffff;}");
        const QString id = pin.id;
        QObject::connect(button, &QPushButton::clicked, this, [this, id] { emit pinClicked(id); });
        button->hide();
        pinButtons.push_back(button);
    }
    repositionPins();
}

void ZoomImage::clearPins() {
    for (auto * button : pinButtons) {
        button->deleteLater();
    }
    pinButtons.clear();
    pins.clear();
}

void ZoomImage::addOverlay(QWidget * widget, double fx, double fy) {
    widget->setParent(label);
    overlays.push_back({widget, fx, fy});
    repositionPins();
}

void ZoomImage::moveOverlay(QWidget * widget, double fx, double fy) {
    for (auto& o : overlays) {
        if (o.widget == widget) {
            o.fx = fx;
            o.fy = fy;
        }
    }
    repositionPins();
}

void ZoomImage::removeOverlay(QWidget * widget) {
    overlays.erase(std::remove_if(overlays.begin(), overlays.end(), [widget] (const Overlay& o) { return o.widget == widget; }), overlays.end());
    widget->deleteLater();
}

void ZoomImage::clearOverlays() {
    for (auto& o : overlays) {
        o.widget->deleteLater();
    }
    overlays.clear();
}

QRectF ZoomImage::visibleFraction() const {
    if (label->width() <= 0 || label->height() <= 0) {
        return {};
    }
    const double w = label->width(), h = label->height();
    const double left = horizontalScrollBar()->value() - std::max(0, (viewport()->width() - label->width()) / 2);
    const double top = verticalScrollBar()->value() - std::max(0, (viewport()->height() - label->height()) / 2);
    const QRectF view{left, top, static_cast<double>(viewport()->width()), static_cast<double>(viewport()->height())};
    const QRectF all{0.0, 0.0, w, h};
    const QRectF shown = view.intersected(all);
    return {shown.left() / w, shown.top() / h, shown.width() / w, shown.height() / h};
}

QSize ZoomImage::pictureSize() const {
    return label->size();
}

void ZoomImage::setOverlayClip(const QRectF& fractions) {
    overlayClip = fractions;
    repositionPins();
}

void ZoomImage::repositionPins() {
    const QRect clip{static_cast<int>(std::lround(overlayClip.left() * label->width())), static_cast<int>(std::lround(overlayClip.top() * label->height())),
                     static_cast<int>(std::lround(overlayClip.width() * label->width())), static_cast<int>(std::lround(overlayClip.height() * label->height()))};
    for (const auto& o : overlays) {
        const int x = static_cast<int>(std::lround(o.fx * label->width()));
        const int y = static_cast<int>(std::lround(o.fy * label->height()));
        o.widget->move(x - o.widget->width() / 2, y - o.widget->height() / 2);
        const QRect inside = clip.translated(-o.widget->pos()).intersected(o.widget->rect());   // the part of the widget in the frame, in its own coordinates
        if (inside.isEmpty()) {
            o.widget->clearMask();
            o.widget->hide();
            continue;
        }
        o.widget->setMask(QRegion{inside});
        o.widget->setVisible(!label->pixmap().isNull());
        o.widget->raise();
    }
    if (pinButtons.empty()) {
        return;
    }
    const bool have = !label->pixmap().isNull();
    std::set<std::pair<int, int>> taken;
    for (size_t i = 0; i < pinButtons.size() && i < pins.size(); i++) {
        auto * button = pinButtons[i];
        const int x = static_cast<int>(std::lround(pins[i].fx * label->width()));
        const int y = static_cast<int>(std::lround(pins[i].fy * label->height()));
        if (!have || !taken.insert({x / 13, y / 13}).second) {
            button->hide();
            continue;
        }
        button->move(x - button->width() / 2, y - button->height() / 2);
        button->show();
        button->raise();
    }
}

void ZoomImage::repositionMarker() {
    repositionPins();
    if (linkedActive && !label->pixmap().isNull()) {
        auto * cursor = static_cast<LinkedCursor *>(linked);
        cursor->setGeometry(label->rect());
        cursor->at = QPointF{linkedFx * label->width(), linkedFy * label->height()};
        cursor->show();
        cursor->raise();
        cursor->update();
    } else {
        linked->hide();
    }
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
    sourceBytes = bytes;
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
    sourceBytes = bytes;
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
    const struct Guard { bool& flag; bool before; ~Guard() { flag = before; } } guard{applying, applying};
    applying = true;
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
    emit viewChanged();
}

void ZoomImage::mousePressEvent(QMouseEvent * event) {
    if (event->button() == Qt::LeftButton) {
        dragging = true;
        dragStart = event->pos();
        pressGlobal = event->globalPosition().toPoint();
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
        const auto wasDragging = dragging;
        dragging = false;
        updateRestingCursor();
        const auto releaseGlobal = event->globalPosition().toPoint();
        if (wasDragging && !source.isNull() && (releaseGlobal - pressGlobal).manhattanLength() < 4) {
            const auto local = label->mapFromGlobal(releaseGlobal);
            if (label->width() > 0 && label->height() > 0 && label->rect().contains(local)) {
                emit clicked(static_cast<double>(local.x()) / label->width(),
                             static_cast<double>(local.y()) / label->height());
            }
        }
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

ZoomImage::View ZoomImage::view() const {
    View v;
    v.fitted = !userZoomed;
    const double fit = fitScale();
    v.zoom = fit > 0.0 ? scale / fit : 1.0;
    if (label->width() > 0 && label->height() > 0) {
        v.cx = std::clamp((horizontalScrollBar()->value() + viewport()->width() / 2.0) / label->width(), 0.0, 1.0);
        v.cy = std::clamp((verticalScrollBar()->value() + viewport()->height() / 2.0) / label->height(), 0.0, 1.0);
    }
    return v;
}

void ZoomImage::setView(const View& v) {
    if (source.isNull()) {
        return;
    }
    const struct Guard { bool& flag; bool before; ~Guard() { flag = before; } } guard{applying, applying};
    applying = true;
    if (v.fitted) {
        fitToViewport();
        return;
    }
    userZoomed = true;
    applyScale(fitScale() * v.zoom, QPoint{});
    horizontalScrollBar()->setValue(static_cast<int>(v.cx * label->width() - viewport()->width() / 2.0));
    verticalScrollBar()->setValue(static_cast<int>(v.cy * label->height() - viewport()->height() / 2.0));
}
