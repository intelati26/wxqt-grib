// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ControlRow.h"
#include <QWidget>

void ControlRow::addWidgetReal(QWidget * widget, int, Qt::Alignment) {
    FlowBox::addWidgetReal(widget);
}

void ControlRow::addWidget(Widget2& widget, int, Qt::Alignment) {
    FlowBox::addWidget(widget);
}

void ControlRow::addLayout(Box& box, int) {
    auto * holder = new QWidget;   // a flow holds widgets, not layouts
    box.getView()->setContentsMargins(0, 0, 0, 0);
    holder->setLayout(box.getView());
    FlowBox::addWidgetReal(holder);
}

int ControlRow::heightIn(QLayout * layout, int width) {
    return layout->hasHeightForWidth() ? layout->heightForWidth(width) : layout->sizeHint().height();
}
