// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef AUTOUPDATE_H
#define AUTOUPDATE_H

#include <functional>
#include <string>
#include <QPushButton>
#include "objects/ObjectDateTime.h"
#include "objects/Timer.h"
#include "ui/ButtonToggle.h"
#include "ui/Widget2.h"
#include "ui/Window.h"
#include "util/Utility.h"
#include "util/UtilityLog.h"

using std::function;
using std::string;

class AutoUpdate : public Widget2 {
public:
    AutoUpdate(Window *, const string &, int, const function<void()> &);
    void toggleAutoUpdate();
    void updateTitle();
    void updateTitleForAutoRefresh();
    bool isActive() const;
    void stop();
    void stopNoDownload();
    void start();
    void restart();
    QPushButton * getView();
    string titleAdd;

private:
    void localDownload();
    string prefToken;
    int defaultValue;
    function<void()> downloadData;
    ButtonToggle button;
    Timer timer;
};

#endif  // AUTOUPDATE_H
