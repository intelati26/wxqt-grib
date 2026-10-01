// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NHCSTORMDETAILS_H
#define NHCSTORMDETAILS_H

#include <string>
#include <vector>
#include <QByteArray>

using std::string;
using std::vector;

class NhcStormDetails {
public:
    NhcStormDetails(const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&, const string&);
    string forTopHeader() const;
    string name;
    string movementDir;
    string movementSpeed;
    string pressure;
    string binNumber;
    string stormId;
    string classification;
    string lat;
    string lon;
    string intensity;
    string status;
    string advisoryUrl;
    string center;
    string goesUrl;
    string movement;
    string modBinNumber;
    string baseUrl;
    // NHC's own graphics page for the storm (graphics_ep3.shtml, ...) lists the pictures' current addresses; they are in a
    // timestamped refresh/ folder, so they are read from the page instead of guessed
    string graphicsPageUrl;
    vector<string> graphicUrls;   // absolute, the useful pictures first (the 5-day cone is first when there is one)
    string coneUrl;               // "" = NHC publishes no cone for this storm
    QByteArray coneBytes;
    string advisoryNumber;
};

#endif  // NHCSTORMDETAILS_H
