// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/TabWidget.h"

TabWidget::TabWidget(Window * parent)
    : parent{parent}
    , tabWidget{new QTabWidget{parent}}
{}

// TODO FIXME
void TabWidget::addTab(QWidget * w, const string& label) {
    tabWidget->addTab(w, QString::fromStdString(label));
}

void TabWidget::connect(const function<void(int)>& fn) {
    QObject::connect(getView(), &QTabWidget::currentChanged, parent, fn);
}

void TabWidget::setIndex(int index) {
    tabWidget->setCurrentIndex(index);
}

QTabWidget * TabWidget::getView() {
    return tabWidget;
}
