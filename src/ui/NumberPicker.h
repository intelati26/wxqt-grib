// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NUMBERPICKER_H
#define NUMBERPICKER_H

#include <string>
#include <QBoxLayout>
#include <QSpinBox>
#include <QString>
#include "ui/Box.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Window.h"

using std::string;

class NumberPicker : public Box {
public:
    NumberPicker(Window *, const string&, const string&, int, int, int, int);
    QBoxLayout * getView();

private:
    void updateNp(const QString&);
    int getCurrentValue();
    HBox box;
    string pref;
    int defaultValue;
    Text text;
    QSpinBox * qSpinBox;
};

#endif  // NUMBERPICKER_H
