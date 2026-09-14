// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsToolbarOrderBox.h"
#include "util/UtilityList.h"

SettingsToolbarOrderBox::SettingsToolbarOrderBox(Window * parent, Toolbar * toolbar)
    : Widget{parent}
    , parent{parent}
    , toolbar{toolbar}
{
    addItems();
    setLayout(box.getView());
}

void SettingsToolbarOrderBox::refresh() {
    box.removeChildren();
    addItems();
}

void SettingsToolbarOrderBox::addItems() {
    buttons.clear();
    labels.clear();
    hboxList.clear();
    const auto& items = toolbar->getRouteItems();
    for (auto index : range(items.size())) {
        hboxList.emplace_back();

        buttons.emplace_back(parent, Down, "Move down");
        buttons.back().connect([this, index] { moveDownClicked(static_cast<int>(index)); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Up, "Move up");
        buttons.back().connect([this, index] { moveUpClicked(static_cast<int>(index)); });
        hboxList.back().addWidget(buttons.back());

        labels.emplace_back(parent, items[index].toolTip);
        hboxList.back().addWidget(labels.back());

        box.addLayout(hboxList.back());
    }
    box.addStretch();
}

void SettingsToolbarOrderBox::moveDownClicked(int position) {
    toolbar->moveRouteItem(position, position + 1);
    refresh();
}

void SettingsToolbarOrderBox::moveUpClicked(int position) {
    toolbar->moveRouteItem(position, position - 1);
    refresh();
}
