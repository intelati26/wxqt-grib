// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SunViewer.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QPushButton>
#include <QHBoxLayout>
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"

// the picture fitted to the widget, the regions found under the pointer
class SunDisc : public QWidget {
public:
    explicit SunDisc(QWidget * parent) : QWidget{parent}, pill{new QLabel{this}} {
        setMouseTracking(true);
        setMinimumSize(480, 480);
        UiStandards::hoverPill(pill, 225);
        pill->hide();
        pill->setAttribute(Qt::WA_TransparentForMouseEvents);
    }
    void set(const QImage& picture, const std::vector<UtilitySun::Placed>& regions) {
        if (image.isNull() || image.width() != picture.width()) {   // another size is another picture: back to the whole disc
            zoom = 1.0;
            offset = QPointF{};
        }
        image = picture;
        placed = regions;
        update();
    }

protected:
    // where the picture is: fitted to the widget, then zoomed about the middle and moved by the drag
    QRectF target() const {
        if (image.isNull()) {
            return {};
        }
        const double scale = std::min(static_cast<double>(width()) / image.width(), static_cast<double>(height()) / image.height()) * zoom;
        const double w = image.width() * scale, h = image.height() * scale;
        return QRectF{(width() - w) / 2.0 + offset.x(), (height() - h) / 2.0 + offset.y(), w, h};
    }
    void wheelEvent(QWheelEvent * event) override {   // zoom about the pointer
        const double factor = event->angleDelta().y() > 0 ? 1.25 : 0.8;
        const double wanted = std::clamp(zoom * factor, 1.0, 16.0);
        const QPointF pointer = event->position();
        const auto before = target();
        if (before.isEmpty()) {
            return;
        }
        const double fx = (pointer.x() - before.left()) / before.width(), fy = (pointer.y() - before.top()) / before.height();
        zoom = wanted;
        const double scale = std::min(static_cast<double>(width()) / image.width(), static_cast<double>(height()) / image.height()) * zoom;
        const double w = image.width() * scale, h = image.height() * scale;
        offset = QPointF{pointer.x() - fx * w - (width() - w) / 2.0, pointer.y() - fy * h - (height() - h) / 2.0};
        if (zoom <= 1.0) {
            offset = QPointF{};
        }
        pill->hide();
        update();
    }
    void mousePressEvent(QMouseEvent * event) override {
        dragging = event->button() == Qt::LeftButton;
        last = event->position();
    }
    void mouseReleaseEvent(QMouseEvent *) override { dragging = false; }
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        painter.fillRect(rect(), Qt::black);
        if (!image.isNull()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(target(), image);
        }
    }
    void mouseMoveEvent(QMouseEvent * event) override {
        if (dragging && zoom > 1.0) {
            offset += event->position() - last;
            last = event->position();
            pill->hide();
            update();
            return;
        }
        const auto t = target();
        if (t.isEmpty()) {
            return;
        }
        const double scale = t.width() / image.width();
        const UtilitySun::Placed * best = nullptr;
        double bestDistance = 1e9;
        for (const auto& p : placed) {
            const QPointF at{t.left() + p.position.x() * scale, t.top() + p.position.y() * scale};
            const double d = std::hypot(at.x() - event->position().x(), at.y() - event->position().y());
            if (d < bestDistance) {
                bestDistance = d;
                best = &p;
            }
        }
        if (best == nullptr || bestDistance > 26.0) {
            pill->hide();
            return;
        }
        pill->setText(UtilitySun::describe(*best));
        pill->adjustSize();
        QPoint at = event->position().toPoint() + QPoint{16, 16};
        at.setX(std::min(at.x(), std::max(0, width() - pill->width() - 4)));
        at.setY(std::min(at.y(), std::max(0, height() - pill->height() - 4)));
        pill->move(at);
        pill->show();
        pill->raise();
    }
    void leaveEvent(QEvent *) override { pill->hide(); }

private:
    QImage image;
    std::vector<UtilitySun::Placed> placed;
    QLabel * pill;
    double zoom{1.0};
    QPointF offset, last;
    bool dragging{false};
};

SunViewer::SunViewer(Window * parent)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Sun - active regions (SDO/HMI, NOAA SWPC)");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    auto * refresh = UiStandards::button(top, "Refresh", "Read the picture and the region list again");
    resolution = UiStandards::combo(top);
    resolution->addItem("1024 px", 1024);
    resolution->addItem("2048 px (about 1.4 MB)", 2048);
    resolution->addItem("4096 px (full resolution, about 5 MB)", 4096);
    resolution->setCurrentIndex(1);
    resolution->setToolTip("The size of SDO's picture of the Sun. Scroll to zoom into it, drag to move it");
    status = new QLabel{"Reading the Sun...", top};
    UiStandards::muted(status);
    topLayout->addWidget(refresh);
    topLayout->addWidget(resolution);
    topLayout->addWidget(status, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    disc = new SunDisc{this};
    box.addWidgetReal(disc, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(860, 900);
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(resolution, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    load();
}

void SunViewer::load() {
    const int mine = ++generation;
    status->setText("Reading the Sun...");
    auto picture = std::make_shared<QImage>();
    auto placed = std::make_shared<std::vector<UtilitySun::Placed>>();
    const int width = resolution->currentData().toInt();
    new FutureVoid{this, [picture, placed, width] { *picture = UtilitySun::labelled(width, *placed); }, [this, mine, picture, placed] {
        if (closed || mine != generation) {
            return;
        }
        if (picture->isNull()) {
            status->setText("The Sun's picture or the region list could not be read.");
            return;
        }
        disc->set(*picture, *placed);
        status->setText(QString::number(placed->size()) + " numbered regions. Point at one for its class, size and flare chances; scroll to zoom, drag to move.");
    }};
}
