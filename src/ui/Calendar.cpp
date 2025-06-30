// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Calendar.h"

Calendar::Calendar(Window * parent)
    : parent{parent}
    , cal{new QCalendarWidget{parent}}
{
    cal->setMinimumDate(QDate{2005, 5, 5});
}

void Calendar::connect(const function<void()>& fn) {
    QObject::connect(cal, &QCalendarWidget::clicked, parent, [this, fn] (QDate d) { this->date = d; fn(); });
}

int Calendar::getDayOfMonth() {
    return date.day();
}

int Calendar::getMonth() {
    return date.month();
}

int Calendar::getYear() {
    return date.year();
}

QCalendarWidget * Calendar::getView() {
    return cal;
}
