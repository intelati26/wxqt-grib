// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsToolbarOrderBox.h"
#include <QLineEdit>
#include "ui/ToolbarGroups.h"
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
    combos.clear();
    entries.clear();
    addStyleAndGroups();
    const auto& items = toolbar->getRouteItems();
    vector<string> groupNames;
    for (const auto& group : ToolbarGroups::groups()) {
        groupNames.push_back(group.name);
    }
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

        // the group this entry is listed under in the icons + names toolbar and the menu bar
        combos.emplace_back(parent, groupNames);
        combos.back().setIndex(static_cast<size_t>(ToolbarGroups::groupOf(items[index].id)));
        const auto id = items[index].id;
        combos.back().connect([this, id, position = combos.size() - 1] {
            ToolbarGroups::moveItem(id, combos[position].getIndex());
            toolbar->rebuild();
        });
        hboxList.back().addWidget(combos.back());

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

void SettingsToolbarOrderBox::changed() {
    toolbar->rebuild();
    refresh();
}

// the toolbar style, and the editable groups the icons + names toolbar and the menu bar are sorted into
void SettingsToolbarOrderBox::addStyleAndGroups() {
    labels.emplace_back(parent, "Toolbar style:");
    labels.back().setBlue();
    box.addWidget(labels.back());
    combos.emplace_back(parent, ToolbarGroups::modeLabels());
    combos.back().setIndex(static_cast<size_t>(ToolbarGroups::mode()));
    combos.back().connect([this, position = combos.size() - 1] {
        ToolbarGroups::setMode(combos[position].getIndex());
        toolbar->rebuild();
    });
    box.addWidget(combos.back());

    labels.emplace_back(parent, "Groups (the headings of the icons + names toolbar and the menus of the menu bar):");
    labels.back().setBlue();
    box.addWidget(labels.back());
    const auto groupCount = static_cast<int>(ToolbarGroups::groups().size());
    for (int index = 0; index < groupCount; index += 1) {
        hboxList.emplace_back();
        buttons.emplace_back(parent, Down, "Move group down");
        buttons.back().connect([this, index] { ToolbarGroups::moveGroup(index, index + 1); changed(); });
        hboxList.back().addWidget(buttons.back());
        buttons.emplace_back(parent, Up, "Move group up");
        buttons.back().connect([this, index] { ToolbarGroups::moveGroup(index, index - 1); changed(); });
        hboxList.back().addWidget(buttons.back());
        entries.emplace_back(parent);
        entries.back().setText(ToolbarGroups::groups()[index].name);
        // renamed when editing is finished (Enter or leaving the box), not on every keystroke
        QObject::connect(entries.back().getView(), &QLineEdit::editingFinished, parent, [this, index, position = entries.size() - 1] {
            if (entries[position].getText() != ToolbarGroups::groups()[index].name) {
                ToolbarGroups::renameGroup(index, entries[position].getText());
                changed();
            }
        });
        hboxList.back().addWidget(entries.back());
        buttons.emplace_back(parent, Delete, "Delete group (its entries move to a neighbouring group)");
        buttons.back().connect([this, index] { ToolbarGroups::deleteGroup(index); changed(); });
        hboxList.back().addWidget(buttons.back());
        box.addLayout(hboxList.back());
    }
    hboxList.emplace_back();
    buttons.emplace_back(parent, Plus, "Add group");
    buttons.back().connect([this] { ToolbarGroups::addGroup("New group"); changed(); });
    hboxList.back().addWidget(buttons.back());
    labels.emplace_back(parent, "Add group");
    hboxList.back().addWidget(labels.back());
    buttons.emplace_back(parent, None, "Reset groups to the built-in grouping");
    buttons.back().setText("Reset groups");
    buttons.back().connect([this] { ToolbarGroups::reset(); changed(); });
    hboxList.back().addWidget(buttons.back());
    box.addLayout(hboxList.back());

    labels.emplace_back(parent, "Entries (order, and the group each is listed under):");
    labels.back().setBlue();
    box.addWidget(labels.back());
}
