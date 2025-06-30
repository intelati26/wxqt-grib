// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SEVEREWARNING_H
#define SEVEREWARNING_H

#include <string>
#include <vector>
#include "objects/ObjectWarning.h"
#include "radar/PolygonType.h"

using std::string;
using std::vector;

class SevereWarning {
public:
    explicit SevereWarning(PolygonType);
    void download();
    void generateString();
    string getName() const;
    string getShortName() const;
    string getCount() const;
    int getCountAsInt() const;
    vector<ObjectWarning> warningList;

private:
    PolygonType type;
};

#endif  // SEVEREWARNING_H
