// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYMODELNCEPINTERFACE_H
#define UTILITYMODELNCEPINTERFACE_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityModelNcepInterface {
public:
    static const vector<string> models;
    static const vector<string> sectorsGfs;
    static const vector<string> sectorsNam;
    static const vector<string> sectorsNamHires;
    static const vector<string> sectorsRap;
    static const vector<string> sectorsHrrr;
    static const vector<string> sectorsSref;
    static const vector<string> sectorsNaefs;
    static const vector<string> sectorsPolar;
    static const vector<string> sectorsHrwNmm;
    static const vector<string> sectorsHrwArw2;
    static const vector<string> sectorsGefsSpag;
    static const vector<string> sectorsGefsMnsprd;
    static const vector<string> sectorsWw3;
    static const vector<string> sectorsEstofs;
    static const vector<string> sectorsFirewx;
    static const vector<string> sectorsNbm;
    static const vector<string> paramsGfs;
    static const vector<string> labelsGfs;
    static const vector<string> paramsNam;
    static const vector<string> labelsNam;
    static const vector<string> paramsRap;
    static const vector<string> labelsRap;
    static const vector<string> paramsHrrr;
    static const vector<string> labelsHrrr;
    static const vector<string> paramsNamHires;
    static const vector<string> labelsNamHires;
    static const vector<string> paramsSref;
    static const vector<string> labelsSref;
    static const vector<string> paramsNaefs;
    static const vector<string> labelsNaefs;
    static const vector<string> paramsPolar;
    static const vector<string> labelsPolar;
    static const vector<string> paramsHrwNmm;
    static const vector<string> labelsHrwNmm;
    static const vector<string> paramsGefsSpag;
    static const vector<string> labelsGefsSpag;
    static const vector<string> paramsGefsMnsprd;
    static const vector<string> labelsGefsMnsprd;
    static const vector<string> paramsWw3;
    static const vector<string> labelsWw3;
    static const vector<string> paramsEstofs;
    static const vector<string> labelsEstofs;
    static const vector<string> paramsFirefx;
    static const vector<string> labelsFirefx;
    static const vector<string> paramsHrwArw2;
    static const vector<string> labelsHrwArw2;
    static const vector<string> paramsNbm;
    static const vector<string> labelsNbm;
    static const vector<string> modelHrwFv3Params;
    static const vector<string> modelHrwFv3Labels;
};

#endif  // UTILITYMODELNCEPINTERFACE_H
