// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "PrefBool.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/Utility.h"

PrefBool::PrefBool(const string& label, const string& prefToken, bool enabledByDefault)
    : label{label}
    , prefToken{prefToken}
    , enabledByDefault{enabledByDefault}
{}

bool PrefBool::isEnabled() const {
    return WString::startsWith(Utility::readPref(prefToken, To::string(enabledByDefault)), "t");
}

bool PrefBool::isEnabledByDefault() const {
    return enabledByDefault;
}

string PrefBool::getPrefToken() const {
    return prefToken;
}

string PrefBool::getLabel() const {
    return label;
}
