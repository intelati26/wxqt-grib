// Tests of the climatology numerics: the calendar, the file's band order, the stored days, the packing and the smooth upsampling.
#include <cmath>
#include <cstdio>
#include "gfs/GfsClimate.h"
#include "gfs/GfsData.h"
#include "gfs/GfsGrid.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    // the calendar: 365 days, 29 February is 28 February, 1 March is day 59 whatever the year
    CHECK(GfsClimate::dayIndex(2026, 1, 1) == 0);
    CHECK(GfsClimate::dayIndex(2026, 10, 8) == 280);
    CHECK(GfsClimate::dayIndex(2026, 12, 31) == 364);
    CHECK(GfsClimate::dayIndex(2028, 2, 29) == GfsClimate::dayIndex(2028, 2, 28));
    CHECK(GfsClimate::dayIndex(2028, 3, 1) == 59 && GfsClimate::dayIndex(2026, 3, 1) == 59);
    CHECK(GfsClimate::dayIndex(2028, 12, 31) == 364);

    // the band: surface files one per day; pressure files with the level varying fastest (checked against the file: band 1 = 1000 mb, band 2 = 925 mb, band 18 = 1000 mb a day later)
    const auto z500 = GfsClimate::height(500);
    CHECK(z500.level == 5 && z500.variable == "hgt");
    CHECK(GfsClimate::band(GfsClimate::height(1000), 0) == 1 && GfsClimate::band(GfsClimate::height(925), 0) == 2 && GfsClimate::band(GfsClimate::height(1000), 1) == 18);
    CHECK(GfsClimate::band(z500, 280) == 280 * 17 + 6);
    CHECK(GfsClimate::band(GfsClimate::seaLevelPressure(), 280) == 281);
    CHECK(GfsClimate::height(555).file.empty());

    // the stored days: every fourth, blended; the last wraps to the first
    int a, b;
    double w;
    GfsClimate::anchors(280, a, b, w);
    CHECK(a == 280 && b == 284 && w == 0.0);
    GfsClimate::anchors(282, a, b, w);
    CHECK(a == 280 && b == 284 && std::abs(w - 0.5) < 1e-9);
    GfsClimate::anchors(363, a, b, w);
    CHECK(a == 360 && b == 364 && std::abs(w - 0.75) < 1e-9);
    GfsClimate::anchors(364, a, b, w);
    CHECK(a == 364 && b == 0 && w == 0.0);

    // packing: a coarse global field comes back to within the 16 bit step, however it was shaped
    GfsGrid::Grid g;
    g.columns = 144;
    g.rows = 73;
    g.lon0 = 0.0;
    g.lat0 = 90.0;
    g.step = 2.5;
    g.values.resize(144 * 73);
    for (int r = 0; r < 73; r++) {
        for (int c = 0; c < 144; c++) {
            g.values[static_cast<size_t>(r) * 144 + static_cast<size_t>(c)] = static_cast<float>(5500.0 + 300.0 * std::cos(r * 2.5 * 3.14159265 / 180.0) + 40.0 * std::sin(c * 2.5 * 3.14159265 / 90.0));
        }
    }
    const auto packed = GfsClimate::pack(g);
    GfsGrid::Grid back;
    CHECK(GfsClimate::unpack(packed, back));
    CHECK(back.columns == 144 && back.rows == 73 && back.step == 2.5 && back.lat0 == 90.0);
    double worst = 0.0;
    for (size_t i = 0; i < g.values.size(); i++) {
        worst = std::max(worst, static_cast<double>(std::abs(g.values[i] - back.values[i])));
    }
    CHECK(worst < 0.01);   // the range is about 700 m over 65536 steps
    CHECK(!GfsClimate::unpack(packed.substr(0, 100), back));

    // smooth upsampling: cubic goes through the points and follows a smooth field far better than bilinear
    CHECK(std::abs(g.sampleCubic(5.0, 40.0) - g.at(2, 20)) < 1e-3);
    double cubicError = 0.0, linearError = 0.0;
    for (double lon = 3.0; lon < 300.0; lon += 0.37) {
        for (double lat = -60.0; lat < 60.0; lat += 0.41) {
            const double truth = 5500.0 + 300.0 * std::cos((90.0 - lat) * 3.14159265 / 180.0 * 0.0 + (90.0 - lat) * 3.14159265 / 180.0) + 40.0 * std::sin(lon * 3.14159265 / 90.0);
            cubicError = std::max(cubicError, std::abs(g.sampleCubic(lon, lat) - truth));
            linearError = std::max(linearError, std::abs(g.sample(lon, lat) - truth));
        }
    }
    CHECK(cubicError < linearError / 3.0);

    // the two models' files, and how a run is found for each
    {
        const GfsData::Run run{"20261008", "12"};
        const auto gfs = GfsData::gfs(), nbm = GfsData::nbm();
        CHECK(gfs.fileUrl(run, 24, "") == "https://noaa-gfs-bdp-pds.s3.amazonaws.com/gfs.20261008/12/atmos/gfs.t12z.pgrb2.0p25.f024");
        CHECK(nbm.fileUrl(run, 6, "") == "https://noaa-nbm-grib2-pds.s3.amazonaws.com/blend.20261008/12/core/blend.t12z.core.f006.co.grib2");
        CHECK(gfs.cycleHours == 6 && gfs.probeHour == 0 && !gfs.warp.enabled && nbm.cycleHours == 1 && nbm.probeHour == 1 && nbm.warp.enabled);
        {
            const auto ge = GfsData::gefs();
            const GfsData::Run r{"20261008", "12"};
            CHECK(ge.fileUrl(r, 24, "avg-a") == "https://noaa-gefs-pds.s3.amazonaws.com/gefs.20261008/12/atmos/pgrb2ap5/geavg.t12z.pgrb2a.0p50.f024");
            CHECK(ge.fileUrl(r, 24, "spr-s") == "https://noaa-gefs-pds.s3.amazonaws.com/gefs.20261008/12/atmos/pgrb2sp25/gespr.t12z.pgrb2s.0p25.f024");
            CHECK(ge.fileOf({"s", "TMP", "500 mb", "", "", "spr"}) == "spr-a");
            CHECK(ge.fileOf({"c", "CAPE", "surface", "", "", ""}) == "avg-s");
            CHECK(!GfsGrid::find(GfsGrid::parseIdx("1:0:d=2026100812:HGT:500 mb:72 hour fcst:ens mean\n"), "HGT", "500 mb", "", "*") == false);
        }
        {
            const auto h = GfsData::hafs("HAFSA", "09l");
            const GfsData::Run r{"20261008", "18"};
            CHECK(h.fileUrl(r, 24, "storm.atm") == "https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.20261008/18/09l.2026100818.hfsa.storm.atm.f024.grb2");
            CHECK(h.fileUrl(r, 24, "ww3") == "https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfsa.20261008/18/09l.2026100818.hfsa.ww3.grb2");
            CHECK(GfsData::hafs("HAFSB", "18e").fileUrl(r, 6, "storm.sat").find("hfsb.20261008/18/18e.2026100818.hfsb.storm.sat.f006.grb2") != std::string::npos);
            CHECK(h.fileOf({"s", "var discipline=3 center=7 local_table=1 parmcat=192 parm=65", "top of atmosphere", "", "", ""}) == "storm.sat");
            CHECK(h.fileOf({"h", "HTSGW", "surface", "", "", "ww3"}) == "ww3");
            CHECK(h.fileOf({"u", "UGRD", "10 m above ground", "", "", ""}) == "storm.atm");
            CHECK(h.id == "HAFSA-09l");
        }
        const auto ai = GfsData::aigfs();
        CHECK(ai.fileUrl(run, 24, "pres") == "https://noaa-nws-graphcastgfs-pds.s3.amazonaws.com/aigfs.20261008/12/model/atmos/grib2/aigfs.t12z.pres.f024.grib2");
        CHECK(ai.fileUrl(run, 6, "sfc").find("aigfs.t12z.sfc.f006.grib2") != std::string::npos);
        CHECK(ai.fileOf({"k", "HGT", "500 mb", "", ""}) == "pres" && ai.fileOf({"k", "TMP", "2 m above ground", "", ""}) == "sfc" && ai.fileOf({"k", "PRMSL", "mean sea level", "", ""}) == "sfc");
        for (const auto& source : {gfs, nbm}) {
            std::vector<std::string> asked;
            GfsData::Config none;
            none.bytes = [&asked] (const std::string& url, long long, long long) { asked.push_back(url); return QByteArray{}; };
            const GfsData nothingThere{none, source};
            GfsData::Run missing;
            const bool foundNone = nothingThere.latestRun(missing);
            CHECK(!foundNone);
            CHECK(static_cast<int>(asked.size()) == source.cyclesToTry);   // nothing there: every cycle tried, no run
            asked.clear();
            GfsData::Config all;
            all.bytes = [&asked] (const std::string& url, long long, long long) { asked.push_back(url); return QByteArray{"1:0:d=2026100812:TMP:2 m above ground:anl:\n"}; };
            const GfsData everythingThere{all, source};
            GfsData::Run found;
            const bool foundOne = everythingThere.latestRun(found);
            CHECK(foundOne);
            CHECK(asked.size() == 1 && asked[0].compare(asked[0].size() - 4, 4, ".idx") == 0 && asked[0].find(source.id == "NBM" ? "f001" : "f000") != std::string::npos);   // the probe hour's index
            CHECK(found.cycle.size() == 2 && found.date.size() == 8 && std::stoi(found.cycle) % source.cycleHours == 0);
        }
    }
    // an anomaly of a fine field against the coarse one it was made from is nothing
    GfsGrid::Grid fine;
    fine.columns = 1440;
    fine.rows = 721;
    fine.lon0 = 0.0;
    fine.lat0 = 90.0;
    fine.step = 0.25;
    fine.values.resize(1440 * 721);
    for (int r = 0; r < 721; r++) {
        for (int c = 0; c < 1440; c++) {
            const double lat = 90.0 - r * 0.25, lon = c * 0.25;
            fine.values[static_cast<size_t>(r) * 1440 + static_cast<size_t>(c)] = static_cast<float>(5500.0 + 300.0 * std::cos((90.0 - lat) * 3.14159265 / 180.0) + 40.0 * std::sin(lon * 3.14159265 / 90.0) + 25.0);
        }
    }
    const auto anom = GfsGrid::anomaly(fine, g);
    CHECK(std::abs(anom.at(720, 360) - 25.0f) < 2.0f);   // a 25 m departure stands out (the coarse field's own error is a meter or two)
    std::printf(failures ? "%d failures\n" : "all climatology tests passed\n", failures);
    return failures ? 1 : 0;
}
