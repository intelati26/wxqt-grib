// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTHISTORY_H
#define DROUGHTHISTORY_H

#include <cmath>
#include <string>
#include <vector>
#include <QString>

// The month by month history of an area (the country, a state, an office ...): rain, how far it was from normal and where it ranks, the temperature, and the share of the area in each
// drought category. Kept as a plain CSV file per area in the data folder (drought/history/<area>.csv, metric units, one row a month, oldest first) that the program only ever adds to:
// a month that is in the file is not worked out again, and a column that was empty (the drought maps are fetched for the last years only) is filled in later. A spreadsheet opens it.
namespace DroughtHistory {
    struct Row {
        QString month;                                   // "2026-09"
        double rain{NAN}, normal{NAN}, departure{NAN}, percent{NAN}, rainRank{NAN};   // millimeters, then percent of normal and the rank (0 to 100)
        double temperature{NAN}, temperatureRank{NAN};   // the departure from normal in degrees C, and its rank
        QString mapDate;                                 // the Monitor map the shares are from (the last one of the month), "20260929"
        double d[5]{NAN, NAN, NAN, NAN, NAN};            // percent of the area in D0 or worse ... D4
        double dsci{NAN};                                // the Severity and Coverage index, 0 to 500
        bool hasWeather() const { return !std::isnan(rain); }
        bool hasDrought() const { return !std::isnan(d[0]); }
    };
    QString folder();                                    // <data folder>/drought/history, made when asked
    QString fileFor(const std::string& areaId);          // its CSV (the id made safe for a file name)
    std::string toCsv(const std::string& areaName, const std::vector<Row>& rows);
    std::vector<Row> fromCsv(const std::string& text);   // comment lines (#) and the header are skipped; a missing value is NaN
    std::vector<Row> read(const std::string& areaId);
    bool write(const std::string& areaId, const std::string& areaName, const std::vector<Row>& rows);   // all or nothing
}

#endif  // DROUGHTHISTORY_H
