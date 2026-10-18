// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NCEIINVENTORY_H
#define NCEIINVENTORY_H

#include <string>
#include <QByteArray>

// NCEI's model archive (https://www.ncei.noaa.gov/oa/prod-model/) keeps a ".inv" beside each file. For the GRIB2 files it is NOAA's own index form; for the GRIB1 ones (the NAM analyses of
// 2004-2020, the 1 degree GFS analyses) it is wgrib's inventory ("12:5483620:D=2011042712:HGT:500 mb:kpds=7,100,500:3hr fcst:winds in grid direction:"Geopotential height [gpm]"). This turns those
// into NOAA's form ("12:5483620:d=2011042712:HGT:500 mb:3 hour fcst:") with the level and forecast words NOAA's files use, so that the same charts read them.
namespace NceiInventory {
    bool handles(const std::string& url);      // an ".inv" of that archive
    QByteArray index(const std::string& url);  // its text in NOAA's form (the GRIB2 ones as they are), empty when it could not be had
    std::string convert(const std::string& wgribText);   // (no network: the part that can be tested)
}

#endif  // NCEIINVENTORY_H
