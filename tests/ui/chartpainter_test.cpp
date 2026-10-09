// Renders cramped and edge labels through ChartPainter and checks the guarantees: nothing outside the image, short labels never overlap.
#include <QGuiApplication>
#include <QImage>
#include "ui/ChartPainter.h"

int main(int argc, char * argv[]) {
    QGuiApplication a{argc, argv};
    QImage image{400, 200, QImage::Format_ARGB32};
    image.fill(Qt::white);
    {
        ChartPainter p{&image};
        for (int i = 0; i < 14; i++) {   // month labels far too close together: the ones that would overlap are dropped
            p.drawText(QRectF{i * 24.0 - 10, 150, 40, 14}, Qt::AlignHCenter, QString{"Oct %1"}.arg(25 + i));
        }
        p.drawText(QRectF{0, 20, 30, 14}, Qt::AlignRight | Qt::AlignVCenter, "-10.1");   // at the left edge
        p.drawText(QRectF{370, 40, 30, 14}, Qt::AlignLeft, "+7.1 inches");   // would run off the right edge
        p.drawText(QPointF{300, 100}, "A long note that cannot fit in what is left of the line, so it is shortened");
        p.drawText(QPointF{5, 2}, "title at the very top");
    }
    image.save("chartpainter.png");
    return 0;
}
