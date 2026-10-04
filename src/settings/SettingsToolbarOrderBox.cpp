// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsToolbarOrderBox.h"
#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <QAbstractItemView>
#include <QListWidget>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include "ui/ToolbarGroups.h"
#include "util/UtilityList.h"

namespace {
    // the order a drag left a list in, as the numbers the rows carried before it (their Qt::UserRole)
    vector<int> wantedOrder(const QListWidget * list) {
        vector<int> wanted;
        for (int row = 0; row < list->count(); row += 1) {
            wanted.push_back(list->item(row)->data(Qt::UserRole).toInt());
        }
        return wanted;
    }

    // turns the old order (0, 1, 2 ...) into the wanted one with neighbour swaps, which is all the toolbar and the
    // groups can do (a swap of neighbours moves one row past the rest, as the drag did)
    void realize(const vector<int>& wanted, const std::function<void(int, int)>& swapNeighbours) {
        vector<int> current(wanted.size());
        std::iota(current.begin(), current.end(), 0);
        for (int target = 0; target < static_cast<int>(wanted.size()); target += 1) {
            int at = static_cast<int>(std::find(current.begin(), current.end(), wanted[static_cast<size_t>(target)]) - current.begin());
            while (at > target) {
                swapNeighbours(at, at - 1);
                std::swap(current[static_cast<size_t>(at)], current[static_cast<size_t>(at) - 1]);
                at -= 1;
            }
        }
    }

    // a drop can announce itself twice (rows moved, rows inserted): run the follow-up once, after the drop has finished
    void onDrop(QListWidget * list, const std::function<void()>& follow) {
        const auto pending = std::make_shared<bool>(false);
        const auto schedule = [pending, follow] {
            if (!*pending) {
                *pending = true;
                QTimer::singleShot(0, [pending, follow] {
                    *pending = false;
                    follow();
                });
            }
        };
        QObject::connect(list->model(), &QAbstractItemModel::rowsMoved, list, schedule);
        QObject::connect(list->model(), &QAbstractItemModel::rowsInserted, list, schedule);
    }

    void renumber(QListWidget * list) {
        for (int row = 0; row < list->count(); row += 1) {
            list->item(row)->setData(Qt::UserRole, row);
        }
    }

    QListWidget * dragList(QWidget * parent, int rows) {
        auto * list = new QListWidget{parent};
        list->setDragDropMode(QAbstractItemView::InternalMove);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setAlternatingRowColors(true);
        (void) rows;
        return list;
    }

    void fitHeight(QListWidget * list) {
        list->setFixedHeight(list->count() * std::max(24, list->sizeHintForRow(0)) + 10);
    }
}

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
    auto * entryList = dragList(this, static_cast<int>(items.size()));
    const auto groupNames = [] {
        vector<string> names;
        for (const auto& group : ToolbarGroups::groups()) {
            names.push_back(group.name);
        }
        return names;
    };
    const auto textOf = [] (const RouteItem& item) {
        const auto group = ToolbarGroups::groupOf(item.id);
        const auto& groups = ToolbarGroups::groups();
        return QString::fromStdString(item.toolTip) + (group >= 0 && group < static_cast<int>(groups.size()) ? "   [" + QString::fromStdString(groups[static_cast<size_t>(group)].name) + "]" : QString{});
    };
    for (auto index : range(items.size())) {
        auto * item = new QListWidgetItem{textOf(items[index]), entryList};
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
    }
    fitHeight(entryList);
    entryList->setToolTip("Drag an entry to move it; right-click it to choose its group");
    const QPointer<QListWidget> entryGuard{entryList};
    onDrop(entryList, [this, entryGuard] {
        if (entryGuard.isNull()) {
            return;
        }
        realize(wantedOrder(entryGuard), [this] (int from, int to) { toolbar->moveRouteItem(from, to); });
        renumber(entryGuard);
    });
    // right-click: the group this entry is listed under in the icons + names toolbar and the menu bar
    entryList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(entryList, &QListWidget::customContextMenuRequested, entryList, [this, entryList, groupNames, textOf] (const QPoint& where) {
        auto * item = entryList->itemAt(where);
        if (item == nullptr) {
            return;
        }
        const auto row = static_cast<size_t>(entryList->row(item));
        const auto& routes = toolbar->getRouteItems();
        if (row >= routes.size()) {
            return;
        }
        const auto id = routes[row].id;
        QMenu menu;
        const auto names = groupNames();
        for (size_t group = 0; group < names.size(); group += 1) {
            auto * action = menu.addAction(QString::fromStdString(names[group]));
            action->setCheckable(true);
            action->setChecked(static_cast<int>(group) == ToolbarGroups::groupOf(id));
        }
        if (const auto * chosen = menu.exec(entryList->viewport()->mapToGlobal(where))) {
            const auto group = static_cast<int>(menu.actions().indexOf(chosen));
            ToolbarGroups::moveItem(id, group);
            toolbar->rebuild();
            item->setText(textOf(toolbar->getRouteItems()[row]));
        }
    });
    box.addWidgetReal(entryList);
    box.addStretch();
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

    labels.emplace_back(parent, "Groups (the headings of the icons + names toolbar and the menus of the menu bar; drag to reorder):");
    labels.back().setBlue();
    box.addWidget(labels.back());
    auto * groupList = dragList(this, static_cast<int>(ToolbarGroups::groups().size()));
    for (const auto& group : ToolbarGroups::groups()) {
        auto * item = new QListWidgetItem{QString::fromStdString(group.name), groupList};
        item->setData(Qt::UserRole, groupList->count() - 1);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEditable);   // double-click to rename
    }
    fitHeight(groupList);
    groupList->setToolTip("Drag a group to move it; double-click it to rename it");
    const QPointer<QListWidget> groupGuard{groupList};
    onDrop(groupList, [this, groupGuard] {
        if (groupGuard.isNull()) {
            return;
        }
        realize(wantedOrder(groupGuard), [] (int from, int to) { ToolbarGroups::moveGroup(from, to); });
        renumber(groupGuard);
        toolbar->rebuild();
        QTimer::singleShot(0, parent, [this] { refresh(); });   // the entries show their group's name
    });
    QObject::connect(groupList, &QListWidget::itemChanged, groupList, [this, groupList] (QListWidgetItem * item) {
        const auto row = groupList->row(item);
        const auto name = item->text().toStdString();
        if (row >= 0 && row < static_cast<int>(ToolbarGroups::groups().size()) && ToolbarGroups::groups()[static_cast<size_t>(row)].name != name) {
            ToolbarGroups::renameGroup(row, name);
            toolbar->rebuild();
            QTimer::singleShot(0, parent, [this] { refresh(); });
        }
    });
    box.addWidgetReal(groupList);
    hboxList.emplace_back();
    buttons.emplace_back(parent, None, "Add a group");
    buttons.back().setText("Add group");
    buttons.back().connect([this] { ToolbarGroups::addGroup("New group"); changed(); });
    hboxList.back().addWidget(buttons.back());
    buttons.emplace_back(parent, None, "Delete the selected group (its entries move to a neighbouring group)");
    buttons.back().setText("Delete group");
    buttons.back().connect([this, groupList] {
        if (groupList->currentRow() >= 0) {
            ToolbarGroups::deleteGroup(groupList->currentRow());
            changed();
        }
    });
    hboxList.back().addWidget(buttons.back());
    buttons.emplace_back(parent, None, "Reset groups to the built-in grouping");
    buttons.back().setText("Reset groups");
    buttons.back().connect([this] { ToolbarGroups::reset(); changed(); });
    hboxList.back().addWidget(buttons.back());
    hboxList.back().addStretch();
    box.addLayout(hboxList.back());

    labels.emplace_back(parent, "Entries (drag to reorder; right-click for the group each is listed under):");
    labels.back().setBlue();
    box.addWidget(labels.back());
}
