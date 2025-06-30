// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SEVERENOTICE_H
#define SEVERENOTICE_H

#include <string>
#include <vector>
#include "radar/PolygonType.h"

using std::string;
using std::vector;

class SevereNotice {
public:
    explicit SevereNotice(PolygonType);
    void getBitmaps();
    string getShortName() const;
    string getCount() const;
    int getCountAsInt() const;
    vector<string> urls;

private:
    PolygonType type;
};

#endif  // SEVERENOTICE_H
