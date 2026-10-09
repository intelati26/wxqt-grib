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
#include <QDateTime>
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
    // The view of a storm-following grid: the box that holds its valid values (the grid is filled only in a tilted footprint), a little outside
    Sector gridSector(const std::string& id, const GfsGrid::Grid& grid);
    const Sector * sector(const std::string& id);

    // A color scale by value: stops are (value, color), linear between them. Values below the first or above the last take the end colors.
    struct Ramp {
        std::vector<std::pair<double, QColor>> stops;
        // banded: each stop's color holds from its value up to the next stop (the flat bands of the model guidance site), and nothing is drawn below the first
        bool banded{false};
        QRgb at(double value) const;
    };
    // The scale the model guidance site (MAG) uses for a kind of chart, by name ("precip", "isotach", "radar_rain", "rh", "spread_mslp", "spread_wind", "helicity", "uh"): flat bands read from
    // its legends (resourceCreation/mag/palettes.py), in the units of our fills. Null when there is none of that name.
    const Ramp * magPalette(const std::string& name);

    using Grids = std::map<std::string, GfsGrid::Grid>;
    // What the fill's numbers are, for showing them in the user's units (the grids are in degrees C, millimeters, centimeters and so on)
    enum class Quantity { Other, Temperature, Millimeters, Centimeters, Meters };

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
        double minimum{-1e30};          // no line below this level (the 30, 60 and 90 per cent lines of a chance, not a 0 one round every member)
        std::string title;              // "Height (dam)"
        QColor color{30, 30, 30};
        QColor colorBelow;              // lines under the split value in this color instead (a thickness line for the rain / snow edge); invalid = not used
        double split{0.0};
        bool dashed{false};
        bool highsAndLows{false};
        bool onlyBelowZero{false};      // omega: the rising air only
        double smoothKm{0.0};           // smooth the field to about this radius before the lines are drawn (omega is noisy at a storm model's 2 km)
        bool labelled{true};            // a number on the line (off for a line of each of 31 members)
        bool uniform{false};            // every line the same weight (no heavy line at every fifth level)
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
        bool linesOnly{false};                  // no fill and no color bar, only the lines (the members' contours): a key line stands in for the legend
        std::string palette;                    // the name of the model guidance site's scale for this chart ("" none): used when Options.magColors is on
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
    // the group a product is listed under in the picker ("Upper air", "Surface", "Precipitation and moisture", "Storms and severe", ...)
    std::string category(const Product& product);
    const Product * product(const std::string& id, const std::string& source = "GFS");
    // Charts made on request: a template (the chance of rain over a limit in a period) with the user's values. Each setting has the values offered and the one to start from.
    struct Setting {
        std::string key, label, unit;
        std::vector<double> choices;
        double standard{0};
        std::vector<std::string> names;   // what each choice is called, when a number is not it (the precipitation types)
    };
    struct Template {
        std::string id, title;
        std::vector<Setting> settings;
    };
    std::vector<Template> templates(const std::string& source);                                      // none for a model that cannot count members
    std::string generatedChart(const std::string& templateId, const std::vector<double>& values);   // the chart's id: product() makes the chart from it
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

    // One point of a forecast track (the hurricane model's ATCF file): position, intensity and the radii of 34, 50 and 64 kt wind in nautical miles by quadrant (NE, SE, SW, NW; 0: none)
    struct TrackPoint {
        int hour{0};
        double lat{0.0}, lon{0.0};     // degrees, west negative
        int wind{0}, pressure{0};      // knots, millibars
        double radii[3][4]{};
    };
    // The view narrowed to the track up to `hour` (and `margin` degrees round it), kept inside `within`: for the charts whose grid is far wider than the storm
    Sector cropToTrack(const Sector& within, const std::vector<TrackPoint>& track, int hour, double margin);
    // the points of an ATCF "a-deck" style track file (one line per wind threshold), by forecast hour; empty when it holds none
    std::vector<TrackPoint> parseTrack(const std::string& text);

    // What a point of a drawn chart holds, for a hover read-out: the chart's own fields (kept as drawn, after derivation) and where the picture's map is. read() takes a point of the
    // picture as fractions (0..1) of its width and height and gives the text: the position, the fill's value, each line's value, the wind.
    struct Probe {
        struct Field {
            std::string label;      // "Wind speed (kt)"
            GfsGrid::Grid grid;
            double scale{1.0};      // the number shown = value * scale (before the units)
            Quantity quantity{Quantity::Other};
            bool convert{false};    // shown in the user's units
        };
        double west{0}, east{0}, south{0}, north{0};
        double areaLeft{0}, areaTop{0}, areaWidth{0}, areaHeight{0}, imageWidth{1}, imageHeight{1};
        bool us{true};
        std::vector<Field> fields;
        GfsGrid::Grid windU, windV;     // m/s
        QDateTime validUtc;             // the valid time of the picture (for a sounding at a clicked point)
        size_t memory() const { size_t n = (windU.values.size() + windV.values.size()) * sizeof(float); for (const auto& f : fields) n += f.grid.values.size() * sizeof(float); return n; }   // bytes held
        bool valid() const { return areaWidth > 0.0 && !fields.empty(); }
        bool locate(double fx, double fy, double& lon, double& lat) const;   // the point of the picture as longitude and latitude; false outside the map
        QString read(double fx, double fy) const;   // empty outside the map
    };

    struct Options {
        // a chart of something made from the chart's own fill (a maximum over hours, a change since an earlier run): this replaces the fill, and the lines and barbs go
        GfsGrid::Grid fillOverride;
        Ramp overrideRamp;                     // empty: the chart's own scale
        std::string overrideNote;              // "Change since the run 12 h earlier (F)": goes before the title
        Quantity overrideQuantity{Quantity::Other};
        Probe * probe{nullptr};                // when set, filled in by render() for the hover read-out
        int width{1100};
        const GfsClimate * climate{nullptr};   // for the anomaly charts
        bool fahrenheit{true};                 // the user's US units: degrees F, inches
        bool magColors{true};                  // the charts that have one use the model guidance site's own color bands (drawn at our resolution)
        std::vector<std::vector<std::pair<float, float>>> lines;   // coastlines and borders as (longitude, latitude)
        std::vector<TrackPoint> track;         // a storm track drawn on the chart, with the wind radii of the forecast hour shown
        bool windRadii{true};                  // the 34 / 50 / 64 kt quadrant wind field of the track's forecast hour
        std::vector<TrackPoint> trackOther;    // another model's track for the same storm, dashed, to compare
        std::string trackName, trackOtherName; // what each is called in the key
    };
    // the fill of a chart for a run and hour, from the grids fetched for it (what render() would paint): for a maximum over hours or a change between runs
    GfsGrid::Grid fillOf(const Product& product, const Grids& fetched, const GfsData::Run& run, int forecastHour, const GfsClimate * climate);
    // a change between two fills in the user's units (a difference of temperatures is not shifted by 32): the grid scaled, and the unit it is in
    GfsGrid::Grid deltaInUserUnits(const GfsGrid::Grid& delta, Quantity quantity, bool us, std::string& unit);
    // the records a product needs for a forecast hour
    std::vector<GfsData::Need> needs(const Product& product, int hour);
    std::vector<GfsData::Need> fallbackNeeds(const Product& product, int hour);   // empty when there is none
    // The finished picture, with a header (model, product, run and valid times) and a legend. Null when a needed grid is missing. The grids are as fetched; the product derives its own (a precipitation period, a thickness) from them.
    QImage render(const Product& product, const Sector& sector, const Grids& grids, const GfsData::Run& run, int forecastHour, const Options& options);
}

#endif  // GFSCHART_H
