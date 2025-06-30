// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TEXTVIEWMETAL_H
#define TEXTVIEWMETAL_H

#include <string>

using std::string;

class TextViewMetal {
public:
    TextViewMetal(double, double, const string&);
    int xPos;
    int yPos;
    string text;
    static double fontSize;
};

#endif  // TEXTVIEWMETAL_H
