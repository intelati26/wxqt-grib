// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityModelNsslWrfInterface.h"

const vector<string> UtilityModelNsslWrfInterface::models{
    "WRF",
    "FV3",
    "HRRRV3",
    "WRF_3KM",
};

const vector<string> UtilityModelNsslWrfInterface::paramsNsslWrf{
    "mslp",
    "10mwind",
    "sfct",
    "sfctd",
    "visibility",
    "250w",
    "500w",
    "850w",
    "mucape",
    "sbcape",
    "srh01",
    "srh03",
    "stpfixed",
    "qpf_001h",
    "qpf_006h",
    "qpf_total",
    "snowfall_total",
    "cref_uh075",
    "maxref1000m",
    "ref1000m_uh075",
    "uh03_004hmax",
    "uh25_004hmax",
    "dd_004hmax",
    "ud_004hmax",
    "wspd_004hmax",
    "graupelsize_001hmax",
    "hailcast_004hmax",
    "ltgthrt1",
    "ltgthrt2",
    "ltgthrt3"
};

const vector<string> UtilityModelNsslWrfInterface::labelsNsslWrf{
    "MSLP (mb)",
    "10 m Wind (kt)",
    "2-m Temperature",
    "2-m Dew Point",
    "Visibility",
    "250 mb",
    "500 mb",
    "850 mb",
    "MUCAPE",
    "SBCAPE",
    "0-1km SRH",
    "0-3km SRH",
    "STP",
    "1-h QPF",
    "6-h QPF",
    "Total QPF",
    "Total Snowfall",
    "Reflectivity - Composite",
    "Reflectivity - 1h-max (1km AGL)",
    "Reflectivity - 1km AGL",
    "Updraft Helicity - 4-h max (0-3km)",
    "Updraft Helicity - 4-h max (2-5km)",
    "Vertical Velocity - 4-h min Downdraft",
    "Vertical Velocity - 4-h max Updraft",
    "Wind Speed - 4-h max",
    "Hail - 1-h max Graupel",
    "Hail - 4-h max HAILCAST",
    "Lightning - 1-h max Threat1",
    "Lightning - 1-h max Threat2",
    "Lightning - 1-h max Threat3"
};

const vector<string> UtilityModelNsslWrfInterface::paramsNsslFv3{
    "mslp",
    "10mwind",
    "sfct",
    "sfctd",
    "250w",
    "500w",
    "850w",
    "sbcape",
    "srh01",
    "srh03",
    "stpfixed",
    "qpf_001h",
    "qpf_006h",
    "qpf_total",
    "cref_uh075",
    "uh25_004hmax",
    "dd_004hmax",
    "ud_004hmax"
};

const vector<string> UtilityModelNsslWrfInterface::labelsNsslFv3{
    "MSLP (mb)",
    "10 m Wind (kt)",
    "2-m Temperature",
    "2-m Dew Point",
    "250 mb",
    "500 mb",
    "850 mb",
    "SBCAPE",
    "0-1km SRH",
    "0-3km SRH",
    "STP",
    "1-h QPF",
    "6-h QPF",
    "Total QPF",
    "Reflectivity - Composite",
    "Updraft Helicity - 4-h max (2-5km)",
    "Vertical Velocity - 4-h min Downdraft",
    "Vertical Velocity - 4-h max Updraft"
};

const vector<string> UtilityModelNsslWrfInterface::paramsNsslHrrrv3{
    "mslp",
    "10mwind",
    "sfct",
    "sfctd",
    "250w",
    "500w",
    "850w",
    "mucape",
    "sbcape",
    "srh01",
    "srh03",
    "stpfixed",
    "qpf_001h",
    "qpf_006h",
    "qpf_total",
    "cref_uh075",
    "maxref1000m",
    "ref1000m_uh075",
    "uh25_004hmax",
    "ud_004hmax",
    "wspd_004hmax"
};

const vector<string> UtilityModelNsslWrfInterface::labelsNsslHrrrv3{
    "MSLP (mb)",
    "10 m Wind (kt)",
    "2-m Temperature",
    "2-m Dew Point",
    "250 mb",
    "500 mb",
    "850 mb",
    "MUCAPE",
    "SBCAPE",
    "0-1km SRH",
    "0-3km SRH",
    "STP",
    "1-h QPF",
    "6-h QPF",
    "Total QPF",
    "Reflectivity - Composite",
    "Reflectivity - 1h-max (1km AGL)",
    "Reflectivity - 1km AGL",
    "Updraft Helicity - 4-h max (2-5km)",
    "Vertical Velocity - 4-h max Updraft",
    "Wind Speed - 4-h max"
};

const vector<string> UtilityModelNsslWrfInterface::sectorsLong{
    "CONUS",
    "Central Plains",
    "Mid Atlantic",
    "Midwest",
    "Northeast",
    "Northern Plains",
    "Northwest",
    "Southeast",
    "Southern Plains",
    "Southwest"
};

const vector<string> UtilityModelNsslWrfInterface::sectors{
    "conus",
    "cp",
    "ma",
    "mw",
    "ne",
    "np",
    "nw",
    "se",
    "sp",
    "sw",
};
