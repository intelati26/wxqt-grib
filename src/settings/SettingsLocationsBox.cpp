// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SettingsLocationsBox.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include "objects/WString.h"
#include "settings/Location.h"
#include "settings/ObjectLocation.h"
#include "util/UtilityList.h"

SettingsLocationsBox::SettingsLocationsBox(Window * parent)
    : Widget{parent}
    , parent{parent}
{
    addLocations();
    setLayout(box.getView());
}

void SettingsLocationsBox::refresh() {
    box.removeChildren();
    Location::refresh();
    addLocations();
    Location::setMainScreenComboBox();
}

void SettingsLocationsBox::addLocations() {
    buttons.clear();
    locationCards.clear();
    hboxList.clear();
    for (auto index : range(Location::getNumLocations())) {
        hboxList.emplace_back();

        buttons.emplace_back(parent, Down, "Move down");
        buttons.back().connect([this, index] { moveDownClicked(index); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Up, "Move up");
        buttons.back().connect([this, index] { moveUpClicked(index); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, Delete, "Delete");
        buttons.back().connect([this, index] { deleteClicked(index); });
        hboxList.back().addWidget(buttons.back());

        buttons.emplace_back(parent, None, "Edit");
        buttons.back().connect([this, index] { editClicked(index); });
        hboxList.back().addWidget(buttons.back());

        locationCards.emplace_back(parent, index);
        hboxList.back().addLayout(locationCards.back());
        box.addLayout(hboxList.back());
    }
    box.addStretch();
}

void SettingsLocationsBox::deleteClicked(int locationIndex) {
    if (Location::getNumLocations() > 1) {
        Location::deleteLocation(locationIndex);
        refresh();
    }
}

void SettingsLocationsBox::moveDownClicked(int position) {
    if (position < (Location::getNumLocations() - 1)) {
        auto locA = ObjectLocation{position};
        auto locB = ObjectLocation{position + 1};
        locA.saveToNewSlot(position + 1);
        locB.saveToNewSlot(position);
    } else {
        auto locA = ObjectLocation{position};
        auto locB = ObjectLocation{0};
        locA.saveToNewSlot(0);
        locB.saveToNewSlot(position);
    }
    refresh();
}

void SettingsLocationsBox::moveUpClicked(int position) {
    if (position > 0) {
        auto locA = ObjectLocation{position - 1};
        auto locB = ObjectLocation{position};
        locA.saveToNewSlot(position);
        locB.saveToNewSlot(position - 1);
    } else {
        auto locA = ObjectLocation{Location::getNumLocations() - 1};
        auto locB = ObjectLocation{0};
        locA.saveToNewSlot(0);
        locB.saveToNewSlot(Location::getNumLocations() - 1);
    }
    refresh();
}

// the name, the point, the radar site and the forecast office of one saved location, changed in place (a moved point looks up its office and radar again, unless they were typed over)
void SettingsLocationsBox::editClicked(int index) {
    const auto before = ObjectLocation{index};
    QDialog dialog{parent};
    dialog.setWindowTitle("Edit location");
    auto * form = new QFormLayout{&dialog};
    auto * name = new QLineEdit{QString::fromStdString(before.getName()), &dialog};
    auto * lat = new QLineEdit{QString::fromStdString(before.getLat()), &dialog};
    auto * lon = new QLineEdit{QString::fromStdString(before.getLon()), &dialog};
    auto * radar = new QLineEdit{QString::fromStdString(before.getRadarSite()), &dialog};
    auto * wfo = new QLineEdit{QString::fromStdString(before.getWfo()), &dialog};
    form->addRow("Name", name);
    form->addRow("Latitude (north +)", lat);
    form->addRow("Longitude (west -)", lon);
    form->addRow("Radar site", radar);
    form->addRow("Forecast office (WFO)", wfo);
    form->addRow(new QLabel{"Move the point and leave the radar site and office as they are: they are looked up again for the new point.", &dialog});
    auto * buttonsBox = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    form->addRow(buttonsBox);
    QObject::connect(buttonsBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonsBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    bool latOk = false, lonOk = false;
    const double latValue = lat->text().trimmed().toDouble(&latOk);
    const double lonValue = lon->text().trimmed().toDouble(&lonOk);
    if (!latOk || !lonOk || latValue < -90.0 || latValue > 90.0 || lonValue < -180.0 || lonValue > 180.0) {
        QMessageBox::warning(parent, "Edit location", "The latitude must be between -90 and 90 and the longitude between -180 and 180.");
        return;
    }
    const LatLon point{lat->text().trimmed().toStdString(), lon->text().trimmed().toStdString()};
    string radarSite = radar->text().trimmed().toStdString();
    string office = wfo->text().trimmed().toStdString();
    const bool moved = lat->text().trimmed().toStdString() != before.getLat() || lon->text().trimmed().toStdString() != before.getLon();
    const bool typedOver = radarSite != before.getRadarSite() || office != before.getWfo();
    if (radarSite.empty() || office.empty() || (moved && !typedOver)) {
        const auto found = Location::wfoAndRadarFor(point);
        office = office.empty() || (moved && !typedOver) ? found[0] : office;
        radarSite = radarSite.empty() || (moved && !typedOver) ? found[1] : radarSite;
    }
    auto label = name->text().trimmed().toStdString();
    if (label.empty()) {
        label = before.getName();
    }
    Location::update(index, point, label, office, radarSite);
    refresh();
}
