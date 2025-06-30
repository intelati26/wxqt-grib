#include "DividerLine.h"
#include <QRect>

DividerLine::DividerLine(Window * parent)
    : line{new QFrame{parent}}
{
    line->setGeometry(QRect(320, 150, 118, 3));
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
}

QFrame * DividerLine::getView() {
    return line;
}
