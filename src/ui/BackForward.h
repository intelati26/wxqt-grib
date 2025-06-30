// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BACKFORWARD_H
#define BACKFORWARD_H

#include <functional>
#include <string>
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Shortcut.h"
#include "ui/Window.h"

using std::function;
using std::string;

class BackForward : public HBox {
public:
    BackForward(Window *, const function<void()>&, const function<void()>&);

private:
    Button buttonBack;
    Button buttonForward;
    Shortcut shortcutBack;
    Shortcut shortcutForward;
};

#endif  // BACKFORWARD_H
