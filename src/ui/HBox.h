// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef HBOX_H
#define HBOX_H

#include <string>
#include <vector>
#include <QHBoxLayout>
#include "ui/Box.h"
#include "ui/Image.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class HBox : public Box {
public:
    HBox();
    void addWidgetReal(QWidget *, int = 0, Qt::Alignment = Qt::AlignTop | Qt::AlignBaseline);
    void addWidget(Widget2&, int = 0, Qt::Alignment = Qt::AlignTop | Qt::AlignBaseline);
    void addWidgetAndCenter(Widget2&);
    void addLayout(Box&, int = 0);
    void addStretch();
    void setSpacing(int);
    void getAndShow(Window *);
    void addImageRow(Window *, const vector<string>&, vector<Image>&, int = 0);
    void removeChildren();
    QHBoxLayout * getView();

private:
    QHBoxLayout * box;
};

#endif  // HBOX_H
