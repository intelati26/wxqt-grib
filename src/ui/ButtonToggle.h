// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BUTTONTOGGLE_H
#define BUTTONTOGGLE_H

#include <functional>
#include <string>
#include <QPushButton>
#include "ui/Icon.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class ButtonToggle : public Widget2 {
public:
    ButtonToggle(Window *, Icon, const string&);
    void connect(const function<void()>&);
    void setText(const string&);
    void setVisible(bool);
    void setCheckable(bool);
    bool getActive();
    void setActive(bool);
    QPushButton * getView();

private:
    Window * parent;
    QPushButton * button;
};

#endif  // BUTTONTOGGLE_H
