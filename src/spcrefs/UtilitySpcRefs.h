// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSPCREFS_H
#define UTILITYSPCREFS_H

#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include "zarr/ZarrStore.h"

using std::string;
using std::vector;

// SPC's REFS ensemble products (https://www.spc.noaa.gov/exper/refs/viewer): the same open Zarr data that SPC's own
// browser viewer draws, read here directly. One store per forecast cycle; each array is a field on the 3 km CONUS
// Lambert grid (LambertGrid.h) for every forecast hour, ready-made ensemble products (means, maxima, probabilities,
// "paintball" member masks, updraft-helicity swaths), so there is no per-member GRIB processing.
namespace UtilitySpcRefs {
    struct Stop {
        double value;   // in shown units: the bottom of the band (banded) or the value of the colour (blended)
        int r;
        int g;
        int b;
    };

    enum class Kind {
        Blended,     // colours blend between stops
        Banded,      // each stop's colour covers its value up to the next stop; the last one has no top
        Paintball,   // integer bit mask: bit i set = member i is over the threshold; each member drawn in its own colour
        Category     // small integer codes, one colour each
    };

    struct Product {
        string id;                  // the array name in the store
        string label;
        string group;
        string units;               // as shown
        double factor{1.0};         // shown value = stored * factor + offset
        double offset{0.0};
        Kind kind{Kind::Blended};
        vector<Stop> stops;
        double hideBelow{-1e30};    // shown values below this are left transparent
        bool members{false};        // 4-D array: (member, time, y, x), one member at a time
        int digits{1};              // decimals in the read-out
        string timeDimension;       // the name of the array's time coordinate
    };

    // how an array is shown (SPC's own titles for its main products, a tidied name for the rest) and which colours
    Product describe(const string& id, const ZarrStore::Array& array);
    string memberDisplayName(const string& raw);
    // the names of the ten members of the newest store opened ("RRFS Ctl", "HRRR -6h" ...), kept for the lists that offer one member; empty until a store has been opened
    // keyed by the run ("yyyyMMddHH"): the names of another run's members are not assumed to be these. memberNames of a run that is not known gives the newest run's.
    void setMemberNames(const string& runId, const vector<string>& displayNames);
    vector<string> memberNames(const string& runId = {});
    std::pair<int, int> menuPosition(const Product& product);   // (group, place in SPC's list): the order of the menu
    vector<Stop> rainbowStops(double low, double high);   // the colour table of a field with none of its own, over its range
    // the products SPC's own viewer lists, described from their names (no store needed): what the chart registry makes charts of
    vector<Product> curatedProducts();
    // every field of the store (the curated ones and the rest), for the model screen's SPC REFS model; rememberArrays keeps the list a store has for the next start
    vector<Product> allProducts();
    void rememberArrays(const ZarrStore& store);
}

#endif  // UTILITYSPCREFS_H
