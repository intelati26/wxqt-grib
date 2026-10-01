// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SCROLLEDWINDOW_H
#define SCROLLEDWINDOW_H

#include <QBoxLayout>
#include <QScrollArea>
#include "ui/VBox.h"
#include "ui/Window.h"

class ScrolledWindow {
public:
    ScrolledWindow(Window *, QBoxLayout *);
    ScrolledWindow(Window *, VBox&);
    void enableMiddleDrag();   // hold the middle button and drag to pan the page

private:
    QScrollArea * scrollArea;
};

#endif  // SCROLLEDWINDOW_H
