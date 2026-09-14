// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef GLOBALVARIABLES_H
#define GLOBALVARIABLES_H

#include <string>

using std::string;

class GlobalVariables {
public:
    static const string aboutString;
    static const string mainScreenShortcuts;
    static const string nexradShortcuts;
    // QSettings organization key - must stay stable across builds/forks:
    // changing it points every future run at a different settings file/
    // registry key, silently orphaning whatever a user already has saved
    // (locations, toggles, etc.) under the old one. Deliberately a neutral
    // project name, not a person's email - see [[wxqt-privacy-hygiene]] /
    // Settings > General's "Contact email" field for the actual, optional,
    // user-editable contact address sent to weather APIs.
    static const string appOrgName;
    static const string appName;
    static const string resDir;
    static const string imageDir;
    static const string newline;
    static const string degreeSymbol;
    static const string nwsSPCwebsitePrefix;
    static const string nwsWPCwebsitePrefix;
    static const string nwsAWCwebsitePrefix;
    static const string nwsGraphicalWebsitePrefix;
    static const string nwsOpcWebsitePrefix;
    static const string nwsNhcWebsitePrefix;
    static const string goes16Url;
    static const string nwsApiUrl;
    static const string tgftpSitePrefix;
    static const string prePattern;
    static const string pre2Pattern;
    static const string vtecPattern;
    static const string warningLatLonPattern;
    static const string utilUsPeriodNamePattern;
    static const string xmlValuePattern;
};

#endif  // GLOBALVARIABLES_H
