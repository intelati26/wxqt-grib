// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef DROUGHTCROPS_H
#define DROUGHTCROPS_H

#include <array>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <QDate>
#include "drought/UtilityDrought.h"

// How much of a crop (or of the livestock) lies in drought, the way the Department of Agriculture's weekly "Agriculture in Drought" figures are made: a county counts as in drought when the
// Drought Monitor's category at its center point is that category or worse, and a commodity's share is the production of the counties in drought over the production of all of them.
// The weights are a static table of the Census of Agriculture (one number a county and commodity, a few KB), the drought is the week's map read at the counties' points (exact, from its
// polygons), so nothing is stored for each week and crop. Measured against the department's figures for all 19 commodities (docs/plans.md, section 32): median 0.5 point.
namespace DroughtCrops {
    struct Commodity {
        std::string key;     // "corn"
        std::string name;    // "Corn"
        std::string item;    // the Quick Stats item: "CORN, GRAIN - PRODUCTION, MEASURED IN BU"
        std::string unit;    // "bushels"
        std::string altItem; // the item an earlier census calls it, when it differs (sugarcane: "SUGARCANE, SUGAR - ..." in 2017), tried when the first has no answer
    };
    const std::vector<Commodity>& commodities();
    const Commodity * find(const std::string& key);

    struct County {
        std::string fips;    // state and county code, "19109"
        double lat{0}, lon{0};
        double value{0};     // production in the item's unit (counties whose number the Census withholds are left out)
    };
    struct Table {
        std::string key;
        int census{0};
        std::vector<County> counties;
        std::string error;
        double total() const;
        bool empty() const { return counties.empty(); }
    };

    // The census whose table the department uses for a week's figure: 2017's for the weeks until the 2022 census was in (between December 2023 and February 2025 for rice: the week of the
    // change is not known, mid 2024 is used), 2022's after.
    int censusFor(const QDate& week);

    // the county table as the cache keeps it: "fips,value" lines (no centers: they come from the Census Bureau's gazetteer)
    std::string toCsv(const Table& table);
    // Quick Stats' JSON answer (api_GET, format JSON) to the counties' lines of "fips,value"; withheld ("(D)") counties are left out; "" with `error` set when it is not an answer
    std::string parseQuickStats(const std::string& json, std::string& error);
    // the Census Bureau's gazetteer of counties (the zip, or its text): fips -> (latitude, longitude) of the internal point
    bool parseGazetteer(const std::string& text, std::vector<County>& centers);
    Table fromCsv(const std::string& key, int census, const std::string& csv, const std::vector<County>& centers);

    // The table of a commodity from a census: read from the permanent cache, else fetched from the Quick Stats API with `apiKey` (a free key from quickstats.nass.usda.gov/api;
    // the environment's WXQT_NASS_KEY when empty) and kept. Blocking: call off the interface thread. `error` says what is missing.
    Table table(const Commodity& commodity, int census, std::string& error, const std::string& apiKey = {});

    // The week's Monitor map as a grid, the way the rain is kept (DroughtRain): one byte a cell (0 none, 1 to 5 D0 to D4, the worst whose shapes hold the cell's center), 0.01 degree over the
    // contiguous states (130 to 60 W, 20 to 55 N: 7000 x 3500 cells), made from the week's vector (DroughtVector) when asked (the rows as runs of one category and zlib would be about 120 KB a week: encode() / decode() are there
    // for a saved copy, not used yet). Everything that needs a week's drought at a place reads it from here (the crops, any area). A 0.05 degree grid moved a county near an edge into the next category, several
    // points for a commodity of a few heavy counties (sugarcane 7); at 0.01 degree the worst of 19 commodities is 1.3 points (rice, 100 counties) and most are under 0.3.
    struct WeekGrid {
        static constexpr double west = -130.0, north = 55.0, step = 0.01;
        static constexpr int columns = 7000, rows = 3500;
        std::vector<uint8_t> cells;   // rows from the north
        bool empty() const { return cells.empty(); }
        int categoryAt(double lat, double lon) const;   // -1 outside the grid
        // the share of the point that is in each category or worse (D0 or worse ... D4: 0 to 1): the four cells nearest the point weighted bilinearly, so a county whose center lies within a cell of
        // an edge counts partly in both and a heavy one is not moved wholly across it; false outside the grid
        bool atLeastAt(double lat, double lon, double (&share)[5]) const;
    };
    std::string encode(const WeekGrid& grid);
    bool decode(const std::string& bytes, WeekGrid& grid);
    // the grid of a Tuesday, made from the week's map (monitor(): the small vector kept for good) by rasterizing it, about a second; blocking
    bool weekGrid(const QDate& tuesday, WeekGrid& out, std::string& error);

    // the share of the commodity in each category or worse on the week's map: percent of the production
    struct Share {
        double atLeast[5]{};   // D0 or worse ... D4
        double total{0};
        int counties{0};
    };
    // The week's category at every county's center, kept (counties/<yyyymmdd>_v1.bin in the drought cache: the shares of each county's point that lie in D0 or worse ... D4, a byte each, 5 bytes
    // a county, about 3 KB a week) so that any commodity, area and range of weeks is read from it without the map: made from the week's grid (weekGrid) the first time. Blocking.
    struct WeekCounties {
        std::map<std::string, std::array<uint8_t, 5>> part;   // fips -> the share of the county's point in each category or worse, 0 to 255
    };
    bool weekCounties(const QDate& tuesday, WeekCounties& out, std::string& error);
    // the counties' centers (the Census Bureau's gazetteer, kept in the drought cache), sorted by code
    const std::vector<County>& centers(std::string& error);
    // the counties whose center lies in the areas (the states, an office, a county ... as the drought window draws them)
    std::set<std::string> countiesIn(const std::vector<UtilityDrought::Area>& areas);
    // the share of the commodity in each category or worse; `only`, when given, keeps the counties in it (an area)
    Share share(const WeekCounties& week, const Table& table, const std::set<std::string> * only = nullptr);
    // the Monitor's map of a Tuesday: the small vector form (DroughtVector, about 30 KB) kept for good in the drought cache, made once from the week's KMZ; blocking
    bool monitor(const QDate& tuesday, UtilityDrought::Monitor& out, std::string& error);
}

#endif  // DROUGHTCROPS_H
