// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsHomeScreenOrderBox.h"
#include <QTimer>
#include "settings/HomeLayoutEditor.h"
#include "util/Utility.h"
#include "util/UtilityList.h"

SettingsHomeScreenOrderBox::SettingsHomeScreenOrderBox(Window * parent)
    : Widget{parent}
    , parent{parent}
{
    addItems();
    setLayout(box.getView());
}

void SettingsHomeScreenOrderBox::refresh() {
    box.removeChildren();
    addItems();
}

void SettingsHomeScreenOrderBox::addItems() {
    buttons.clear();
    labels.clear();
    hboxList.clear();
    combos.clear();
    // one control for the home screen's radar picture: the live Nexrad tile, a still MRMS reflectivity picture, or none
    hboxList.emplace_back();
    labels.emplace_back(parent, "Radar on the home screen:");
    labels.back().setWordWrap(false);
    hboxList.back().addWidget(labels.back());
    combos.emplace_back(parent, vector<string>{"Live radar (Nexrad)", "MRMS radar picture (still, around your location)", "None"});
    combos.back().getView()->setToolTip("The live Nexrad tile downloads and decodes radar data in the background; the MRMS picture is a single image");
    const bool live = Utility::readPref("NEXRAD_ON_MAIN_SCREEN", "false").rfind("t", 0) == 0;
    const bool still = Utility::readPref("MRMS_RADAR", "false").rfind("t", 0) == 0;
    combos.back().setIndex(live ? 0 : (still ? 1 : 2));
    const auto * radarCombo = &combos.back();
    combos.back().connect([this, radarCombo] {
        const int choice = radarCombo->getIndex();
        Utility::writePref("NEXRAD_ON_MAIN_SCREEN", choice == 0 ? "true" : "false");
        Utility::writePref("MRMS_RADAR", choice == 1 ? "true" : "false");
        UIPreferences::initialize();
        QTimer::singleShot(0, this, [this] { refresh(); });   // after this handler returns: the lists below show which items are hidden
    });
    hboxList.back().addWidget(combos.back());
    box.addLayout(hboxList.back());
    labels.emplace_back(parent, "Layout - pick a layout, then drag the sections into its zones (or click a section for a menu):");
    labels.back().setBlue();
    labels.back().setWordWrap(false);
    box.addWidget(labels.back());
    box.addWidgetReal(new HomeLayoutEditor{this, [] {}});
    addSection("Image column (top to bottom):", "Show or hide these under General (Nexrad: \"Show Nexrad on main screen\").", UIPreferences::homeScreenImageOrder);
    addSection("Text column (top to bottom):", "", UIPreferences::homeScreenTextOrder);
    // the MRMS home thumbnail: the area around the current location, or all of the lower 48
    hboxList.emplace_back();
    labels.emplace_back(parent, "MRMS thumbnail area:");
    labels.back().setWordWrap(false);
    hboxList.back().addWidget(labels.back());
    combos.emplace_back(parent, vector<string>{"Around my location", "All of CONUS"});
    combos.back().setIndex(Utility::readPref("MRMS_THUMB_EXTENT", "regional") == "conus" ? 1 : 0);
    combos.back().connect([this] { Utility::writePref("MRMS_THUMB_EXTENT", combos.back().getIndex() == 1 ? "conus" : "regional"); });
    hboxList.back().addWidget(combos.back());
    box.addLayout(hboxList.back());
    labels.emplace_back(parent, "Changes show on the main screen when Settings is closed.");
    labels.back().setWordWrap(false);
    box.addWidget(labels.back());
    box.addStretch();
}

void SettingsHomeScreenOrderBox::addSection(const string& title, const string& note, HomeScreenOrder& order) {
    labels.emplace_back(parent, title);
    labels.back().setBlue();
    labels.back().setWordWrap(false);
    box.addWidget(labels.back());
    if (!note.empty()) {
        labels.emplace_back(parent, note);
        labels.back().setWordWrap(false);
        box.addWidget(labels.back());
    }
    const auto& tokens = order.getTokens();
    for (auto index : range(tokens.size())) {
        const auto position = static_cast<int>(index);
        hboxList.emplace_back();

        buttons.emplace_back(parent, Down, "Move down");
        buttons.back().connect([this, &order, position] { order.move(position, position + 1); refresh(); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Up, "Move up");
        buttons.back().connect([this, &order, position] { order.move(position, position - 1); refresh(); });
        hboxList.back().addWidget(buttons.back());

        auto label = UIPreferences::homeScreenLabel(tokens[index]);
        if (!isShown(tokens[index])) {
            label += "  (hidden)";
        }
        labels.emplace_back(parent, label);
        labels.back().setWordWrap(false);
        hboxList.back().addWidget(labels.back());

        box.addLayout(hboxList.back());
    }
}

bool SettingsHomeScreenOrderBox::isShown(const string& token) {
    if (token == UIPreferences::homeScreenNexradToken) {
        return UIPreferences::nexradMainScreen;
    }
    for (const auto& items : {&UIPreferences::homeScreenItemsImage, &UIPreferences::homeScreenItemsText}) {
        for (const auto& item : *items) {
            if (item.getPrefToken() == token) {
                return item.isEnabled();
            }
        }
    }
    return true;   // columns
}
