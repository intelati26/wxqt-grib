// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSCHART_H
#define GFSCHART_H

#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QColor>
#include <QImage>
#include <QString>
#include "gfs/GfsData.h"
#include "gfs/GfsGrid.h"

// The GFS model charts drawn here from the GRIB data (the ones that used to be pictures from the NCEP model guidance site): a color fill, contour lines, wind barbs, highs and lows.
// A chart is a recipe of grids by name; the recipes are in GfsChart.cpp, keyed by the same product codes the model screen has always used ("500_wnd_ht").
namespace GfsChart {
    struct Sector {
        std::string id;      // "CONUS"
        double west, south, east, north;
    };
    const std::vector<Sector>& sectors();
    const Sector * sector(const std::string& id);

    // A color scale by value: stops are (value, color), linear between them. Values below the first or above the last take the end colors.
    struct Ramp {
        std::vector<std::pair<double, QColor>> stops;
        QRgb at(double value) const;
    };

    using Grids = std::map<std::string, GfsGrid::Grid>;
    struct Product {
        std::string id;                         // "500_wnd_ht"
        std::string label;                      // "500mb Wind and Height"
        std::vector<GfsData::Want> wants;       // the records to fetch
        // the filled field (by recipe), its scale, the title line for it and the unit shown on the legend
        std::function<GfsGrid::Grid(const Grids&)> fill;
        Ramp ramp;
        std::string fillTitle;                  // "Wind speed (kt)"
        double legendStep{10.0};
        bool fahrenheitAware{false};            // the fill is a temperature in degrees C: shown in F when the user prefers it
        // contour lines: by the grid of that key, scaled to the label unit
        std::string contourKey;
        double contourScale{1.0};               // 0.1 turns meters into decameters
        double contourInterval{0.0};
        double contourBase{0.0};                // levels are base + n * interval
        std::string contourTitle;               // "Height (dam)"
        bool highsAndLows{false};
        // wind barbs (m/s grids), drawn in knots
        std::string barbU, barbV;
    };
    const std::vector<Product>& products();
    const Product * product(const std::string& id);

    struct Options {
        int width{1100};
        bool fahrenheit{true};
        std::vector<std::vector<std::pair<float, float>>> lines;   // coastlines and borders as (longitude, latitude)
    };
    // The finished picture, with a header (model, product, run and valid times) and a legend. Null when a needed grid is missing.
    QImage render(const Product& product, const Sector& sector, const Grids& grids, const GfsData::Run& run, int forecastHour, const Options& options);
}

#endif  // GFSCHART_H
