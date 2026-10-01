// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYSPCREFS_H
#define UTILITYSPCREFS_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include <QImage>
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
    // every field of a store, in menu order (groups, then SPC's order within a group)
    vector<Product> catalog(const ZarrStore& store);

    // "https://www.spc.noaa.gov/exper/refs/data/2026/10/01/refs-spc_20261001_1800.zarr"
    string cycleUrl(const QDateTime& initUtc);
    // the newest cycles that exist on the server, newest first (probes 00 / 06 / 12 / 18 UTC cycles back from now)
    vector<QDateTime> findCycles(int wanted);

    // the data layer as an image on the grid (north at the top), transparent where there is nothing to show
    QImage dataImage(const Product& product, const vector<float>& values, const vector<string>& memberNames);
    // the finished picture: light map background, county and state lines, the data, a colour key
    QByteArray renderPng(const Product& product, const vector<float>& values, const vector<string>& memberNames);
    // the value under a grid point as shown (NaN when there is none)
    double shownValue(const Product& product, const vector<float>& values, int column, int row);
    // text for the read-out at a grid point ("42.5 dBZ", "6 of 10 members")
    string readout(const Product& product, const vector<float>& values, int column, int row, const vector<string>& memberNames);
    string memberDisplayName(const string& raw);
}

#endif  // UTILITYSPCREFS_H
