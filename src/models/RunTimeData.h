// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RUNTIMEDATA_H
#define RUNTIMEDATA_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class RunTimeData {
public:
    void appendListRun(const string&);
    void appendListRunWithList(const vector<string>&);
    vector<string> listRun;
    string mostRecentRun;
    string timeStringConversion;
    string validTime;
};

#endif  // RUNTIMEDATA_H
