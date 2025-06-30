// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYUI_H
#define UTILITYUI_H

#include <vector>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QLayout>

using std::vector;

class UtilityUI {
public:
    static vector<int> getScreenBounds();
    static bool isMobile();
    static int getImageWidth(int);
    static int getImageWidth2(int, double);
    static void updateImage(QLabel *, const QByteArray&, int);
    static void removeChildren(QLayout *);
};

#endif  // UTILITYUI_H
