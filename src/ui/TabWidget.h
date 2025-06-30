// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TABWIDGET_H
#define TABWIDGET_H

#include <functional>
#include <string>
#include <QTabWidget>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class TabWidget : public Widget2 {
public:
    explicit TabWidget(Window *);
    void addTab(QWidget *, const string&);
    void connect(const function<void(int)>&);
    void setIndex(int);
    QTabWidget * getView();

private:
    QWidget * parent;
    QTabWidget * tabWidget;
};

#endif  // TABWIDGET_H
