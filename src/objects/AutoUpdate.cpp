// *****************************************************************************
// * Copyright (c) 2020, 2021 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "AutoUpdate.h"
#include <algorithm>
#include "util/To.h"

AutoUpdate::AutoUpdate(Window * parent, const string &prefToken, int defaultValue, const function<void()> &downloadData, const string& stateToken)
    : prefToken{prefToken}
    , stateToken{stateToken}
    , defaultValue{defaultValue}
    , downloadData{downloadData}
    , button{parent, Update, "Auto Update ctrl-u"}
    , timer{parent, [this] { localDownload(); }}
{
    button.setCheckable(true);
    button.connect([this] { toggleAutoUpdate(); });
}

void AutoUpdate::localDownload() {
    updateTitle();
    downloadData();
}

void AutoUpdate::toggleAutoUpdate() {
    if (isActive()) {
        stop();
        localDownload();
    } else {
        start();
    }
}

void AutoUpdate::updateTitle() {
    if (isActive()) {
        updateTitleForAutoRefresh();
    }
}

void AutoUpdate::updateTitleForAutoRefresh() {
    const auto dataRefreshInterval = Utility::readPrefInt(prefToken, defaultValue);
    titleAdd = " (Update every " + To::string(dataRefreshInterval) + " min, last update: " + ObjectDateTime::getLocalTimeAsString() + ")";
}

bool AutoUpdate::isActive() const {
    return timer.isRunning();
}

void AutoUpdate::stop() {
    if (!stateToken.empty()) {
        Utility::writePref(stateToken, "false");
    }
    button.setActive(false);
    UtilityLog::d("Timer DISABLED: " + prefToken);
    timer.stop();
    titleAdd = "";
}

void AutoUpdate::stopNoDownload() {
    if (isActive()) {
        if (!stateToken.empty()) {
            Utility::writePref(stateToken, "false");
        }
        UtilityLog::d("Timer DISABLED: " + prefToken);
        button.setActive(false);
        timer.stop();
        titleAdd = "";
    }
}

void AutoUpdate::startQuiet() {
    activeInterval = std::max(1, Utility::readPrefInt(prefToken, defaultValue));
    button.setActive(true);
    timer.start(activeInterval * 60 * 1000);
    updateTitleForAutoRefresh();
}

void AutoUpdate::syncWithSettings() {
    if (stateToken.empty()) {
        return;
    }
    const bool wanted = Utility::readPref(stateToken, "true") != "false";
    const int interval = std::max(1, Utility::readPrefInt(prefToken, defaultValue));
    if (wanted && (!isActive() || interval != activeInterval)) {
        timer.stop();
        startQuiet();
    } else if (!wanted && isActive()) {
        button.setActive(false);
        timer.stop();
        titleAdd = "";
    }
}

void AutoUpdate::start() {
    if (!stateToken.empty()) {
        Utility::writePref(stateToken, "true");
    }
    const auto dataRefreshInterval = Utility::readPrefInt(prefToken, defaultValue);
    activeInterval = std::max(1, dataRefreshInterval);
    UtilityLog::d("Timer ENABLED: " + prefToken + " " + To::string(dataRefreshInterval) + " min");
    button.setActive(true);
    timer.start(dataRefreshInterval * 60 * 1000);
    localDownload();
}

void AutoUpdate::restart() {
    stop();
    start();
}

QPushButton * AutoUpdate::getView() {
    return button.getView();
}
