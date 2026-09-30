// https://wiki.qt.io/Clickable_QLabel

#include "ClickableLabel.h"
#include "util/UtilityUI.h"

ClickableLabel::ClickableLabel(Window * parent, [[maybe_unused]] Qt::WindowFlags f)
    : QLabel{parent}
    , parent{parent}
{}

void ClickableLabel::mousePressEvent(QMouseEvent * event) {
    // left button only: right-click belongs to the context menu (Photo's
    // "Save image..."), and must not also trigger the click action
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

void ClickableLabel::connect(const function<void()>& fn) {
    QObject::connect(this, &ClickableLabel::clicked, parent, fn);
}

void ClickableLabel::setToWidth(const QByteArray& ba, int width) {
    UtilityUI::updateImage(this, ba, width);
}
