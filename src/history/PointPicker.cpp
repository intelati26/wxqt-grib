// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/PointPicker.h"
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>
#include "util/UtilityUI.h"

PointPicker::PointPicker(Window * parent, double lat, double lon, bool have, const std::function<void(double, double)>& chosen)
    : Window{parent}, lat{lat}, lon{lon}, have{have} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Pick a point");
    const auto dimens = UtilityUI::getScreenBounds();
    const int side = std::max(360, std::min(dimens[0] - 40, dimens[1] - 220));
    view = std::make_unique<MapView>(this, side);
    auto * map = view->map();
    map->dataLayer = [] (QPainter& painter) { painter.fillRect(QRectF{-1.0e6, -1.0e6, 2.0e6, 2.0e6}, QColor{16, 26, 42}); };
    map->topLayer = [this] (QPainter& painter) { paintMarker(painter); };
    map->clickHandler = [this] (const QPointF& at) {
        const auto [clickedLat, clickedLon] = view->toLatLon(at);
        this->lat = clickedLat;
        this->lon = clickedLon;
        this->have = true;
        label->setText(QString{"%1 %2, %3 %4"}.arg(std::abs(clickedLat), 0, 'f', 3).arg(clickedLat >= 0 ? "N" : "S").arg(std::abs(clickedLon), 0, 'f', 3).arg(clickedLon >= 0 ? "E" : "W"));
        view->map()->update();
        return true;
    };
    if (have) {
        view->showRegion(lat - 6.0, lat + 6.0, lon - 9.0, lon + 9.0);
    } else {
        view->showRegion(22.0, 52.0, -126.0, -64.0);
    }
    label = new QLabel{have ? QString{"%1, %2"}.arg(lat, 0, 'f', 3).arg(lon, 0, 'f', 3) : QString{"Click the map (wheel to zoom, drag to move)"}, this};
    auto * use = new QPushButton{"Use this point", this};
    QObject::connect(use, &QPushButton::clicked, [this, chosen] {
        if (this->have) {
            chosen(this->lat, this->lon);
            close();
        }
    });
    auto * column = new QVBoxLayout;
    column->addWidget(map);
    column->addWidget(label);
    column->addWidget(use);
    auto * central = new QWidget{this};
    central->setLayout(column);
    setCentralWidget(central);
}

void PointPicker::paintMarker(QPainter& painter) {
    if (!have) {
        return;
    }
    const auto at = view->toUnits(lat, lon);
    const double p = view->unitsPerPixel();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const auto& pen : {QPen{QColor{0, 0, 0, 220}, 4.0 * p}, QPen{QColor{255, 220, 40}, 2.0 * p}}) {
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(at, 7.0 * p, 7.0 * p);
        painter.drawLine(QPointF{at.x() - 13.0 * p, at.y()}, QPointF{at.x() + 13.0 * p, at.y()});
        painter.drawLine(QPointF{at.x(), at.y() - 13.0 * p}, QPointF{at.x(), at.y() + 13.0 * p});
    }
}
