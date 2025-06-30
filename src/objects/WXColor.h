// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WXCOLOR_H
#define WXCOLOR_H

#include <string>
#include <QColor>
#include <QString>

using std::string;

class WXColor {
public:
    WXColor(const string&, const string&, int, int, int);
    void setValue(int) const;
    QString styleSheetCurrent() const;
    string uiLabel;
    string prefVar;
    int defaultcolorAsInt;
    int colorAsInt;
    int red;
    int green;
    int blue;
    QColor qcolor;
    QColor qcolorDefault;
};

#endif  // WXCOLOR_H
