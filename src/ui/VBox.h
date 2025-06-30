// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef VBOX_H
#define VBOX_H

#include <string>
#include <vector>
#include <QVBoxLayout>
#include "ui/Box.h"
#include "ui/HBox.h"
#include "ui/Image.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class VBox : public Box {
public:
    VBox();
    void addWidgetReal(QWidget *, int = 0, Qt::Alignment = Qt::AlignTop | Qt::AlignBaseline);
    void addWidget(Widget2&, int = 0, Qt::Alignment = Qt::AlignTop | Qt::AlignBaseline);
    void addWidgetAndCenter(Widget2&);
    void addLayoutReal(QLayout *, int = 0);
    void addLayout(Box&, int = 0);
    void addStretch();
    void setSpacing(int);
    void addMargins();
    void setAlignment(QWidget *, Qt::Alignment);
    void setAlignment(Widget2&, Qt::Alignment);
    void getAndShow(Window *);
    void removeChildren();
    void addImageRows(Window *, const vector<string>&, vector<Image>&, int, int = 0);
    QVBoxLayout * getView();

private:
    QVBoxLayout * box;
    vector<HBox> boxRows;
};

#endif  // VBOX_H
