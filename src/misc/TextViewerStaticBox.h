// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TEXTVIEWERSTATICBOX_H
#define TEXTVIEWERSTATICBOX_H

#include <string>
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Widget.h"
#include "ui/Window.h"

using std::string;

class TextViewerStaticBox : public Widget {
public:
    TextViewerStaticBox(Window *, const string&);

private:
    VBox box;
    Text text;
};

#endif  // TEXTVIEWERSTATICBOX_H
