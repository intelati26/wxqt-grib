// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NHCSTORMDETAILS_H
#define NHCSTORMDETAILS_H

#include <string>
#include <QByteArray>

using std::string;

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
    QByteArray coneBytes;
    string advisoryNumber;
};

#endif  // NHCSTORMDETAILS_H
