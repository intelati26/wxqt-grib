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
#include "gfs/GfsClimate.h"
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
    // What the fill's numbers are, for showing them in the user's units (the grids are in degrees C, millimeters, centimeters and so on)
    enum class Quantity { Other, Temperature, Millimeters, Centimeters };

    // what a product's own derivations may need: the hour, the run (for the valid date) and the climatology
    struct Context {
        int hour;
        GfsData::Run run;
        const GfsClimate * climate;
    };
    // One family of contour lines
    struct ContourSet {
        std::string key;                // the grid
        double scale{1.0};              // grid value * scale = the number labelled (0.1: meters to decameters)
        double interval{0.0};
        double base{0.0};               // levels are base + n * interval
        std::string title;              // "Height (dam)"
        QColor color{30, 30, 30};
        QColor colorBelow;              // lines under the split value in this color instead (a thickness line for the rain / snow edge); invalid = not used
        double split{0.0};
        bool dashed{false};
        bool highsAndLows{false};
        bool onlyBelowZero{false};      // omega: the rising air only
        double width{1.0};
    };
    struct Product {
        std::string source{"GFS"};              // the model: "GFS" or "NBM" (an id is only unique within one)
        std::string id;                         // "500_wnd_ht"
        std::string label;                      // "500mb Wind and Height"
        // the records to fetch, by forecast hour (default: wants, at the hour shown)
        std::vector<GfsData::Want> wants;
        std::function<std::vector<GfsData::Need>(int hour)> needs;
        // what to ask for when the run does not have `needs` (a 6 hour amount that ends between the hours the blend makes them: the 1 hour amounts, added up); empty: nothing else to try
        std::function<std::vector<GfsData::Need>(int hour)> fallbackNeeds;
        std::function<void(Grids&, const Context&)> derive;   // adds grids made from the fetched ones
        std::function<GfsGrid::Grid(const Grids&)> fill;
        Ramp ramp;                              // in the grids' own units
        // more fills painted over the first where they have a value (the snow and the mixed precipitation over the rain)
        std::vector<std::pair<std::string, Ramp>> overlays;
        std::string fillTitle;                  // "Wind speed (kt)"
        std::function<std::string(int hour)> fillTitleFor;   // when the title depends on the hour (a precipitation period)
        Quantity quantity{Quantity::Other};
        double legendStep{10.0};                // in displayed units; 0: a tick at every ramp stop
        std::vector<ContourSet> contours;
        // wind barbs (m/s grids), drawn in knots
        std::string barbU, barbV;
        // streamlines of a wind (m/s grids)
        std::string streamU, streamV;
    };
    const std::vector<Product>& products();
    const Product * product(const std::string& id, const std::string& source = "GFS");
    // What can be ticked onto a chart as well: lines (the sea level pressure, the thickness, the heights of a level, the 850 mb temperature) and one set of wind barbs. Each is its own
    // records, so any base chart can have them: "this and that". group is "Lines" or "Wind barbs".
    struct OverlayChoice {
        std::string id;
        std::string label;
        std::string group;
    };
    std::vector<OverlayChoice> overlayChoices(const std::string& source);
    // the base chart with the ticked overlays added (the barbs of the last one ticked replace the base's); the base itself when none applies
    Product compose(const Product& base, const std::vector<std::string>& overlayIds);

    // the names for under a chart, and the sectors a model can show (the blend covers only the contiguous United States)
    std::string sourceLabel(const std::string& source);
    std::vector<std::string> sectorIds(const std::string& source);

    struct Options {
        int width{1100};
        const GfsClimate * climate{nullptr};   // for the anomaly charts
        bool fahrenheit{true};                 // the user's US units: degrees F, inches
        std::vector<std::vector<std::pair<float, float>>> lines;   // coastlines and borders as (longitude, latitude)
    };
    // the records a product needs for a forecast hour
    std::vector<GfsData::Need> needs(const Product& product, int hour);
    std::vector<GfsData::Need> fallbackNeeds(const Product& product, int hour);   // empty when there is none
    // The finished picture, with a header (model, product, run and valid times) and a legend. Null when a needed grid is missing. The grids are as fetched; the product derives its own (a precipitation period, a thickness) from them.
    QImage render(const Product& product, const Sector& sector, const Grids& grids, const GfsData::Run& run, int forecastHour, const Options& options);
}

#endif  // GFSCHART_H
