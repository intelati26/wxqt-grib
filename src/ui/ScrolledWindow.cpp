// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ScrolledWindow.h"
#include <QScroller>
#include "util/UtilityUI.h"

ScrolledWindow::ScrolledWindow(Window * parent, QBoxLayout * layout)
    : scrollArea{new QScrollArea{parent}}
{
    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(layout);
    parent->show();
}

ScrolledWindow::ScrolledWindow(Window * parent, VBox& layout)
    : scrollArea{ new QScrollArea{parent} }
{
    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(layout.getView());
    parent->show();
}
