// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SunViewer.h"
#include <algorithm>
#include <cmath>
#include <QMouseEvent>
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
        image = picture;
        placed = regions;
        update();
    }

protected:
    QRectF target() const {
        if (image.isNull()) {
            return {};
        }
        const double scale = std::min(static_cast<double>(width()) / image.width(), static_cast<double>(height()) / image.height());
        const double w = image.width() * scale, h = image.height() * scale;
        return QRectF{(width() - w) / 2.0, (height() - h) / 2.0, w, h};
    }
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        painter.fillRect(rect(), Qt::black);
        if (!image.isNull()) {
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.drawImage(target(), image);
        }
    }
    void mouseMoveEvent(QMouseEvent * event) override {
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
    status = new QLabel{"Reading the Sun...", top};
    UiStandards::muted(status);
    topLayout->addWidget(refresh);
    topLayout->addWidget(status, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    disc = new SunDisc{this};
    box.addWidgetReal(disc, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(860, 900);
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { load(); });
    load();
}

void SunViewer::load() {
    const int mine = ++generation;
    status->setText("Reading the Sun...");
    auto picture = std::make_shared<QImage>();
    auto placed = std::make_shared<std::vector<UtilitySun::Placed>>();
    new FutureVoid{this, [picture, placed] { *picture = UtilitySun::labelled(1100, *placed); }, [this, mine, picture, placed] {
        if (closed || mine != generation) {
            return;
        }
        if (picture->isNull()) {
            status->setText("The Sun's picture or the region list could not be read.");
            return;
        }
        disc->set(*picture, *placed);
        status->setText(QString::number(placed->size()) + " numbered regions. Point at one for its class, size and flare chances.");
    }};
}
