// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectModel.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "UtilityModelEsrlInterface.h"
#include "UtilityModelNcepInterface.h"
#include "UtilityModelNsslWrfInterface.h"
#include "UtilityModelSpcHrefInterface.h"
#include "UtilityModelSpcHrrrInterface.h"
#include "UtilityModelSpcSrefInterface.h"
#include "UtilityModelWpcGefsInterface.h"

ObjectModel::ObjectModel(const string& prefModel)
    : prefModel{prefModel}
    , prefSector{"MODEL" + prefModel + "SECTORLASTUSED"}
    , prefParam{"MODEL" + prefModel + "PARAMLASTUSED"}
    , prefRunPosn{prefModel + "RUNPOSN"}
    , prefRunPosnIdx{prefModel + "RUNPOSN" + "IDX"}
{
    if (prefModel == "NCARENSEMBLE") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "t2mean";
        sector = "CONUS";
        model = "NCARENSEMBLE";
        models = {"NCARENSEMBLE"};
    } else if (prefModel == "NSSLWRF") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "sfct";
        sector = "CONUS";
        model = "WRF";
        models = UtilityModelNsslWrfInterface::models;
    } else if (prefModel == "ESRL") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "1ref_sfc";
        model = "HRRR_NCEP";
        sector = "US";
        models = UtilityModelEsrlInterface::models;
    } else if (prefModel == "GLCFS") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 0;
        param = "wv";
        model = "GLCFS";
        models = {"GLCFS"};
        sector = "All Lakes";
    } else if (prefModel == "NCEP") {
        run = "00Z";
        timeStr = "003";
        timeIdx = 1;
        param = "500_vort_ht";
        model = "GFS";
        sector = "NAMER";
        models = UtilityModelNcepInterface::models;
    } else if (prefModel == "WPCGEFS") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "capegt500";
        sector = "US";
        model = "WPCGEFS";
        models = {"WPCGEFS"};
    } else if (prefModel == "SPCHRRR") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "sfcprec";
        model = "HRRR";
        models = {"HRRR"};
        sector = "US";
    } else if (prefModel == "SPCHREF") {
        run = "00Z";
        timeStr = "01";
        timeIdx = 1;
        param = "500wmean,500hmean";
        model = "HREF";
        models = {"HREF"};
        sector = "CONUS";
    } else if (prefModel == "SPCSREF") {
        run = "00Z";
        timeStr = "03";
        timeIdx = 1;
        param = "SREFH5";
        model = "SREF";
        models = {"SREF"};
        sector = "US";
    }
    getPrefs();
    setModelVars(model);
}

void ObjectModel::getPrefs() {
    model = Utility::readPref(prefModel, model);
    param = Utility::readPref(prefParam, param);
    sector = Utility::readPref(prefSector, sector);
    timeStr = Utility::readPref(prefRunPosn, timeStr);
    timeIdx = Utility::readPrefInt(prefRunPosnIdx, static_cast<int>(timeIdx));
}

void ObjectModel::writePrefs() const {
    Utility::writePref(prefModel, model);
    Utility::writePref(prefParam, param);
    Utility::writePref(prefSector, sector);
    Utility::writePref(prefRunPosn, timeStr);
    Utility::writePrefInt(prefRunPosnIdx, static_cast<int>(timeIdx));
}

void ObjectModel::loadTimeList(int from, int to, int by) {
    for (auto value : range3(from, to, by)) {
        times.push_back(WString::fixedLengthStringPad0(To::string(value), 2));
    }
}

void ObjectModel::loadTimeList3(int from, int to, int by) {
    for (auto value : range3(from, to, by)) {
        times.push_back(WString::fixedLengthStringPad0(To::string(value), 3));
    }
}

void ObjectModel::loadRunList(int from, int to, int by) {
    for (auto value : range3(from, to, by)) {
        runs.push_back(WString::fixedLengthStringPad0(To::string(value), 2) + "Z");
    }
}

void ObjectModel::setModelVars(const string& modelName) {
    modelToken = prefModel + ":" + modelName;
    if (modelToken == "NSSLWRF:WRF" || modelToken == "NSSLWRF:WRF_3KM") {
        params = UtilityModelNsslWrfInterface::paramsNsslWrf;
        paramLabels = UtilityModelNsslWrfInterface::labelsNsslWrf;
        sectors = UtilityModelNsslWrfInterface::sectorsLong;
        times.clear();
        loadTimeList(1, 36, 1);
    } else if (modelToken == "NSSLWRF:FV3") {
        params = UtilityModelNsslWrfInterface::paramsNsslFv3;
        paramLabels = UtilityModelNsslWrfInterface::labelsNsslFv3;
        sectors = UtilityModelNsslWrfInterface::sectorsLong;
        times.clear();
        loadTimeList(1, 60, 1);
    } else if (modelToken == "NSSLWRF:HRRRV3") {
        params = UtilityModelNsslWrfInterface::paramsNsslHrrrv3;
        paramLabels = UtilityModelNsslWrfInterface::labelsNsslHrrrv3;
        sectors = UtilityModelNsslWrfInterface::sectorsLong;
        times.clear();
        loadTimeList(1, 36, 1);
    } else if (modelToken == "ESRL:HRRR" || modelToken == "ESRL:HRRR_NCEP") {
        params = UtilityModelEsrlInterface::modelHrrrParams;
        paramLabels = UtilityModelEsrlInterface::modelHrrrLabels;
        sectors = UtilityModelEsrlInterface::sectorsHrrr;
        times.clear();
        loadTimeList(0, 36, 1);
    } else if (modelToken == "ESRL:HRRR_AK") {
        params = UtilityModelEsrlInterface::modelHrrrParams;
        paramLabels = UtilityModelEsrlInterface::modelHrrrLabels;
        sectors = UtilityModelEsrlInterface::sectorsHrrrAk;
        times.clear();
        loadTimeList(0, 36, 1);
    } else if (modelToken == "ESRL:HRRR_SMOKE") {
        params = UtilityModelEsrlInterface::modelHrrrSmokeParams;
        paramLabels = UtilityModelEsrlInterface::modelHrrrSmokeLabels;
        sectors = UtilityModelEsrlInterface::sectorsHrrrSmoke;
        times.clear();
        loadTimeList(0, 48, 1);
    } else if (modelToken == "ESRL:RAP" || modelToken == "ESRL:RAP_NCEP") {
        params = UtilityModelEsrlInterface::modelRapParams;
        paramLabels = UtilityModelEsrlInterface::modelRapLabels;
        sectors = UtilityModelEsrlInterface::sectorsRap;
        times.clear();
        loadTimeList(0, 21, 1);
    } else if (modelToken == "NCEP:GFS") {
        params = UtilityModelNcepInterface::paramsGfs;
        paramLabels = UtilityModelNcepInterface::labelsGfs;
        sectors = UtilityModelNcepInterface::sectorsGfs;
        times.clear();
        loadTimeList3(0, 243, 3);
        loadTimeList3(252, 396, 12);
        setupListRunZ();
    } else if (modelToken == "NCEP:HRRR") {
        params = UtilityModelNcepInterface::paramsHrrr;
        paramLabels = UtilityModelNcepInterface::labelsHrrr;
        sectors = UtilityModelNcepInterface::sectorsHrrr;
        times.clear();
        loadTimeList3(0, 18, 1);
        runs.clear();
        loadRunList(0, 22, 1);
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:RAP") {
        params = UtilityModelNcepInterface::paramsRap;
        paramLabels = UtilityModelNcepInterface::labelsRap;
        sectors = UtilityModelNcepInterface::sectorsRap;
        times.clear();
        loadTimeList3(0, 21, 1);
        runs.clear();
        loadRunList(0, 22, 1);
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:NAM-HIRES") {
        params = UtilityModelNcepInterface::paramsNamHires;
        paramLabels = UtilityModelNcepInterface::labelsNamHires;
        sectors = UtilityModelNcepInterface::sectorsNamHires;
        times.clear();
        loadTimeList3(1, 61, 1);
        setupListRunZ();
    } else if (modelToken == "NCEP:NAM") {
        params = UtilityModelNcepInterface::paramsNam;
        paramLabels = UtilityModelNcepInterface::labelsNam;
        sectors = UtilityModelNcepInterface::sectorsNam;
        times.clear();
        loadTimeList3(0, 85, 3);
        setupListRunZ();
    } else if (modelToken == "NCEP:HRW-FV3") {
        params = UtilityModelNcepInterface::modelHrwFv3Params;
        paramLabels = UtilityModelNcepInterface::modelHrwFv3Labels;
        sectors = UtilityModelNcepInterface::sectorsHrwNmm;
        times.clear();
        loadTimeList3(1, 49, 1);
        loadTimeList3(51, 61, 3);
        runs.clear();
        runs.emplace_back("00Z");
        runs.emplace_back("12Z");
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:HRW-ARW") {
        params = UtilityModelNcepInterface::paramsHrwNmm;
        paramLabels = UtilityModelNcepInterface::labelsHrwNmm;
        sectors = UtilityModelNcepInterface::sectorsHrwNmm;
        times.clear();
        loadTimeList3(1, 49, 1);
        runs.clear();
        runs.emplace_back("00Z");
        runs.emplace_back("12Z");
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:HRW-ARW2") {
        params = UtilityModelNcepInterface::paramsHrwArw2;
        paramLabels = UtilityModelNcepInterface::labelsHrwArw2;
        sectors = UtilityModelNcepInterface::sectorsHrwArw2;
        times.clear();
        loadTimeList3(1, 49, 1);
        runs.clear();
        runs.emplace_back("00Z");
        runs.emplace_back("12Z");
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:NBM") {
        params = UtilityModelNcepInterface::paramsNbm;
        paramLabels = UtilityModelNcepInterface::labelsNbm;
        sectors = UtilityModelNcepInterface::sectorsNbm;
        times.clear();
        loadTimeList3(0, 264, 3);
        runs.clear();
        runs.emplace_back("00Z");
        runs.emplace_back("06Z");
        runs.emplace_back("12Z");
        runs.emplace_back("18Z");
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:GEFS-SPAG") {
        params = UtilityModelNcepInterface::paramsGefsSpag;
        paramLabels = UtilityModelNcepInterface::labelsGefsSpag;
        sectors = UtilityModelNcepInterface::sectorsGefsSpag;
        times.clear();
        loadTimeList3(0, 180, 6);
        loadTimeList3(192, 384, 12);
        setupListRunZ();
    } else if (modelToken == "NCEP:GEFS-MEAN-SPRD") {
        params = UtilityModelNcepInterface::paramsGefsMnsprd;
        paramLabels = UtilityModelNcepInterface::labelsGefsMnsprd;
        sectors = UtilityModelNcepInterface::sectorsGefsMnsprd;
        times.clear();
        loadTimeList3(0, 180, 6);
        loadTimeList3(192, 384, 12);
        setupListRunZ();
    } else if (modelToken == "NCEP:SREF") {
        params = UtilityModelNcepInterface::paramsSref;
        paramLabels = UtilityModelNcepInterface::labelsSref;
        sectors = UtilityModelNcepInterface::sectorsSref;
        times.clear();
        loadTimeList3(0, 87, 3);
        setupListRunZWithStart("03Z");
    } else if (modelToken == "NCEP:NAEFS") {
        params = UtilityModelNcepInterface::paramsNaefs;
        paramLabels = UtilityModelNcepInterface::labelsNaefs;
        sectors = UtilityModelNcepInterface::sectorsNaefs;
        times.clear();
        loadTimeList3(0, 384, 6);
        setupListRunZ();
    } else if (modelToken == "NCEP:POLAR") {
        params = UtilityModelNcepInterface::paramsPolar;
        paramLabels = UtilityModelNcepInterface::labelsPolar;
        sectors = UtilityModelNcepInterface::sectorsPolar;
        times.clear();
        loadTimeList3(0, 384, 24);
        runs.clear();
        runs.emplace_back("00Z");
        runTimeData.listRun = runs;
    } else if (modelToken == "NCEP:WW3") {
        params = UtilityModelNcepInterface::paramsWw3;
        paramLabels = UtilityModelNcepInterface::labelsWw3;
        sectors = UtilityModelNcepInterface::sectorsWw3;
        times.clear();
        loadTimeList3(0, 126, 6);
        setupListRunZ();
    } else if (modelToken == "NCEP:ESTOFS") {
        params = UtilityModelNcepInterface::paramsEstofs;
        paramLabels = UtilityModelNcepInterface::labelsEstofs;
        sectors = UtilityModelNcepInterface::sectorsEstofs;
        times.clear();
        loadTimeList3(0, 180, 1);
        setupListRunZ();
    } else if (modelToken == "NCEP:FIREWX") {
        params = UtilityModelNcepInterface::paramsFirefx;
        paramLabels = UtilityModelNcepInterface::labelsFirefx;
        sectors = UtilityModelNcepInterface::sectorsFirewx;
        times.clear();
        loadTimeList3(0, 37, 1);
        setupListRunZ();
    } else if (modelToken == "WPCGEFS:WPCGEFS") {
        params = UtilityModelWpcGefsInterface::params;
        paramLabels = UtilityModelWpcGefsInterface::labels;
        sectors = UtilityModelWpcGefsInterface::sectors;
        times.clear();
        loadTimeList3(0, 240, 6);
        runs = runTimeData.listRun;
    } else if (modelToken == "SPCHRRR:HRRR") {
        params = UtilityModelSpcHrrrInterface::params;
        paramLabels = UtilityModelSpcHrrrInterface::labels;
        sectors = UtilityModelSpcHrrrInterface::sectors;
        times.clear();
        loadTimeList(2, 15, 1);
        runs = runTimeData.listRun;
    } else if (modelToken == "SPCHREF:HREF") {
        params = UtilityModelSpcHrefInterface::params;
        paramLabels = UtilityModelSpcHrefInterface::labels;
        sectors = UtilityModelSpcHrefInterface::sectorsLong;
        times.clear();
        loadTimeList(1, 49, 1);
        runs = runTimeData.listRun;
    } else if (modelToken == "SPCSREF:SREF") {
        params = UtilityModelSpcSrefInterface::params;
        paramLabels = UtilityModelSpcSrefInterface::labels;
        sectors.clear();
        times.clear();
        loadTimeList3(0, 90, 3);
        runs = runTimeData.listRun;
    }
    if (!sectors.empty()) {
        if (!contains(sectors, sector)) {
            sector = sectors[0];
        }
    }
    if (!contains(params, param)) {
        if (!params.empty()) {
            param = params[0];
        }
    }
    if (timeIdx > times.size()) {
        setTimeIdx(2);
    }
}

void ObjectModel::setupListRunZ() {
    runs.clear();
    runs.emplace_back("00Z");
    runs.emplace_back("06Z");
    runs.emplace_back("12Z");
    runs.emplace_back("18Z");
    runTimeData.listRun = runs;
}

void ObjectModel::setupListRunZWithStart([[maybe_unused]] const string& start) {
    runs.clear();
    runs.emplace_back("03Z");
    runs.emplace_back("09Z");
    runs.emplace_back("15Z");
    runs.emplace_back("21Z");
    runTimeData.listRun = runs;
}

void ObjectModel::setTimeIdx(int timeIdxF) {
    if (timeIdxF > -2 && timeIdxF < static_cast<int>(times.size())) {
        timeIdx = timeIdxF;
        timeStr = times[timeIdx];
    }
}

void ObjectModel::timeIdxIncr() {
    timeIdx += 1;
    timeStr = Utility::safeGet(times, timeIdx);
}

void ObjectModel::timeIdxDecr() {
    timeIdx -= 1;
    timeStr = Utility::safeGet(times, timeIdx);
}

void ObjectModel::leftClick() {
    if (timeIdx == 0) {
        setTimeIdx(static_cast<int>(times.size()) - 1);
    } else {
        timeIdxDecr();
    }
}

void ObjectModel::rightClick() {
    if (timeIdx == times.size() - 1) {
        setTimeIdx(0);
    } else {
        timeIdxIncr();
    }
}

void ObjectModel::setTimeArr(int idx, const string& time) {
    times[idx] = time;
}

string ObjectModel::getTime() const {
    if (WString::contains(timeStr, " ")) {
        return WString::split(timeStr, " ")[0];
    } else {
        return timeStr;
    }
}
