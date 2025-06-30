// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MYAPPLICATION_H
#define MYAPPLICATION_H

#include <QSettings>

class MyApplication {
public:
    static void onCreate();
    static QSettings * preferences;
};

#endif  // MYAPPLICATION_H
