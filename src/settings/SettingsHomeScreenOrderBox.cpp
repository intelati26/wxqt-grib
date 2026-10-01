// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsHomeScreenOrderBox.h"
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
    addSection("Sections (forecast and text share a row when the window is wide enough; thumbnails go below only if listed after the forecast):", "", UIPreferences::homeScreenColumnOrder);
    addSection("Image column (top to bottom):", "Show or hide these under General (Nexrad: \"Show Nexrad on main screen\").", UIPreferences::homeScreenImageOrder);
    addSection("Text column (top to bottom):", "", UIPreferences::homeScreenTextOrder);
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
