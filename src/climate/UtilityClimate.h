// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCLIMATE_H
#define UTILITYCLIMATE_H

#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// Climate and ocean: sea surface temperature (raw and anomaly), El Niño / La Niña (the Climate Prediction Center's ENSO status and
// figures), the tropical intraseasonal oscillation, and the index series behind the cycles (ONI / RONI, Niño regions, SOI, PDO,
// NAO, AO, PNA, AAO). Every source is a plain public NOAA picture or text file.
namespace UtilityClimate {
    struct Tile {
        string section;
        string label;
        string url;
        string full;   // a larger version for the click (empty when the picture is the same)
        string history;   // the HistoryProduct key when the picture has a history and a loop (the click opens that)
        Tile(string section, string label, string url, string full = "", string history = "")
            : section{std::move(section)}, label{std::move(label)}, url{std::move(url)}, full{std::move(full)}, history{std::move(history)} {}
    };

    // the pictures, grouped by section in display order
    const vector<Tile>& tiles();

    struct Reading {
        int year;
        int month;       // 1-12 (the middle month of a three-month season)
        double value;
    };
    using Series = vector<Reading>;

    struct IndexInfo {
        string key;       // "roni"
        string label;     // "Relative Oceanic Niño Index (RONI), 3-month"
        string unit;
        string url;
        double threshold; // the El Niño / La Niña line (+/-), or 0 for none
    };

    // the index series that get a chart, in display order
    const vector<IndexInfo>& indices();
    bool loadSeries(const IndexInfo&, Series&, string& error);

    struct TextProduct {
        string label;
        string url;
        int tailLines;    // how many of the last lines to show (0 = all)
    };
    const vector<TextProduct>& textProducts();
    string tailOf(const string& text, int lines);

    // Pictures that have a history: NOAA Coral Reef Watch's daily global SST products (one picture per day, back to 2020-01-01) and NHC's
    // 14-day SST loops
    struct HistoryProduct {
        string key;
        string label;
        bool dated;      // one picture a day at a date-built address (else a fixed list of recent frames)
    };
    const vector<HistoryProduct>& historyProducts();
    // the frame addresses, oldest first: `count` pictures `stepDays` apart ending on endDate (yyyyMMdd); a fixed-list product ignores the date
    vector<string> historyFrames(const string& key, const string& endDate, int stepDays, int count);
    string earliestHistoryDate();                // "20200101"
    string latestHistoryDate();                  // the newest day that has a picture (looked for, so it is not always yesterday)

    struct EnsoStatus {
        string status;     // "El Niño Advisory"
        string issued;     // "10 September 2026"
        string synopsis;
        string discussion; // the whole text
        string next;       // when the next one is due
    };
    bool ensoStatus(EnsoStatus&, string& error);
}

#endif  // UTILITYCLIMATE_H
