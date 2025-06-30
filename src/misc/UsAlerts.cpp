// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UsAlerts.h"
#include <algorithm>
#include <set>
#include "objects/FutureVoid.h"
#include "objects/FutureBytes.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityDownloadNws.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

using std::set;

UsAlerts::UsAlerts(Window * parent)
    : Window{parent}
    , sw{this, box}
    , photo{this, Normal}
    , comboBox{this}
{
    setTitle("US Alerts");
    comboBox.connect([this] { filterEvents(); });
    box.addWidget(comboBox);
    box.addWidgetAndCenter(photo);
    box.addLayout(boxText);
    new FutureBytes{this, "https://forecast.weather.gov/wwamap/png/US.png", [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureVoid{this, [this] { html = UtilityDownloadNws::getCap("us"); }, [this] { update(); }};
}

void UsAlerts::update() {
    html = WString::replace(html, "\n", "ABC123");
    const auto alerts = UtilityString::parseColumn(html, "<entry>(.*?)</entry>");
    vector<string> rawEvents;
    for (const auto& alert : alerts) {
        capAlerts.emplace_back(alert);
    }
    for (const auto& cap : capAlerts) {
        rawEvents.push_back(cap.event);
        if (filterCount.contains(cap.event)) {
            filterCount[cap.event] += 1;
        } else {
            filterCount[cap.event] = 1;
        }
        if (contains(defaultFilter, cap.event)) {
            alertCards.emplace_back(this, cap);
            boxText.addLayout(alertCards.back());
        }
    }

    boxText.addStretch();

    const set<string> uniqEventsSet{rawEvents.begin(), rawEvents.end()};
    vector<string> uniqEvents{uniqEventsSet.begin(), uniqEventsSet.end()};
    sort(uniqEvents.begin(), uniqEvents.end());
    vector<string> uniqEventsWithCount;
    for (const auto& item : uniqEvents) {
        uniqEventsWithCount.push_back(item + ": " + To::string(UtilityList::count(rawEvents, item)));
    }
    eventList.emplace_back("Tor/Ffw/Tst");
    addAll(eventList, uniqEventsWithCount);
    setTitle("US Alerts: " + To::string(static_cast<int>(capAlerts.size())));
    comboBox.setList(eventList);
    comboBox.setIndex(0);
}

void UsAlerts::filterEvents() {
    const auto index = comboBox.getIndex();
    boxText.removeChildren();
    alertCards.clear();
    auto filter = WString::split(eventList[index], ":")[0];
    if (index == 0) {
        filter = ":" + WString::join(defaultFilter, ":") + ":";
        for (const auto& cap : capAlerts) {
            if (WString::contains(filter, ":" + cap.event + ":")) {
                alertCards.emplace_back(this, cap);
                boxText.addLayout(alertCards.back());
            }
        }
    } else {
        for (const auto& cap : capAlerts) {
            if (cap.event == filter) {
                alertCards.emplace_back(this, cap);
                boxText.addLayout(alertCards.back());
            }
        }
    }
    boxText.addStretch();
}
