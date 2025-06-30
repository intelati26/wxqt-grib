// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARSTATUSBOX_H
#define RADARSTATUSBOX_H

#include <functional>
#include <string>
#include "radar/NexradLevelData.h"
#include "ui/ClickableLabel.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class RadarStatusBox : public Widget2 {
public:
    explicit RadarStatusBox(Window *);
    void setBox(const NexradLevelData&, const string&, const string&);
    void connect(const function<void()>&);
    ClickableLabel * getView();

private:
    void setCurrent(const string&);
    void setOld(const string&);
    void setText(const string&);
    void setBackGroundRed();
    void setBackGroundGreen();
    Window * parent;
    ClickableLabel * label;
};

#endif  // RADARSTATUSBOX_H
