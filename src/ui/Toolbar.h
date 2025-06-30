// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <functional>
#include <vector>
#include "objects/AutoUpdate.h"
#include "ui/ButtonFlat.h"
#include "ui/ComboBox.h"
#include "ui/RouteItem.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::function;
using std::vector;

class Toolbar : public VBox {
public:
    Toolbar(Window *, const function<void()>&);
    void launchSettings();
    void launchNexrad(int);
    void launchHourly();
    void launchWfoText();
    void launchSpcSwoSummary();
    void launchGoesViewer();
    void launchSpcSwoDay1(int);
    void launchNationalText();
    void launchSpcTstormOutlooks();
    void launchSpcCompmap();
    void launchSevereDashboard();
    void launchNhc();
    void launchRadarMosaicViewer();
    void launchLightning();
    void launchObservationSites();
    void launchObservations();
    void launchSpcMeso(const string& = "");
    void launchModelViewer();
    void launchModelViewerGeneric(const string&);
    void launchNationalImages();
    void launchUsAlerts();
    void launchRainfallOutlookSummary();
    void launchSpcFireWeatherOutlookSummary();
    void launchSpcStormReports(const string&);
    void launchOpc();
    void launchRtma();
    void refresh();

private:
    Window * parent;
    function<void()> reloadFn;
    vector<RouteItem> routeItems;
    vector<ButtonFlat> buttons;

public:
    AutoUpdate autoUpdate;
};

#endif  // TOOLBAR_H
