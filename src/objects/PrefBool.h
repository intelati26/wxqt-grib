// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PREFBOOL_H
#define PREFBOOL_H

#include <string>

using std::string;

class PrefBool {
public:
    PrefBool(const string&, const string&, bool);
    bool isEnabled() const;
    bool isEnabledByDefault() const;
    string getPrefToken() const;
    string getLabel() const;

private:
    string label;
    string prefToken;
    bool enabledByDefault;
};

#endif  // PREFBOOL_H
