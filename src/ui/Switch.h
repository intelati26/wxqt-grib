// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SWITCH_H
#define SWITCH_H

#include <string>
#include <QCheckBox>
#include "objects/PrefBool.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::string;

class Switch : public Widget2 {
public:
    Switch(Window *, const string&, const string&, bool);
    static Switch * fromPrefBool(Window *, const PrefBool&);
    bool isTrue();
    void toggle();
    QCheckBox * getView();

private:
    string pref;
    string defaultValueAsString;
    QCheckBox * checkBox;
};

#endif  // SWITCH_H
