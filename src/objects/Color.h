// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef COLOR_H
#define COLOR_H

#include <vector>
#include <QColor>

using std::vector;

class Color {
public:
    static int rgb(int, int, int);
    static int red(int);
    static int green(int);
    static int blue(int);
    static QColor intToQColor(int);
    static int qcolorToInt(QColor);
    static vector<double> colorToHsv(const vector<double>&);
    static vector<double> hsvToColor(const vector<double>&);
    static QColor qtBlack();
    static const int redInt;
    static const int greenInt;
    static const int magentaInt;
    static const QColor RED_PEN;
    static const QColor BLUE_PEN;
    static const QColor PINK_PEN;
    static const QColor BROWN_PEN;
};

#endif  // COLOR_H
