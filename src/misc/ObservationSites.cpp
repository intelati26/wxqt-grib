// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "misc/ObservationSites.h"
#include <QDesktopServices>
#include <QUrl>
#include "objects/WString.h"
#include "settings/Location.h"

ObservationSites::ObservationSites(Window * parent)
    : Window{parent}
    , button1{this, None, Location::wfo() + ": nearby current observations"}
    , button2{this, None, Location::getObs() + ": recent observations"}
    , layers{"&tab=layers&obs=true&obs_type=weather&elements=temp,wind,gust&temp_filter=-80,130&gust_filter=0,150&rh_filter=0,100&elev_filter=-300,14000&precip_filter=0.01,30&obs_popup=false&obs_density=60&obs_provider=ALL"}
    , url1{"https://www.wrh.noaa.gov/map/?&zoom=12&center=" + Location::getLatLonCurrent().latStr() + "," + Location::getLatLonCurrent().lonStr() + layers}
    , url2{"https://www.weather.gov/wrh/timeseries?site=F8857&hours=72&chart=off"}
    , webViewer1{this, url1}
    , webViewer2{this, url2}
{
    setTitle("Observation web sites - " + Location::name());
    const auto url = "https://www.wrh.noaa.gov/map/?obs=true&wfo=" + WString::toLower(Location::wfo());
    const auto url2 = "https://www.weather.gov/wrh/timeseries?site=" + Location::getObs();
    button1.connect([url] { launchAlert(url); });
    button2.connect([url2] { launchAlert(url2); });

    box.addWidget(button1);
    box.addWidget(button2);
    box.addLayout(boxH);
    boxH.addWidget(webViewer1);
    boxH.addWidget(webViewer2);
    box.addStretch();

    webViewer1.getView()->setFixedHeight(500);
    webViewer2.getView()->setFixedHeight(500);

    webViewer1.getView()->setFixedWidth(500);
    webViewer2.getView()->setFixedWidth(500);

    box.getAndShow(this);
}

void ObservationSites::launchAlert(const string& url) {
    QDesktopServices::openUrl(QUrl{ QString::fromStdString(url) });
}
