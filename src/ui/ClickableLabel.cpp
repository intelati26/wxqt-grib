// https://wiki.qt.io/Clickable_QLabel

#include "ClickableLabel.h"
#include "util/UtilityUI.h"

ClickableLabel::ClickableLabel(Window * parent, [[maybe_unused]] Qt::WindowFlags f)
    : QLabel{parent}
    , parent{parent}
{}

void ClickableLabel::mousePressEvent([[maybe_unused]] QMouseEvent * event) {
    emit clicked();
}

void ClickableLabel::connect(const function<void()>& fn) {
    QObject::connect(this, &ClickableLabel::clicked, parent, fn);
}

void ClickableLabel::setToWidth(const QByteArray& ba, int width) {
    UtilityUI::updateImage(this, ba, width);
}
