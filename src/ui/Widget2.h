// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WIDGET2_H
#define WIDGET2_H

#include <QWidget>

class Widget2 {
public:
    virtual QWidget * getView() = 0;
    virtual ~Widget2() {};
};

#endif  // WIDGET2_H
