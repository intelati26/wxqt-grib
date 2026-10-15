// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CONTROLROW_H
#define CONTROLROW_H

#include "ui/FlowBox.h"

// A row of controls (boxes, buttons, checkboxes) that wraps onto more lines when the window is narrower than the row, so a window can be made as narrow as its widest
// control. It takes what an HBox takes: a layout put in it (a back / forward pair) is wrapped in a widget of its own; stretch, alignment and spacing have no meaning in a flow.
class ControlRow : public FlowBox {
public:
    void addWidgetReal(QWidget *, int = 0, Qt::Alignment = {});
    void addWidget(Widget2&, int = 0, Qt::Alignment = {});
    void addLayout(Box&, int = 0);
    void addStretch() {}
    void setSpacing(int) {}
    // how tall a layout is at a width: a flow wraps, so a narrow window has more lines of controls
    static int heightIn(QLayout * layout, int width);
};

#endif  // CONTROLROW_H
