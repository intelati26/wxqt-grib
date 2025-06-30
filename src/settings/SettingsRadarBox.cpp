// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "settings/SettingsRadarBox.h"
#include "common/GlobalVariables.h"
#include "misc/TextViewerStatic.h"
#include "objects/PolygonWarning.h"
#include "radarcolorpalette/ColorPalette.h"
#include "ui/DividerLine.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SettingsRadarBox::SettingsRadarBox(Window * parent)
    : Widget{parent}
    , parent{parent}
    , button{parent, None, "Keyboard Shortcuts"}
    , text1{parent, "Reflectivity Palette:"}
    , text2{parent, "Velocity Palette:"}
    , comboBoxRefPal{parent, refPalChoices}
    , comboBoxVelPal{parent, velPalChoices}
{
    button.connect([this] { launchShortcuts(); });
    hbox0.addWidget(button);
    hbox.addLayout(hbox0);

    comboBoxRefPal.setIndex(findex(Utility::readPref("RADAR_COLOR_PALETTE_94", "CODENH"), refPalChoices));
    comboBoxRefPal.connect([this] { changeRefPal(); });
    hbox1.setSpacing(10);
    hbox1.addWidget(text1);
    hbox1.addWidget(comboBoxRefPal);
    hbox.addLayout(hbox1);

    comboBoxVelPal.setIndex(findex(Utility::readPref("RADAR_COLOR_PALETTE_99", "CODENH"), velPalChoices));
    comboBoxVelPal.connect([this] { changeVelPal(); });
    hbox2.addWidget(text2);
    hbox2.addWidget(comboBoxVelPal);
    hbox.addLayout(hbox2);
    box.addLayout(hbox);
    auto d = DividerLine(parent);
    box.addWidget(d);
    hboxBottom.addLayout(vbox0);
    hboxBottom.addLayout(vbox1);
    hboxBottom.addLayout(vbox2);
    box.addLayout(hboxBottom);

    for (auto type1 : PolygonWarning::polygonList) {
        auto warning = PolygonWarning::byType[type1].get();
        alertConfigs.push_back(std::make_unique<Switch>(parent, warning->name(), warning->prefTokenEnabled(), false));
    }
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Canada Borders", "RADARCANADALINES", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Colormap Legend", "RADAR_COLOR_LEGEND", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Controls", "RADAR_SHOW_CONTROLS", true));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "County Lines", "RADAR_SHOW_COUNTY", true));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "County Labels", "RADAR_COUNTY_LABELS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Cities", "COD_CITIES_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Hail Indicators", "RADAR_SHOW_HI", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Highways", "COD_HW_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Location Markers", "COD_LOCDOT_DEFAULT", true));
    // alertConfigs.push_back(std::make_unique<Switch>(parent, "Location marker follows GPS", "LOCDOT_FOLLOWS_GPS", false},
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Mexico Borders", "RADARMEXICOLINES", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Multi-pane: share position && radar site", "DUALPANE_SHARE_POSN", true));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Observations", "WXOGL_OBS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Remember location, site, and product", "WXOGL_REMEMBER_LOCATION", true));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Rivers", "COD_LAKES_DEFAULT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Secondary Roads", "RADAR_HW_ENH_EXT", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC Convective Outlook Day 1", "RADAR_SHOW_SWO", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC Fire Weather Outlook Day 1", "RADAR_SHOW_FIRE", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "SPC MCD", "RADAR_SHOW_MCD", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Storm Tracks", "RADAR_SHOW_STI", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Tornado Vortex Signature", "RADAR_SHOW_TVS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Watches", "RADAR_SHOW_WATCH", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "Wind Barbs", "WXOGL_OBS_WINDBARBS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "WPC Fronts", "RADAR_SHOW_WPC_FRONTS", false));
    alertConfigs.push_back(std::make_unique<Switch>(parent, "WPC MPD", "RADAR_SHOW_MPD", false));

    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Nexrad refresh interval (min)", "RADAR_DATA_REFRESH_INTERVAL", 3, 1, 60, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Radar Text Size", "RADAR_TEXT_SIZE", 8, 2, 24, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Wind barbs line size", "RADAR_WB_LINESIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Storm tracks line size", "RADAR_STI_LINESIZE", 10, 1, 100, 1));
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
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "Hail marker size", "RADAR_HI_SIZE", 10, 1, 100, 1));
    numberPickers.push_back(std::make_unique<NumberPicker>(parent, "TVS marker size", "RADAR_TVS_SIZE", 10, 1, 100, 1));

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

void SettingsRadarBox::changeRefPal() {
    Utility::writePref("RADAR_COLOR_PALETTE_94", comboBoxRefPal.getValue());
    ColorPalette::loadColorMap(94);
}

void SettingsRadarBox::changeVelPal() {
    Utility::writePref("RADAR_COLOR_PALETTE_99", comboBoxVelPal.getValue());
    ColorPalette::loadColorMap(99);
}

void SettingsRadarBox::launchShortcuts() {
    new TextViewerStatic{parent, GlobalVariables::nexradShortcuts, "Shortcuts", 500, 500};
}
