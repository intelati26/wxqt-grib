// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BUTTONICON_H
#define BUTTONICON_H

#include <string>
#include <QPushButton>

using std::string;

class ButtonIcon {
public:
    ButtonIcon(QPushButton *, const string&, int = 0);
};

#endif  // BUTTONICON_H
