// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BUTTON_H
#define BUTTON_H

#include <functional>
#include <string>
#include <QPushButton>
#include "ui/Icon.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class Button : public Widget2 {
public:
    Button(Window *, Icon, const string&);
    void connect(const function<void()>&);
    void setText(const string&);
    string getText() const;
    void setVisible(bool);
    QPushButton * getView();

private:
    Window * parent;
    QPushButton * button;
};

#endif  // BUTTON_H
