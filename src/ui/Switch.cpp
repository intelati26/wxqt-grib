// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Switch.h"
#include <QString>
#include "objects/WString.h"
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

Switch::Switch(Window * parent, const string& label, const string& pref, bool defaultValue)
    : pref{pref}
    , defaultValueAsString{defaultValue ? "true" : "false"}
    , checkBox{new QCheckBox{QString::fromStdString(label), parent}}
{
    checkBox->setChecked(isTrue());
    QObject::connect(checkBox, &QCheckBox::stateChanged, parent, [this] { toggle(); });
}

Switch * Switch::fromPrefBool(Window * parent, const PrefBool& prefBool) {
    return new Switch{parent, prefBool.getLabel(), prefBool.getPrefToken(), prefBool.isEnabledByDefault()};
}

bool Switch::isTrue() {
    return WString::startsWith(Utility::readPref(pref, defaultValueAsString), "t");
}

void Switch::toggle() {
    if (!isTrue()) {
        Utility::writePref(pref, "true");
    } else {
        Utility::writePref(pref, "false");
    }
    RadarPreferences::initialize();
    UIPreferences::initialize();
}

QCheckBox * Switch::getView() {
    return checkBox;
}
