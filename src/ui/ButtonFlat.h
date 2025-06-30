// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BUTTONFLAT_H
#define BUTTONFLAT_H

#include <functional>
#include <string>
#include <QPushButton>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class ButtonFlat : public Widget2 {
public:
    ButtonFlat(Window *, const string&, const string&);
    void connect(const function<void()>&);
    void setText(const string&);
    void setVisible(bool);
    void refresh();
    static int getIconSize();
    QPushButton * getView();

private:
    Window * parent;
    QPushButton * button;
    static int iconSize;
};

#endif  // BUTTONFLAT_H
