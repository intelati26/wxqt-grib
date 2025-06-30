// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "TwoWidgetScroll.h"
#include <QScroller>
#include <QScrollArea>
#include "util/UtilityUI.h"

TwoWidgetScroll::TwoWidgetScroll(Window * parent, Widget2& w1, Widget2& w2)
    : scrollArea{new QScrollArea{parent}}
{
    vbox0.addWidget(w1);
    vbox0.setAlignment(w1, Qt::AlignTop);

    vbox1.addWidget(w2);

    hbox.addLayout(vbox0);
    hbox.addLayout(vbox1);

    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(hbox.getView());
    parent->show();
}

TwoWidgetScroll::TwoWidgetScroll(Window * parent, Box& w1, Widget2& w2)
    : scrollArea{ new QScrollArea{parent} }
{
    vbox0.addLayout(w1);
    vbox0.addStretch();

    vbox1.addWidget(w2);
    vbox1.setAlignment(w2, Qt::AlignTop);

    hbox.addLayout(vbox0);
    hbox.addLayout(vbox1);

    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(hbox.getView());
    parent->show();
}

TwoWidgetScroll::TwoWidgetScroll(Window * parent, Widget2& w1, Box& w2)
    : scrollArea{ new QScrollArea{parent} }
{
    vbox0.addWidget(w1);
    vbox0.setAlignment(w1, Qt::AlignTop);

    vbox1.addLayout(w2);

    hbox.addLayout(vbox0);
    hbox.addLayout(vbox1);

    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(hbox.getView());
    parent->show();
}

TwoWidgetScroll::TwoWidgetScroll(Window * parent, Box& w1, Box& w2)
    : scrollArea{ new QScrollArea{parent} }
{
    vbox0.addLayout(w1);
    vbox1.addLayout(w2);

    hbox.addLayout(vbox0);
    hbox.addLayout(vbox1);

    scrollArea->setWidget(parent->centralWidget);
    scrollArea->setWidgetResizable(true);

    if (UtilityUI::isMobile()) {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::TouchGesture);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    } else {
        QScroller::grabGesture(scrollArea->viewport(), QScroller::LeftMouseButtonGesture);
    }
    parent->setCentralWidget(scrollArea);
    parent->centralWidget->setLayout(hbox.getView());
    parent->show();
}
