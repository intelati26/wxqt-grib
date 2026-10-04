// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsHomeScreenOrderBox.h"
#include <QAbstractItemView>
#include <QListWidget>
#include <QTimer>
#include <algorithm>
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
    addDragList("Image column (top to bottom):", "Drag to reorder, tick to show. The radar is chosen with the control above.", UIPreferences::homeScreenImageOrder);
    addDragList("Text column (top to bottom):", "Drag to reorder, tick to show.", UIPreferences::homeScreenTextOrder);
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

// one column of the home screen as a list: drag a row to move it, tick it to show it
void SettingsHomeScreenOrderBox::addDragList(const string& title, const string& note, HomeScreenOrder& order) {
    labels.emplace_back(parent, title);
    labels.back().setBlue();
    labels.back().setWordWrap(false);
    box.addWidget(labels.back());
    if (!note.empty()) {
        labels.emplace_back(parent, note);
        labels.back().setWordWrap(false);
        box.addWidget(labels.back());
    }
    auto * list = new QListWidget{this};
    list->setDragDropMode(QAbstractItemView::InternalMove);
    list->setDefaultDropAction(Qt::MoveAction);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    list->setAlternatingRowColors(true);
    list->setToolTip("Drag a row to move it; tick it to show it on the home screen");
    for (const auto& token : order.getTokens()) {
        auto * item = new QListWidgetItem{QString::fromStdString(UIPreferences::homeScreenLabel(token)), list};
        item->setData(Qt::UserRole, QString::fromStdString(token));
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled);
        if (token != UIPreferences::homeScreenNexradToken) {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(isShown(token) ? Qt::Checked : Qt::Unchecked);
        } else {
            item->setText(item->text() + "   (chosen above)");   // the radar has its own control
        }
    }
    const int rows = list->count();
    list->setFixedHeight(rows * std::max(24, list->sizeHintForRow(0)) + 10);
    const auto sync = [list, &order] {
        vector<string> next;
        for (int row = 0; row < list->count(); row += 1) {
            next.push_back(list->item(row)->data(Qt::UserRole).toString().toStdString());
        }
        order.set(next);
    };
    // a drop moves the row (or removes and inserts it): read the new order once the move is finished
    QObject::connect(list->model(), &QAbstractItemModel::rowsMoved, list, [sync] { QTimer::singleShot(0, sync); });
    QObject::connect(list->model(), &QAbstractItemModel::rowsInserted, list, [sync] { QTimer::singleShot(0, sync); });
    QObject::connect(list, &QListWidget::itemChanged, list, [] (QListWidgetItem * item) {
        if ((item->flags() & Qt::ItemIsUserCheckable) == 0) {
            return;
        }
        const auto token = item->data(Qt::UserRole).toString().toStdString();
        const bool shown = item->checkState() == Qt::Checked;
        if (isShown(token) != shown) {   // not the move itself
            Utility::writePref(token, shown ? "true" : "false");
            UIPreferences::initialize();
        }
    });
    box.addWidgetReal(list);
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
