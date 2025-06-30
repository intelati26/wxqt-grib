// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RunTimeData.h"
#include "util/UtilityList.h"

void RunTimeData::appendListRun(const string& value) {
    listRun.push_back(value);
}

void RunTimeData::appendListRunWithList(const vector<string>& values) {
    addAll(listRun, values);
}
