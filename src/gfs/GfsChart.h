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
    // A square of a number of kilometers a side round a place ("LOCAL:35.47,-97.52,600,Oklahoma City": latitude, longitude, kilometers, the name). The square is moved, not shrunk, to stay inside
    // the box a model covers (a city near the edge of the regional grid is as near its center as the grid lets it be); too big for the grid, it is the grid.
    std::string localId(double lat, double lon, double km, const std::string& name);
    bool parseLocalId(const std::string& id, double& lat, double& lon, double& km, std::string& name);
    Sector localSector(const std::string& id, double west, double south, double east, double north);   // the model's coverage; the sector has the id's own id
    // An exact box ("BOX:-100.5,30,-80,45": west, south, east, north): the view of the scrollable map, which a chart is then drawn for. Longitudes within -180 .. 180 (west < east), latitudes within +-85.
    std::string boxId(double west, double south, double east, double north);
    bool parseBoxId(const std::string& id, Sector& out);

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
    // The user's own scales (the palette editor): a scale replaces every default one with the same stop values, so one edit changes all the charts of a kind (every temperature chart).
    // rampKey names a default scale; "" for a scale with no stops.
    std::string rampKey(const Ramp& ramp);
    struct Product;
    Ramp defaultRamp(const Product& product, bool magColors);   // the scale render() uses for the chart before the user's own: the model guidance site's bands when it has them and they are on
    // the text of a scale and back, for keeping it and for the import and export files: a "banded" line, then "value red green blue alpha" per stop
    std::string rampToText(const Ramp& ramp);
    bool rampFromText(const std::string& text, Ramp& ramp, std::string& error);

    using Grids = std::map<std::string, GfsGrid::Grid>;
    // What the fill's numbers are, for showing them in the user's units (the grids are in degrees C, millimeters, centimeters and so on)
    enum class Quantity { Other, Temperature, Millimeters, Centimeters, Meters };
    // a number of the grids in the user's units (degrees F, inches, feet when `us`), the other way, and the unit's name ("" for none)
    double shownValue(Quantity quantity, double value, bool us);
    double storedValue(Quantity quantity, double shown, bool us);
    std::string unitLabel(Quantity quantity, bool us);

    // what a product's own derivations may need: the hour, the run (for the valid date) and the climatology
    struct Context {
        int hour;
        GfsData::Run run;
        const GfsClimate * climate;
        std::string source;   // the model the chart is of ("CFS": its anomalies are from the CFS's own normal)
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
    // The chart's contour lines, their numbers and the highs and lows as vectors, for a map that draws them at the zoom and size of the view or the export (not baked into the picture): lines of
    // (longitude, latitude), labels at a place with halo
    struct VectorLines {
        struct Line {
            QColor color;
            double width{1.0};     // pixels
            bool dashed{false};
            std::vector<std::pair<double, double>> lonLat;
        };
        struct Label {
            double lon{0}, lat{0};
            QString text;
            QColor color;
        };
        std::vector<Line> lines;
        std::vector<Label> labels;
    };
    struct ContourOut {   // a set of lines with the grid it is drawn from, for whoever draws the fill itself (the high resolution world export)
        ContourSet set;
        GfsGrid::Grid grid;
    };
    struct Product {
        std::string pickerGroup;   // the group of the chart picker when it is the source's own (SPC REFS: SPC's groups); empty: worked out from the chart's id
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
        // the wind of more levels, ticked onto the chart: barbs at the same points as the chart's own, each set in a color of its own, stacked on one origin (shorter staffs on top, so a wind that is the same at
        // two levels shows both); `name` is what the header calls it
        struct BarbSet {
            std::string u, v;
            QColor color;
            std::string name, colorName;
        };
        std::vector<BarbSet> barbSets;
        // streamlines of a wind (m/s grids)
        std::string streamU, streamV;
        // Numbers printed over the fill, side by side at each point ("41 63"): the grids named here (made by `derive`), in the chart's `quantity` and the user's units. Placed at
        // Options::places (cities), biggest first and never on one another, then evenly over what is left of the map (oceans, outside the US). Each grid is also offered in the hover read-out.
        struct PointValue {
            std::string key;
            QColor color;
            std::string title;   // for the hover read-out: "10th percentile"
        };
        std::vector<PointValue> pointValues;
        // An anomaly chart that can also be drawn in standard deviations (withStandardDeviations): `sigma` makes the grids of that view (the fill "anom" in deviations, and the two named
        // below in the chart's own unit). Set only on the charts that have a climatology with deviations.
        std::function<void(Grids&, const Context&)> sigma;
        // The chart's fields are made only by the runs whose hour is a multiple of this (the NBM: 6 for the 6 and 12 hour chances, 12 for the 12 hour extremes; the hourly runs have the 1 hour
        // fields only). 0: every run has them.
        int mainCycleStep{0};
        int periodEndHourUtc{-1};   // with periodHours: the period must end at this hour of the day (the NBM's night minimum ends at 12Z, the day maximum at 00Z); -1: any
        // A chart of members pooled from several ensembles (the ECWMF's, the GEFS and the GEPS): the field each member provides, read by GfsRender (pool) onto one grid, as "<prefix><tag>" for every
        // member of poolMembers(); the chart's own derive and contours work on those keys.
        struct Pool {
            std::string variable, level;
            std::string prefix;   // "f": the keys are "fI01" ...
            std::string only;     // only the members of this ensemble ("IFS-ENS", "GEFS", "GEPS"); "": all of them (withPoolOnly)
            int periodHours{-1};  // precipitation ("APCP"): the amount in the period of that many hours ending at the valid time (0: the total since the start of the member's run); -1: not an amount
            bool active() const { return !variable.empty(); }
        } pool;
        int periodHours{0};   // the chart is of a period that ends at the hour (12 hour extremes, 6 hour chances): only the forecast hours that are multiples of this have it. 0: any hour
        std::string sigmaUnit;      // the chart's own unit for the legend's second row and the read-out: "dam", "mb"
        std::string sigmaSizeKey;   // the grid of one standard deviation in that unit: the legend's second row uses its middle value
        std::string sigmaReadKey;   // the grid of the anomaly in that unit: drawn as dashed lines over the deviations, and in the hover read-out
        double sigmaInterval{0.0};  // the spacing of those lines, in that unit
    };
    // The members of the ensembles that are pooled: the model they come from, the name that model's files know the member by, and a tag that is its key ("I01" the ECMWF's member 1, "G00" the GEFS
    // control, "C05" the GEPS member 5)
    struct PoolMember {
        std::string source, stat, tag;
    };
    const std::vector<PoolMember>& poolMembers();
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
    // One member of the ensemble instead of the mean: the choices a chart offers (empty when it cannot show a single member: spread, probability and counting charts) and the chart for one.
    // `value` is the member's name in the model's files ("" the mean, "c00" the control and "p01" ... "p30" in GEFS; "m001" ... "m005" in REFS).
    struct MemberChoice {
        std::string value;
        std::string label;
    };
    // runId ("yyyyMMddHH") names the run the members are of: SPC REFS names its members in each run's store
    std::vector<MemberChoice> memberChoices(const Product& product, const std::string& runId = {});
    Product withMember(const Product& product, const std::string& member, const std::string& runId = {});
    // The anomaly chart in standard deviations from the 1991-2020 mean (the legend gives the unit's equivalents too); the chart itself when it has none (see Product::sigma)
    bool hasStandardDeviations(const Product& product);
    Product withStandardDeviations(const Product& product);
    // A pooled chart (ENS-ALL) drawn from the members of one of its ensembles only, to compare that ensemble with the pool: same grid, same valid time, same chart. Titles say so.
    Product withPoolOnly(const Product& product, const std::string& source);

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
        bool place(double lon, double lat, double& fx, double& fy) const;
        bool placeAnywhere(double lon, double lat, double& fx, double& fy) const;   // the same, beyond the map too (a shape that runs off the edge)   // where a longitude and latitude are on the picture as fractions of it (the inverse of locate); false outside the map
        QString read(double fx, double fy) const;   // empty outside the map
    };

    struct Options {
        // a chart of something made from the chart's own fill (a maximum over hours, a change since an earlier run): this replaces the fill, and the lines and barbs go
        GfsGrid::Grid fillOverride;
        Ramp overrideRamp;                     // empty: the chart's own scale
        std::string overrideNote;              // "Change since the run 12 h earlier (F)": goes before the title
        Quantity overrideQuantity{Quantity::Other};
        Probe * probe{nullptr};                // when set, filled in by render() for the hover read-out
        GfsGrid::Grid * fillOut{nullptr};      // when set: render() makes no picture, it hands over the fill's grid (and the scale in rampOut) for something that draws it another way (the globe)
        Ramp * rampOut{nullptr};
        GfsGrid::Grid * windUOut{nullptr}, * windVOut{nullptr};   // with fillOut: the chart's wind (m/s) too, for barbs drawn by whoever draws the fill
        std::vector<ContourOut> * contoursOut{nullptr};           // with fillOut: the chart's line sets and their grids
        VectorLines * vectorOut{nullptr};                          // the contour lines, their numbers and the highs / lows are handed over here instead of being painted into the picture
        bool noBarbs{false};                   // the picture has no barbs (the map layer draws them itself, as vectors on a world grid; the probe still holds the wind)
        int width{1100};
        double scale{1.0};                     // a picture this many times larger in each direction, drawn at the same design (text, lines and barbs scaled, the fill computed at the larger size): for exports
        const GfsClimate * climate{nullptr};   // for the anomaly charts
        bool ticks{false};                     // latitude and longitude marks along the frame, numbered
        bool hasMarker{false};                 // a mark at a place (the saved location a local area is round) with its name
        double markerLat{0}, markerLon{0};
        std::string markerName;
        bool fahrenheit{true};                 // the user's US units: degrees F, inches
        bool magColors{true};                  // the charts that have one use the model guidance site's own color bands (drawn at our resolution)
        std::map<std::string, Ramp> palettes;  // the user's scales by rampKey of the default one they replace
        std::vector<std::vector<std::pair<float, float>>> lines;   // coastlines and borders as (longitude, latitude)
        std::vector<std::pair<float, float>> places;               // cities as (longitude, latitude), biggest first: where a chart's point numbers (Product::pointValues) go first
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
