// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "LocationEditBox.h"
#include <QDesktopServices>
#include <QUrl>
#include "common/GlobalVariables.h"
#include "objects/LatLon.h"
#include "objects/WString.h"
#include "radar/RadarSites.h"
#include "settings/Location.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

LocationEditBox::LocationEditBox(Window * parent)
    : Widget{parent}
    , table{nullptr}
    , saveButton{parent, None, "Save"}
    , mapButton{parent, None, "Show on Map"}
    , cityEdit{parent}
    , editName{parent}
    , editLat{parent}
    , editLon{parent}
    , editNexrad{parent}
    , cities{UtilityIO::rawFileToStringArray(GlobalVariables::resDir + "cityall.txt")}
{
    cityEdit.connect([this] { lookupSearchTerm(); });
    saveButton.connect([this] { saveLocation(); });
    mapButton.connect([this] { showOnMap(); });

    table.addRow("Enter City:", cityEdit);
    table.addRow("Name", editName);
    table.addRow("Latitude", editLat);
    table.addRow("Longitude", editLon);
    table.addRow("Nexrad", editNexrad);
    table.addRow("", saveButton);
    table.addRow("", mapButton);
    boxMain.addLayout(table);

    for (auto index : range(6)) {
        buttons.emplace_back(parent, None, "");
        boxResults.addWidget(buttons[index]);
        buttons[index].connect([this, index] { populateLabels(index); });
    }
    boxResults.addStretch();
    boxMain.addLayout(boxResults);
    setLayout(boxMain.getView());
    blankOutButtons();
}

void LocationEditBox::lookupSearchTerm() {
    const auto text = cityEdit.getText();
    editLat.setText(text);
    if (text.size() > 2) {
        vector<string> citiesSelected;
        for (auto& city : cities) {
            if (WString::startsWith(WString::toLower(city), text)) {
                citiesSelected.push_back(city + " Radar: " + getRadarFromCity(city));
            }
        }
        for (size_t index : range(buttons.size())) {
            if (index < citiesSelected.size()) {
                buttons[index].setText(citiesSelected[index]);
                buttons[index].setVisible(true);
            } else {
                buttons[index].setText("");
                buttons[index].setVisible(false);
            }
        }
        if (!citiesSelected.empty()) {
            populateLabels(0);
        }
    } else {
        blankOutButtons();
    }
}

string LocationEditBox::getRadarFromCity(const string& s) {
    auto tokens = WString::split(s, ",");
    if (tokens.size() < 3) {
        return "";
    }
    auto latLon = LatLon{tokens[1], tokens[2]};
    return RadarSites::getNearestCode(latLon, false);
}

void LocationEditBox::populateLabels(int index) {
    auto city = buttons[index].getText();
    const auto radarSuffix = city.find(" Radar: ");
    if (radarSuffix != string::npos) {
        city = city.substr(0, radarSuffix);
    }
    const auto tokens = WString::split(city, ",");
    if (tokens.size() < 3) {
        return;
    }
    editName.setText(tokens[0]);
    editLat.setText(tokens[1]);
    editLon.setText(tokens[2]);
    editNexrad.setText(getRadarFromCity(city));
}

void LocationEditBox::blankOutButtons() {
    editName.setText("");
    editLat.setText("");
    editLon.setText("");
    editNexrad.setText("");
    for (auto index : range(buttons.size())) {
        buttons[index].setText("");
        buttons[index].setVisible(false);
    }
}

void LocationEditBox::showOnMap() {
    const auto lat = editLat.getText();
    const auto lon = editLon.getText();
    if (lat.empty() || lon.empty()) {
        return;
    }
    const auto url = "https://www.openstreetmap.org/?mlat=" + lat + "&mlon=" + lon +
        "#map=9/" + lat + "/" + lon;
    QDesktopServices::openUrl(QUrl{QString::fromStdString(url)});
}

void LocationEditBox::saveLocation() {
    const auto latLon = LatLon{editLat.getText(), editLon.getText()};
    const auto nameToSave = editName.getText();
    Location::save(latLon, nameToSave);
    Location::setMainScreenComboBox();
    blankOutButtons();
    cityEdit.setText("");
}
