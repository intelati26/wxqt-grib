// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTMODEL_H
#define OBJECTMODEL_H

#include <string>
#include <vector>
#include "RunTimeData.h"

using std::string;
using std::vector;

class ObjectModel {
public:
    explicit ObjectModel(const string&);
    void getPrefs();
    void writePrefs() const;
    void loadTimeList(int, int, int);
    void loadTimeList3(int, int, int);
    void loadRunList(int, int, int);
    void setModelVars(const string&);
    void setupListRunZ();
    void setupListRunZWithStart(const string&);
    string getTime() const;
    void setTimeIdx(int);
    void leftClick();
    void rightClick();
    void timeIdxIncr();
    void timeIdxDecr();
    void setTimeArr(int, const string&);
    vector<string> params;
    vector<string> paramLabels;
    vector<string> sectors;
    vector<string> times;
    vector<string> runs;
    vector<string> models;
    RunTimeData runTimeData;
    string modelToken;
    string prefModel;
    string prefSector;
    string prefParam;
    string prefRunPosn;
    string prefRunPosnIdx;
    string model;
    string run;
    string timeStr;
    size_t timeIdx{};
    string param;
    string sector;
};

#endif  // OBJECTMODEL_H
