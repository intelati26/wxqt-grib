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
    // stateToken: a preference that keeps whether it is on ("": not kept). The interval is in minutes, from the preference prefToken.
    AutoUpdate(Window *, const string &, int, const function<void()> &, const string& stateToken = {});
    void toggleAutoUpdate();
    void updateTitle();
    void updateTitleForAutoRefresh();
    bool isActive() const;
    void stop();
    void stopNoDownload();
    void start();
    void restart();
    void startQuiet();            // the timer and the button, without a download now
    void syncWithSettings();      // follow the kept state and the interval after they were changed in the settings (a download is not made)
    QPushButton * getView();
    string titleAdd;

private:
    void localDownload();
    string prefToken;
    string stateToken;
    int activeInterval{0};
    int defaultValue;
    function<void()> downloadData;
    ButtonToggle button;
    Timer timer;
};

#endif  // AUTOUPDATE_H
