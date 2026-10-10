// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsRadarBox.h"
#include "objects/PolygonWarning.h"
#include "ui/DividerLine.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SettingsRadarBox::SettingsRadarBox(Window * parent)
    : Widget{parent}
    , parent{parent}
{
    hboxBottom.addLayout(vbox0);
    hboxBottom.addLayout(vbox1);
    hboxBottom.addLayout(vbox2);
    box.addLayout(hboxBottom);

    for (auto type1 : PolygonWarning::polygonList) {
        auto warning = PolygonWarning::byType[type1].get();
        alertConfigs.push_back(std::make_unique<Switch>(parent, warning->name(), warning->prefTokenEnabled(), false));
    }
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Canada Borders", "RADARCANADALINES", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "County Lines", "RADAR_SHOW_COUNTY", true));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "County Labels", "RADAR_COUNTY_LABELS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Cities", "COD_CITIES_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Highways", "COD_HW_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Location Markers", "COD_LOCDOT_DEFAULT", true));
    // alertConfigs.push_back(std::make_unique<Switch>(parent, "Location marker follows GPS", "LOCDOT_FOLLOWS_GPS", false},
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Mexico Borders", "RADARMEXICOLINES", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Observations", "WXOGL_OBS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Rivers", "COD_LAKES_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Secondary Roads", "RADAR_HW_ENH_EXT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC Convective Outlook Day 1", "RADAR_SHOW_SWO", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC Fire Weather Outlook Day 1", "RADAR_SHOW_FIRE", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC MCD", "RADAR_SHOW_MCD", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Watches", "RADAR_SHOW_WATCH", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Wind Barbs", "WXOGL_OBS_WINDBARBS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "WPC Fronts", "RADAR_SHOW_WPC_FRONTS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "WPC MPD", "RADAR_SHOW_MPD", false));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Radar Text Size", "RADAR_TEXT_SIZE", 8, 2, 24, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Wind barbs line size", "RADAR_WB_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Convective outlook line size", "RADAR_SWO_LINESIZE", 20, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Warning line size", "RADAR_WARN_LINESIZE", 20, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "MCD/MPD/Watch line size", "RADAR_WATMCD_LINESIZE", 20, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "State line size", "RADAR_STATE_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "County line size", "RADAR_COUNTY_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Highway line size", "RADAR_HW_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Secondary road line size", "RADAR_HWEXT_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Lake line size", "RADAR_LAKE_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Location marker size", "RADAR_LOCDOT_SIZE", 20, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Aviation dot size", "RADAR_AVIATION_SIZE", 20, 1, 100, 1));

    for (size_t i : range(alertConfigs.size())) {
        if (i >= alertConfigs.size() / 2) {
            vbox1.addWidget(*alertConfigs[i]);
        } else {
            vbox0.addWidget(*alertConfigs[i]);
        }
    }
    vbox0.addStretch();
    vbox1.addStretch();
    for (auto i : range(numberPickers.size())) {
        vbox2.addLayout(*numberPickers[i]);
    }
    vbox2.addStretch();
    setLayout(box.getView());
}
