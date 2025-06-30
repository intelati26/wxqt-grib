// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CALENDAR_H
#define CALENDAR_H

#include <functional>
#include <QCalendarWidget>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;

class Calendar : public Widget2 {
public:
    explicit Calendar(Window * parent);
    void connect(const function<void()>&);
    int getYear();
    int getMonth();
    int getDayOfMonth();
    QCalendarWidget * getView();

private:
    Window * parent;
    QCalendarWidget * cal;
    QDate date;
};

#endif  // CALENDAR_H
