// Tests of the options of a picture (GfsRender::chartOptions): from the settings, the area and the request alone, with no settings file and no network. The marks along the frame by the
// MODEL_TICKS setting and the area, the ring of a saved location and its switch, the mark of the maximum, the units, a bare chart (the map layer's) with none of the frame's marks, the size of
// the picture and what the request hands back.
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include "gfs/GfsRender.h"
#include "util/Utility.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

using GfsRender::OptionPrefs;
using GfsRender::Variant;

static GfsChart::Options make(const OptionPrefs& prefs, const std::string& sector, const Variant& v = {}, GfsChart::Probe * probe = nullptr, GfsChart::Probe * bare = nullptr) {
    return GfsRender::chartOptions(prefs, sector, 35.5, -97.5, "Place", v, probe, bare);
}

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_options_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    // the settings, read: the defaults, then what is written
    {
        const auto d = OptionPrefs::read();
        CHECK(d.ticks == "local" && d.placeMark && !d.maxMark && d.windRadii);
        Utility::writePref("MODEL_TICKS", "all");
        Utility::writePref("MODEL_PLACE_MARK", "false");
        Utility::writePref("MODEL_MAX_MARK", "true");
        Utility::writePref("HAFS_RADII", "false");
        Utility::writePref("MAG_COLORS", "false");
        const auto w = OptionPrefs::read();
        CHECK(w.ticks == "all" && !w.placeMark && w.maxMark && !w.windRadii && !w.magColors);
    }
    const std::string local = GfsChart::localId(35.5, -97.5, 900.0, "Place");
    OptionPrefs prefs;   // the defaults
    // the marks along the frame: "local" only on an area round a saved location, "all" everywhere, "off" nowhere
    CHECK(make(prefs, local).ticks && !make(prefs, "CONUS").ticks);
    prefs.ticks = "all";
    CHECK(make(prefs, local).ticks && make(prefs, "CONUS").ticks);
    prefs.ticks = "off";
    CHECK(!make(prefs, local).ticks && !make(prefs, "CONUS").ticks);
    prefs.ticks = "local";
    // the ring: on a saved location's area, with its place and name, unless the switch is off
    {
        const auto o = make(prefs, local);
        CHECK(o.hasMarker && o.markerLat == 35.5 && o.markerLon == -97.5 && o.markerName == "Place");
        CHECK(!make(prefs, "CONUS").hasMarker);
        prefs.placeMark = false;
        CHECK(!make(prefs, local).hasMarker);
        prefs.placeMark = true;
    }
    // the maximum, the units, the color bands, the wind radii
    prefs.maxMark = true;
    prefs.fahrenheit = false;
    prefs.magColors = false;
    prefs.windRadii = false;
    {
        const auto o = make(prefs, "CONUS");
        CHECK(o.markMax && !o.fahrenheit && !o.magColors && !o.windRadii);
        CHECK(!make(OptionPrefs{}, "CONUS").markMax);
    }
    // the size of the picture and the legend
    {
        Variant v;
        CHECK(make(prefs, "CONUS", v).width == GfsChart::Options{}.width);
        v.width = 1234;
        v.scale = 2.5;
        v.legendFit = true;
        v.legendRange = true;
        v.legendLow = -3.0;
        v.legendHigh = 7.0;
        const auto o = make(prefs, "CONUS", v);
        CHECK(o.width == 1234 && o.scale == 2.5 && o.legendFit && o.legendRange && o.legendLow == -3.0 && o.legendHigh == 7.0);
    }
    // what the request hands back, and the probe: the caller's, or a bare chart's own
    {
        GfsGrid::Grid fill;
        GfsChart::Probe own, bare;
        Variant v;
        v.fillOut = &fill;
        v.noBarbs = true;
        auto o = make(prefs, "CONUS", v);
        CHECK(o.fillOut == &fill && o.noBarbs && o.probe == nullptr);
        o = make(prefs, "CONUS", v, &own, &bare);
        CHECK(o.probe == &own);
        v.bare = true;
        CHECK(make(prefs, "CONUS", v, nullptr, &bare).probe == &bare);
        CHECK(make(prefs, "CONUS", v, &own, &bare).probe == &own);
    }
    // a bare chart (the map layer's): no marks along the frame and no ring, whatever the settings
    {
        Variant v;
        v.bare = true;
        prefs.ticks = "all";
        const auto o = make(prefs, local, v);
        CHECK(!o.ticks && !o.hasMarker);
    }
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
