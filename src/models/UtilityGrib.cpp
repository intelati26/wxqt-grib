// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityGrib.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <QByteArray>
#include <QColor>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QProcess>
#include <QTextStream>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include "common/GlobalVariables.h"
#include "objects/LatLon.h"
#include "objects/URL.h"
#include "radar/CitiesExtended.h"
#include "radar/CityExt.h"
#include "radar/Metar.h"
#include "util/WfoSites.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

namespace {
    // ---------------------------------------------------------------------
    // RRFS/REFS cut over from "para" (parallel/pre-operational) to "prod"
    // (operational) on NOMADS. NET (no earlier than) 2026-10-14 (this date
    // has already slipped once - if rrfsStream = "para" below stops
    // returning data before then, that's the signal it happened early).
    // THE ONLY CHANGE NEEDED AT CUTOVER: flip the line below from "para" to
    // "prod". Nothing else in this file depends on which stream it is - idx
    // probing, subh fallback, run discovery etc. are all written generically
    // against whatever baseDir points at.
    const string rrfsStream{"para"};   // <-- flip to "prod" on/after 2026-10-14
    // ---------------------------------------------------------------------
    const string baseDir{"https://nomads.ncep.noaa.gov/pub/data/nccf/com/rrfs/" + rrfsStream};

    const string dewColorMap{
        "-30 120 90 60\n" "-10 150 130 90\n" "0 205 195 135\n" "5 175 205 135\n"
        "10 110 190 130\n" "15 70 170 120\n" "18 60 150 150\n" "21 55 130 180\n"
        "24 70 100 190\n" "30 95 70 170\n"};

    const string rhColorMap{
        "0 150 100 55\n" "20 190 160 90\n" "40 220 210 150\n"
        "60 150 200 140\n" "80 70 170 100\n" "100 30 110 70\n"};

    const string windColorMap{
        "0 0 0 0 0\n" "2 200 230 245\n" "6 120 200 235\n" "10 90 180 120\n"
        "15 240 220 90\n" "20 245 150 50\n" "28 230 60 50\n" "38 180 40 160\n"
        "50 255 255 255\n"};

    const string echoTopColorMap{
        "-5001 0 0 0 0\n" "2999 0 0 0 0\n" "3000 120 210 235\n" "6000 60 150 230\n"
        "9000 40 200 60\n" "12000 240 220 80\n" "15000 240 130 40\n" "18000 230 50 50\n"};

    const string uphlColorMap{
        "0 0 0 0 0\n" "24 0 0 0 0\n" "25 120 200 235\n" "50 70 170 120\n"
        "75 240 220 90\n" "100 240 150 50\n" "150 235 60 50\n" "250 200 40 160\n"
        "400 255 255 255\n"};

    const string updraftColorMap{
        "0 0 0 0 0\n" "2 0 0 0 0\n" "3 150 210 235\n" "6 90 180 130\n"
        "10 240 220 90\n" "16 240 150 50\n" "25 235 60 50\n" "40 200 40 160\n"};

    const string capeColorMap{
        "0 0 0 0 0\n" "100 0 0 0 0\n" "250 200 230 245\n" "500 120 200 235\n"
        "1000 110 190 120\n" "1500 240 225 100\n" "2500 245 150 50\n"
        "3500 230 60 50\n" "5000 190 40 150\n" "6500 255 255 255\n"};

    const string cinColorMap{
        "-500 40 0 60\n" "-200 90 30 130\n" "-100 130 70 180\n" "-50 175 130 210\n"
        "-25 210 190 235\n" "0 0 0 0 0\n"};

    const string srhColorMap{
        "0 0 0 0 0\n" "49 0 0 0 0\n" "50 190 225 245\n" "100 120 200 235\n"
        "150 110 190 120\n" "250 240 225 100\n" "350 245 150 50\n"
        "500 230 60 50\n" "700 200 40 150\n"};

    const string pwatColorMap{
        "0 120 90 60\n" "10 180 160 110\n" "20 210 205 150\n" "30 150 200 140\n"
        "40 90 180 130\n" "50 55 150 140\n" "60 50 120 180\n" "70 70 90 180\n"
        "85 100 60 170\n"};

    const string precipColorMap{
        "0 0 0 0 0\n" "0.4 0 0 0 0\n" "0.5 150 220 235\n" "2 90 180 230\n"
        "6 60 190 70\n" "12 240 225 90\n" "25 240 150 45\n" "50 230 55 50\n"
        "100 190 40 150\n" "200 255 255 255\n"};

    const string visColorMap{
        "0 200 40 40\n" "400 235 120 45\n" "800 240 190 70\n" "1600 240 235 120\n"
        "3200 180 215 150\n" "6400 150 210 220\n" "12000 205 230 245\n" "16000 0 0 0 0\n"};

    const string cloudColorMap{
        "0 0 0 0 0\n" "10 0 0 0 0\n" "25 220 235 245\n" "50 180 205 225\n"
        "75 140 175 205\n" "100 90 130 175\n"};

    const string snowDepthColorMap{
        "0 0 0 0 0\n" "0.01 0 0 0 0\n" "0.02 210 235 250\n" "0.1 150 200 240\n"
        "0.3 100 150 230\n" "0.6 120 100 210\n" "1 150 60 180\n" "2 90 20 120\n"};

    const string hgt200ColorMap{
        "11400 70 40 130\n" "11700 60 100 190\n" "12000 70 175 165\n"
        "12200 130 200 110\n" "12350 240 225 110\n" "12500 245 155 55\n" "12700 225 65 55\n"};
    const string hgt300ColorMap{
        "8800 70 40 130\n" "9000 60 100 190\n" "9250 70 175 165\n"
        "9450 130 200 110\n" "9600 240 225 110\n" "9750 245 155 55\n" "9950 225 65 55\n"};
    const string hgt500ColorMap{
        "5400 70 40 130\n" "5550 60 100 190\n" "5700 70 175 165\n"
        "5820 130 200 110\n" "5880 240 225 110\n" "5940 245 155 55\n" "6030 225 65 55\n"};
    const string hgt700ColorMap{
        "2700 70 40 130\n" "2850 60 100 190\n" "2950 70 175 165\n"
        "3050 130 200 110\n" "3120 240 225 110\n" "3180 245 155 55\n" "3260 225 65 55\n"};
    const string hgt850ColorMap{
        "1200 70 40 130\n" "1320 60 100 190\n" "1400 70 175 165\n"
        "1470 130 200 110\n" "1520 240 225 110\n" "1560 245 155 55\n" "1650 225 65 55\n"};

    struct RegionEntry {
        const char * label;
        UtilityGrib::Bbox box;   // {0,0,0,0} means "derive from the current location"
    };

    // index 0 = CONUS, index 1 = the user's area, 2+ = SPC mesoanalysis sectors
    const RegionEntry regionTable[]{
        {"CONUS",              {-125.0,  24.0,  -66.5,  50.0}},
        {"My Area",            {   0.0,   0.0,    0.0,   0.0}},
        {"Midwest",            { -98.1,  34.4,  -84.9,  43.6}},
        {"North Central",      {-103.1,  40.0,  -89.9,  49.3}},
        {"Central",            {-102.9,  33.0,  -89.7,  42.2}},
        {"South Central",      {-103.3,  27.2,  -90.1,  36.4}},
        {"Northeast",          { -83.7,  39.1,  -70.5,  48.3}},
        {"Central East",       { -88.5,  32.3,  -75.3,  41.5}},
        {"Southeast",          { -92.2,  26.4,  -79.0,  35.6}},
        {"Southwest",          {-119.1,  31.2, -105.9,  40.4}},
        {"Northwest",          {-119.3,  40.0, -106.1,  49.2}},
        {"Great Lakes",        { -92.5,  39.4,  -79.3,  48.6}},
        {"Intermountain West", {-117.2,  36.4, -104.0,  45.6}},
    };

    constexpr int regionCount = static_cast<int>(sizeof(regionTable) / sizeof(regionTable[0]));

    // the hover sample grid's column count. Rows come along for free from
    // gdal_translate preserving aspect, so total cell count (and therefore
    // sidecar render + parse time) grows with the *square* of this - keep it
    // down for wide regions (CONUS/NA) rather than holding one fixed value
    // for every region regardless of area. Small/zoomed regions (SPC-meso,
    // "My Area") get a higher count since they're both cheap and where the
    // hover read-out actually gets used closely.
    int sampleGridColumns(const UtilityGrib::Bbox& box) {
        const auto span = std::max(box.east - box.west, box.north - box.south);
        if (span > 30.0) {
            return 220;    // CONUS-sized (and any future NA/AK-sized) regions
        }
        if (span > 15.0) {
            return 350;
        }
        return 450;        // SPC-meso / "My Area"
    }

    // RRFS's native grid spacing (all products are "3km" - see the NOMADS
    // filenames throughout this file).
    constexpr double nativeGridKm = 3.0;

    string fixed(double value, int decimals) {
        return QString::number(value, 'f', decimals).toStdString();
    }

    QString fixedQ(double value) {
        return QString::number(value, 'f', 3);
    }

    // shared parser for the app's "*.bin" line-segment resources - a flat
    // list of big-endian float32 groups (lat1, lon1, lat2, lon2). Same
    // format/byte-swap `RadarGeomInfo::loadData()` uses to feed the Nexrad
    // radar screen's geometry-overlay system (statev2.bin, county.bin,
    // hwv4.bin, hwv4ext.bin, lakesv3.bin, ... - see RadarGeometry.cpp's
    // type list), reused here rather than a second hand-rolled parser per
    // caller. `stateLinesGeoJson()`'s own inline version predates this and
    // is the one this was extracted from.
    string binSegmentsToGeoJson(const string& resourceFileName) {
        const auto raw = UtilityIO::readBinaryFileFromResource(GlobalVariables::resDir + resourceFileName);
        const auto size = static_cast<int>(raw.size());
        auto floatAt = [&raw] (int offset) {
            const unsigned char bytes[4]{
                static_cast<unsigned char>(raw[offset + 3]),
                static_cast<unsigned char>(raw[offset + 2]),
                static_cast<unsigned char>(raw[offset + 1]),
                static_cast<unsigned char>(raw[offset]),
            };
            float value = 0.0f;
            std::memcpy(&value, bytes, 4);
            return value;
        };
        string json = R"({"type":"Feature","properties":{},"geometry":{"type":"MultiLineString","coordinates":[)";
        bool first = true;
        for (int index = 0; index + 15 < size; index += 16) {
            const auto lat1 = floatAt(index);
            const auto lon1 = floatAt(index + 4);
            const auto lat2 = floatAt(index + 8);
            const auto lon2 = floatAt(index + 12);
            if (lat1 == lat2 && lon1 == lon2) {
                continue;
            }
            if (lat1 < 10.0f || lat1 > 75.0f || lat2 < 10.0f || lat2 > 75.0f) {
                continue;
            }
            json += first ? "[[" : ",[[";
            first = false;
            json += fixed(-lon1, 4) + "," + fixed(lat1, 4) + "],[" + fixed(-lon2, 4) + "," + fixed(lat2, 4) + "]]";
        }
        json += "]}}";
        return json;
    }

    // rewrite a gdaldem color-relief table from Celsius breakpoints to Fahrenheit
    string colorMapToFahrenheit(const string& colorMap) {
        string out;
        for (const auto& line : QString::fromStdString(colorMap).split('\n', Qt::SkipEmptyParts)) {
            const auto parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.isEmpty()) {
                continue;
            }
            auto rebuilt = QString::number(parts[0].toDouble() * 1.8 + 32.0, 'f', 1);
            for (int i = 1; i < parts.size(); i += 1) {
                rebuilt += " " + parts[i];
            }
            out += rebuilt.toStdString() + "\n";
        }
        return out;
    }

    // the reflectivity colour map is deliberately transparent below ~5 dBZ
    // (alpha=0) for normal overlay use - fine on top of a real basemap, but
    // as a *background* layer that reads as "no data at all", indistinguishable
    // from the true off-domain edges. Rewrite those transparent stops to a
    // flat, distinctive grey so renderBackground() can hatch them afterwards.
    const int greyBackgroundStop = 200;
    string reflectivityToGreyBase(const string& colorMap) {
        string out;
        for (const auto& line : QString::fromStdString(colorMap).split('\n', Qt::SkipEmptyParts)) {
            const auto parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 5 && parts[4].toInt() == 0) {
                out += parts[0].toStdString() + " " + To::string(greyBackgroundStop) + " " +
                    To::string(greyBackgroundStop) + " " + To::string(greyBackgroundStop) + "\n";
            } else {
                out += line.toStdString() + "\n";
            }
        }
        return out;
    }

    // fills every pixel that came out as the flat grey sentinel above (i.e.
    // in-domain but below the reflectivity floor) with a diagonal hatch,
    // leaving real echo colours and the (already transparent) off-domain
    // edges untouched
    void hatchLowReflectivity(QImage& image) {
        image = image.convertToFormat(QImage::Format_ARGB32);
        const auto base = qRgba(greyBackgroundStop, greyBackgroundStop, greyBackgroundStop, 255);
        QImage hatch{image.size(), QImage::Format_ARGB32};
        hatch.fill(Qt::transparent);
        {
            QPainter painter{&hatch};
            painter.setPen(QPen{QColor{150, 150, 150, 255}, 1});
            for (int d = -hatch.height(); d < hatch.width(); d += 7) {
                painter.drawLine(d, 0, d + hatch.height(), hatch.height());
            }
            painter.end();
        }
        for (int y = 0; y < image.height(); y += 1) {
            auto * line = reinterpret_cast<QRgb *>(image.scanLine(y));
            const auto * hatchLine = reinterpret_cast<const QRgb *>(hatch.constScanLine(y));
            for (int x = 0; x < image.width(); x += 1) {
                if (line[x] == base) {
                    line[x] = qAlpha(hatchLine[x]) > 0 ? qRgba(150, 150, 150, 255) : base;
                }
            }
        }
    }
}

const string UtilityGrib::tempColorMap{
    "-40 170 130 200\n"
    "-30 130 70 200\n"
    "-20 60 90 220\n"
    "-10 70 170 245\n"
    "-1 190 235 255\n"
    "0 130 205 130\n"
    "10 45 160 55\n"
    "20 245 235 90\n"
    "27 250 160 45\n"
    "33 225 55 40\n"
    "40 150 0 25\n"
    "50 95 0 0\n"};

const string UtilityGrib::reflColorMap{
    "-35 0 0 0 0\n"
    "4.9 0 0 0 0\n"
    "5 100 235 235\n"
    "20 20 130 240\n"
    "30 10 0 240\n"
    "35 20 250 20\n"
    "40 15 180 15\n"
    "45 240 240 20\n"
    "50 235 20 20\n"
    "60 235 20 235\n"
    "70 255 255 255\n"};

const vector<UtilityGrib::Field> UtilityGrib::fields{
    Field{"2m Temperature", "tmp2m", "C", ":TMP:2 m above ground:", tempColorMap},
    Field{"2m Temp / Dewpoint", "tmpdew2m", "C", ":TMP:2 m above ground:", tempColorMap,
          "2dfld", 5, ":DPT:2 m above ground:"},
    Field{"2m Temp / Dew / Wind", "tmpdewwind", "C", ":TMP:2 m above ground:", tempColorMap,
          "2dfld", 5, ":DPT:2 m above ground:", true},
    Field{"2m Dewpoint", "dpt2m", "C", ":DPT:2 m above ground:", dewColorMap},
    Field{"2m Relative Humidity", "rh2m", "%", ":RH:2 m above ground:", rhColorMap},
    Field{"Surface Temperature", "tmpsfc", "C", ":TMP:surface:", tempColorMap},
    Field{"10m Wind Speed", "wind10m", "m/s", ":WIND:10 m above ground:", windColorMap},
    Field{"Surface Wind Gust", "gust", "m/s", ":GUST:surface:", windColorMap},
    Field{"Composite Reflectivity", "refc", "dBZ", ":REFC:entire atmosphere", reflColorMap},
    Field{"Reflectivity 1km AGL", "refd1km", "dBZ", ":REFD:1000 m above ground:", reflColorMap},
    Field{"Echo Top", "retop", "m", ":RETOP:entire atmosphere", echoTopColorMap},
    Field{"Updraft Helicity 2-5km", "uphl25", "m2/s2", ":MXUPHL:5000-2000 m above ground:", uphlColorMap},
    Field{"Max Updraft Velocity", "maxuvv", "m/s", ":MAXUVV:100-1000 mb:", updraftColorMap},
    Field{"Surface CAPE", "capesfc", "J/kg", ":CAPE:surface:", capeColorMap},
    Field{"Surface CIN", "cinsfc", "J/kg", ":CIN:surface:", cinColorMap},
    Field{"0-3km Storm-Rel Helicity", "srh3", "m2/s2", ":HLCY:3000-0 m above ground:", srhColorMap},
    Field{"Precipitable Water", "pwat", "mm", ":PWAT:entire atmosphere", pwatColorMap},
    Field{"Total Precipitation", "apcp", "mm", ":APCP:surface:", precipColorMap},
    Field{"Surface Visibility", "vis", "m", ":VIS:surface:", visColorMap},
    Field{"Total Cloud Cover", "tcdc", "%", ":TCDC:entire atmosphere", cloudColorMap},
    Field{"Snow Depth", "snod", "m", ":SNOD:surface:", snowDepthColorMap},
    Field{"200mb Height", "hgt200", "gpm", ":HGT:200 mb:", hgt200ColorMap, "prslev", 120},
    Field{"300mb Height", "hgt300", "gpm", ":HGT:300 mb:", hgt300ColorMap, "prslev", 120},
    Field{"500mb Height", "hgt500", "gpm", ":HGT:500 mb:", hgt500ColorMap, "prslev", 60},
    Field{"700mb Height", "hgt700", "gpm", ":HGT:700 mb:", hgt700ColorMap, "prslev", 30},
    Field{"850mb Height", "hgt850", "gpm", ":HGT:850 mb:", hgt850ColorMap, "prslev", 30},
};

vector<string> UtilityGrib::fieldLabels() {
    vector<string> labels;
    for (const auto& field : fields) {
        labels.push_back(field.label);
    }
    return labels;
}

vector<string> UtilityGrib::forecastHours(int cycle) {
    // RRFS runs to 84 h at the 00/06/12/18 UTC cycles, 18 h at the other hours
    const int maxHour = (cycle >= 0 && cycle % 6 == 0) ? 85 : 19;
    vector<string> hours;
    for (auto index : range3(1, maxHour, 1)) {
        hours.push_back(WString::fixedLengthStringPad0(To::string(index), 2));
    }
    return hours;
}

vector<std::pair<string, string>> UtilityGrib::runOptions() {
    static vector<std::pair<string, string>> cached;
    static qint64 cachedAtMs = 0;
    const auto nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!cached.empty() && nowMs - cachedAtMs < 600000) {
        return cached;
    }
    vector<std::pair<string, string>> options{{"Latest", ""}};
    const auto now = QDateTime::currentDateTimeUtc();
    int found = 0;
    for (int hoursBack = 1; hoursBack <= 30 && found < 12; hoursBack += 1) {
        const auto runTime = now.addSecs(-3600 * hoursBack);
        const auto date = runTime.toString("yyyyMMdd").toStdString();
        const auto hour = runTime.toString("HH").toStdString();
        const auto probe = UtilityIO::getHtml(
            baseDir + "/rrfs." + date + "/" + hour + "/rrfs.t" + hour + "z.2dfld.3km.subh.f001.conus.grib2.idx");
        if (WString::contains(probe, ":REFC:")) {
            options.emplace_back(runTime.toString("MMM d ").toStdString() + hour + "z", date + hour);
            found += 1;
        }
    }
    if (found > 0) {
        cached = options;
        cachedAtMs = nowMs;
    }
    return options;
}

vector<std::pair<string, string>> UtilityGrib::synopticRunOptions() {
    vector<std::pair<string, string>> synoptic{{"Latest", ""}};
    for (const auto& option : runOptions()) {
        if (option.second.size() == 10 && To::Int(option.second.substr(8, 2)) % 6 == 0) {
            synoptic.push_back(option);
        }
    }
    return synoptic;
}

bool UtilityGrib::resolveSynopticRun(const string& runId, string& dateStr, string& cycle) {
    if (runId.size() == 10) {
        dateStr = runId.substr(0, 8);
        cycle = runId.substr(8, 2);
        return true;
    }
    for (const auto& option : synopticRunOptions()) {
        if (option.second.size() == 10) {
            dateStr = option.second.substr(0, 8);
            cycle = option.second.substr(8, 2);
            return true;
        }
    }
    return resolveLatestRun(dateStr, cycle);   // fallback - may not be synoptic
}

vector<string> UtilityGrib::regions() {
    vector<string> list;
    for (const auto& entry : regionTable) {
        list.emplace_back(entry.label);
    }
    return list;
}

UtilityGrib::Bbox UtilityGrib::regionBbox(int regionIndex) {
    if (regionIndex < 0 || regionIndex >= regionCount) {
        regionIndex = 0;
    }
    auto box = regionTable[regionIndex].box;
    if (box.west == 0.0 && box.east == 0.0) {
        const auto here = Location::getLatLonCurrent();
        box = Bbox{here.lon() - 6.5, here.lat() - 4.5, here.lon() + 6.5, here.lat() + 4.5};
    }
    return box;
}

// one output column per native cell ("1 pixel per 3km") is the resolution
// beyond which gdalwarp's bilinear resample can't add any real detail -
// it's just interpolated filler. lonSpanKm accounts for longitude degrees
// shrinking toward the poles (cos of the domain's center latitude).
int UtilityGrib::nativeGridColumns(const Bbox& box) {
    const auto centerLatRad = (box.north + box.south) / 2.0 * M_PI / 180.0;
    const auto lonSpanKm = (box.east - box.west) * 111.32 * std::cos(centerLatRad);
    return std::max(1, static_cast<int>(std::round(lonSpanKm / nativeGridKm)));
}

// SPC-meso/"My Area" domains render at true 1:1 - one pixel per native grid
// cell, no more (wasted interpolation, slower for no real detail) and no
// less (that's exactly the zoomed-in view where per-pixel detail matters).
// CONUS/NA is a deliberate step down from that: at that zoom the eye can't
// resolve native-resolution detail anyway, so rendering at half native
// trades a speed/file-size win for no visible loss.
int UtilityGrib::mainRenderColumns(const Bbox& box) {
    const auto span = std::max(box.east - box.west, box.north - box.south);
    const auto native = nativeGridColumns(box);
    if (span > 30.0) {
        return std::max(400, native / 2);   // CONUS-sized (and any future NA/AK-sized) regions
    }
    return native;   // SPC-meso / "My Area" - true 1:1
}

string UtilityGrib::gdalBinDir() {
    const auto found = QStandardPaths::findExecutable("gdalwarp");
    if (found.isEmpty()) {
        return "";
    }
    return QFileInfo{found}.absolutePath().toStdString();
}

bool UtilityGrib::gdalAvailable() {
    return !gdalBinDir().empty();
}

string UtilityGrib::dataStream() {
    return rrfsStream;
}

string UtilityGrib::cacheDir() {
    auto path = QDir::tempPath() + "/wxqt_grib";
    QDir{}.mkpath(path);
    return path.toStdString();
}

string UtilityGrib::cwaLinesGeoJson() {
    const auto path = QString::fromStdString(cacheDir()) + "/cwa.geojson";
    if (!QFile::exists(path)) {
        QFile::copy(QString::fromStdString(GlobalVariables::resDir) + "cwa.geojson", path);
    }
    return path.toStdString();
}

// shared cache-or-generate wrapper for every *LinesGeoJson() accessor below -
// each is a one-liner over this once a resource file's cached path is known.
string UtilityGrib::cachedLinesGeoJson(const string& cacheFileName, const string& resourceBinName) {
    const auto path = QString::fromStdString(cacheDir()) + QString::fromStdString("/" + cacheFileName);
    if (QFile::exists(path)) {
        return path.toStdString();
    }
    const auto json = binSegmentsToGeoJson(resourceBinName);
    QFile file{path};
    if (file.open(QIODevice::WriteOnly)) {
        file.write(json.c_str(), static_cast<qint64>(json.size()));
        file.close();
    }
    return path.toStdString();
}

string UtilityGrib::stateLinesGeoJson() {
    return cachedLinesGeoJson("states.geojson", "statev2.bin");
}

// county.bin/hwv4.bin/lakesv3.bin/ca.bin/mx.bin - all already bundled for
// the Nexrad radar screen's geometry-overlay system (RadarGeometry.cpp),
// same binary format `stateLinesGeoJson()` already parses, reused here
// rather than sourcing new boundary data. hwv4.bin is the primary-highways
// tier (hwv4ext.bin is a denser secondary tier, not used here to avoid
// over-cluttering a synoptic-scale map). ca.bin/mx.bin (Canada/Mexico
// borders+provinces/states) are relevant since several regions actually
// reach those borders (Northwest/Great Lakes/North Central; Southwest/
// South Central).
string UtilityGrib::countyLinesGeoJson() {
    return cachedLinesGeoJson("county.geojson", "county.bin");
}

string UtilityGrib::highwayLinesGeoJson() {
    return cachedLinesGeoJson("hwy.geojson", "hwv4.bin");
}

string UtilityGrib::lakeLinesGeoJson() {
    return cachedLinesGeoJson("lakes.geojson", "lakesv3.bin");
}

string UtilityGrib::canadaLinesGeoJson() {
    return cachedLinesGeoJson("canada.geojson", "ca.bin");
}

string UtilityGrib::mexicoLinesGeoJson() {
    return cachedLinesGeoJson("mexico.geojson", "mx.bin");
}

bool UtilityGrib::resolveLatestRun(string& dateStr, string& cycle) {
    // NOMADS only serves the cycle-level directory listing, so the run cannot be
    // discovered by scraping. Derive the cycle from the clock and probe backward
    // (data lands roughly 1.5-2 h after cycle time) until f001 CONUS is present.
    // The result is cached for 10 minutes so region/field/hour changes don't re-probe.
    static string resolvedDate;
    static string resolvedCycle;
    static qint64 resolvedAtMs = 0;
    const auto nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!resolvedDate.empty() && nowMs - resolvedAtMs < 600000) {
        dateStr = resolvedDate;
        cycle = resolvedCycle;
        return true;
    }
    const auto now = QDateTime::currentDateTimeUtc();
    for (int hoursBack = 1; hoursBack <= 12; hoursBack += 1) {
        const auto runTime = now.addSecs(-3600 * hoursBack);
        const auto date = runTime.toString("yyyyMMdd").toStdString();
        const auto hour = runTime.toString("HH").toStdString();
        const auto probe = UtilityIO::getHtml(
            baseDir + "/rrfs." + date + "/" + hour + "/rrfs.t" + hour + "z.2dfld.3km.subh.f001.conus.grib2.idx");
        if (WString::contains(probe, ":REFC:")) {
            if (resolvedDate != date || resolvedCycle != hour) {
                purgeCacheForOldRun(date + hour);
            }
            resolvedDate = date;
            resolvedCycle = hour;
            resolvedAtMs = nowMs;
            dateStr = date;
            cycle = hour;
            return true;
        }
    }
    return false;
}

// remove cached grib slices / rendered images that are not from the given run id
void UtilityGrib::purgeCacheForOldRun(const string& keepRunId) {
    QDir dir{QString::fromStdString(cacheDir())};
    const auto keep = QString::fromStdString("_" + keepRunId + "_");
    for (const auto& name : dir.entryList(
            {"g_*.grib2", "gc_*.grib2", "gu_*.grib2", "gv_*.grib2", "gg_*.grib2", "r_*.png", "r3_*.png", "r4_*.png", "r5_*", "r6_*", "r7_*"}, QDir::Files)) {
        if (!name.contains(keep)) {
            dir.remove(name);
        }
    }
}

bool UtilityGrib::idxByteRange(const string& idxText, const string& match, long long& start, long long& end, int stepHour) {
    const auto lines = WString::split(idxText, "\n");
    start = -1;
    end = -1;
    // in sub-hourly files a field appears at 15/30/45/60-min steps; require the
    // record whose step ends on the wanted forecast hour. Instantaneous records
    // read ":N hour fcst:", accumulated/averaged ones ":0-N hour acc fcst:".
    const auto tok = To::string(stepHour) + " hour ";
    const auto onHour = ":" + tok;
    const auto onHourAccum = "-" + tok;
    for (size_t i = 0; i < lines.size(); i += 1) {
        if (!WString::contains(lines[i], match)) {
            continue;
        }
        if (stepHour > 0) {
            if (lines[i].find(onHour) == string::npos && lines[i].find(onHourAccum) == string::npos) {
                continue;
            }
        }
        const auto fieldsOnLine = WString::split(lines[i], ":");
        if (fieldsOnLine.size() < 2) {
            return false;
        }
        start = To::int64(fieldsOnLine[1]);
        for (size_t j = i + 1; j < lines.size(); j += 1) {
            const auto next = WString::split(lines[j], ":");
            if (next.size() >= 2 && !next[1].empty()) {
                end = To::int64(next[1]) - 1;
                break;
            }
        }
        return true;
    }
    return false;
}

// draw each contour's value along the line, onto the finished PNG. decimals/
// suffix control the label text (e.g. 0/"" for a rounded height in gpm,
// 2/"\"" for a hail diameter in inches) - public so other RRFS-derived
// products (e.g. UtilitySevereIndices) can reuse this instead of
// duplicating it, the way fetchFieldSlice already is.
void UtilityGrib::labelContours(const QString& pngPath, const QString& geoJsonPath, const Bbox& box, bool green,
                                 int decimals, const QString& suffix) {
    QFile jsonFile{geoJsonPath};
    if (!jsonFile.open(QIODevice::ReadOnly)) {
        return;
    }
    const auto doc = QJsonDocument::fromJson(jsonFile.readAll());
    jsonFile.close();
    QImage image{pngPath};
    if (image.isNull()) {
        return;
    }
    const auto spanLon = box.east - box.west;
    const auto spanLat = box.north - box.south;
    if (spanLon <= 0.0 || spanLat <= 0.0) {
        return;
    }
    auto toPixel = [&] (double lon, double lat) {
        return QPointF((lon - box.west) / spanLon * image.width(),
                       (box.north - lat) / spanLat * image.height());
    };

    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing, true);
    auto font = painter.font();
    font.setPixelSize(std::max(15, image.height() / 55));
    font.setBold(true);
    painter.setFont(font);
    const QColor textColor = green ? QColor{15, 60, 15} : QColor{255, 255, 255};
    const QColor haloColor = green ? QColor{255, 255, 255, 220} : QColor{0, 0, 0, 210};
    const auto minGap = image.width() / 16.0;   // keep labels spread out
    std::vector<QPointF> placed;

    auto tryLabel = [&] (const QPointF& position, const QString& text) {
        if (position.x() < 25 || position.x() > image.width() - 45
            || position.y() < 18 || position.y() > image.height() - 12) {
            return;
        }
        for (const auto& other : placed) {
            if (std::hypot(position.x() - other.x(), position.y() - other.y()) < minGap) {
                return;
            }
        }
        placed.push_back(position);
        painter.setPen(haloColor);
        for (int dx = -2; dx <= 2; dx += 2) {
            for (int dy = -2; dy <= 2; dy += 2) {
                painter.drawText(position + QPointF(dx, dy), text);
            }
        }
        painter.setPen(textColor);
        painter.drawText(position, text);
    };

    for (const auto& featureValue : doc.object().value("features").toArray()) {
        const auto feature = featureValue.toObject();
        const auto elevation = feature.value("properties").toObject().value("elev").toDouble();
        const auto text = QString::number(elevation, 'f', decimals) + suffix;
        const auto geometry = feature.value("geometry").toObject();
        const auto type = geometry.value("type").toString();
        QJsonArray lineStrings;
        if (type == "LineString") {
            lineStrings.append(geometry.value("coordinates"));
        } else if (type == "MultiLineString") {
            lineStrings = geometry.value("coordinates").toArray();
        }
        for (const auto& lineValue : lineStrings) {
            const auto points = lineValue.toArray();
            if (points.size() < 22) {
                continue;   // skip short fragments (mostly noise)
            }
            const std::vector<double> fractions = points.size() > 60
                ? std::vector<double>{0.25, 0.6, 0.85} : std::vector<double>{0.5};
            for (const double fraction : fractions) {
                const auto point = points[static_cast<int>(points.size() * fraction)].toArray();
                tryLabel(toPixel(point[0].toDouble(), point[1].toDouble()), text);
            }
        }
    }
    painter.end();
    image.save(pngPath, "PNG");
}

namespace {
    // one wind barb (knots) at a screen point; staff points upwind
    void drawBarb(QPainter& painter, const QPointF& at, double dirX, double dirY, double knots) {
        if (knots < 2.5) {
            painter.drawEllipse(at, 3.0, 3.0);
            return;
        }
        const double staffLength = 30.0;
        const QPointF tip = at + QPointF{dirX, dirY} * staffLength;
        painter.drawLine(at, tip);
        const double angle = -68.0 * M_PI / 180.0;
        const QPointF side{dirX * std::cos(angle) - dirY * std::sin(angle),
                           dirX * std::sin(angle) + dirY * std::cos(angle)};
        const QPointF towardStation{-dirX, -dirY};
        int remaining = static_cast<int>(std::round(knots / 5.0)) * 5;
        double along = 0.0;
        const double step = 5.2;
        while (remaining >= 50) {
            const QPointF base = tip + towardStation * along;
            const QPointF apex = base + side * 11.0;
            QPolygonF pennant;
            pennant << base << apex << (tip + towardStation * (along + step * 1.7));
            painter.setBrush(painter.pen().color());
            painter.drawPolygon(pennant);
            painter.setBrush(Qt::NoBrush);
            along += step * 2.0;
            remaining -= 50;
        }
        while (remaining >= 10) {
            const QPointF base = tip + towardStation * along;
            painter.drawLine(base, base + side * 11.0);
            along += step;
            remaining -= 10;
        }
        if (remaining >= 5) {
            const QPointF base = tip + towardStation * std::max(along, step);
            painter.drawLine(base, base + side * 6.0);
        }
    }

    // read a "lon lat value" XYZ dump into parallel vectors
    bool readXyz(const QString& path, std::vector<double>& lon, std::vector<double>& lat, std::vector<double>& value) {
        QFile file{path};
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }
        QTextStream stream{&file};
        while (!stream.atEnd()) {
            const auto parts = stream.readLine().split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 3) {
                lon.push_back(parts[0].toDouble());
                lat.push_back(parts[1].toDouble());
                value.push_back(parts[2].toDouble());
            }
        }
        file.close();
        return !value.empty();
    }


    struct StationPoint {
        QString label;
        double lon;
        double lat;
    };

    // NWS forecast offices first (priority), then ASOS/METAR sites to fill gaps,
    // keeping stations at least minGapPx apart in the rendered image.
    std::vector<StationPoint> pickStations(const UtilityGrib::Bbox& box, double imageWidth, double imageHeight) {
        std::vector<StationPoint> picked;
        std::vector<QPointF> placedPx;
        const auto spanLon = box.east - box.west;
        const auto spanLat = box.north - box.south;
        const auto minGapPx = imageWidth / 17.0;   // was /21 - a modest thin-out (user: "least favorite option")
        auto consider = [&] (const string& code, const LatLon& latLon, bool stripK) {
            const auto lon = latLon.lon();
            const auto lat = latLon.lat();
            if (lon < box.west || lon > box.east || lat < box.south || lat > box.north) {
                return;
            }
            const QPointF px{(lon - box.west) / spanLon * imageWidth,
                             (box.north - lat) / spanLat * imageHeight};
            for (const auto& other : placedPx) {
                if (std::hypot(px.x() - other.x(), px.y() - other.y()) < minGapPx) {
                    return;
                }
            }
            auto label = QString::fromStdString(code);
            if (stripK && label.size() == 4 && label.startsWith('K')) {
                label = label.mid(1);
            }
            placedPx.push_back(px);
            picked.push_back({label, lon, lat});
        };
        if (WfoSites::sites != nullptr) {
            for (const auto& code : WfoSites::sites->codeList) {
                const auto it = WfoSites::sites->byCode.find(code);
                if (it != WfoSites::sites->byCode.end() && it->second) {
                    consider(code, it->second->latLon, false);
                }
            }
        }
        if (Metar::sites != nullptr) {
            for (const auto& code : Metar::sites->codeList) {
                const auto it = Metar::sites->byCode.find(code);
                if (it != Metar::sites->byCode.end() && it->second) {
                    consider(code, it->second->latLon, true);
                }
            }
        }
        return picked;
    }

    struct CityPoint {
        QString label;
        double lon;
        double lat;
    };

    // Population-priority label placement: biggest city wins whenever it
    // would overlap a smaller one already placed - same greedy min-pixel-gap
    // approach as pickStations() above, sourced from CitiesExtended (already
    // bundled for the Nexrad radar screen) instead of WFO/METAR sites.
    vector<CityPoint> pickCities(const UtilityGrib::Bbox& box, double imageWidth, double imageHeight) {
        CitiesExtended::create();
        vector<CityPoint> picked;
        std::vector<QPointF> placedPx;
        const auto spanLon = box.east - box.west;
        const auto spanLat = box.north - box.south;
        const auto minGapPx = imageWidth / 14.0;

        auto sorted = CitiesExtended::cities;   // already population-sorted in cityall.txt, but don't rely on that
        std::sort(sorted.begin(), sorted.end(), [] (const CityExt& a, const CityExt& b) {
            return a.population > b.population;
        });
        for (const auto& city : sorted) {
            if (city.longitude < box.west || city.longitude > box.east ||
                city.latitude < box.south || city.latitude > box.north) {
                continue;
            }
            const QPointF px{(city.longitude - box.west) / spanLon * imageWidth,
                             (box.north - city.latitude) / spanLat * imageHeight};
            bool tooClose = false;
            for (const auto& other : placedPx) {
                if (std::hypot(px.x() - other.x(), px.y() - other.y()) < minGapPx) {
                    tooClose = true;
                    break;
                }
            }
            if (tooClose) {
                continue;
            }
            placedPx.push_back(px);
            picked.push_back({QString::fromStdString(city.name), city.longitude, city.latitude});
            if (picked.size() >= 150) {
                break;   // plenty for any screen size - avoid scanning the whole ~29k-city list needlessly
            }
        }
        return picked;
    }

    // nearest grid value from an XYZ dump (grids are all warped identically so
    // lon/lat come from the first, others are value-aligned by index)
    double nearestValue(const std::vector<double>& gLon, const std::vector<double>& gLat,
                        const std::vector<double>& value, double lon, double lat) {
        double best = 1e30;
        double result = -9999.0;
        for (size_t i = 0; i < value.size(); i += 1) {
            const auto d = (gLon[i] - lon) * (gLon[i] - lon) + (gLat[i] - lat) * (gLat[i] - lat);
            if (d < best) {
                best = d;
                result = value[i];
            }
        }
        return result;
    }

    void drawStationPlots(const QString& pngPath, const std::vector<StationPoint>& stations,
                          const QString& tXyz, const QString& tdXyz, const QString& uXyz,
                          const QString& vXyz, const QString& gustXyz, const UtilityGrib::Bbox& box) {
        std::vector<double> gLon, gLat, t, junk, td, u, v, gust;
        if (!readXyz(tXyz, gLon, gLat, t)) {
            return;
        }
        readXyz(tdXyz, junk, junk, td);   junk.clear();
        readXyz(uXyz, junk, junk, u);     junk.clear();
        readXyz(vXyz, junk, junk, v);     junk.clear();
        const bool haveGust = readXyz(gustXyz, junk, junk, gust);
        const bool haveWind = u.size() == t.size() && v.size() == t.size();
        const bool haveTd = td.size() == t.size();

        QImage image{pngPath};
        if (image.isNull()) {
            return;
        }
        const auto spanLon = box.east - box.west;
        const auto spanLat = box.north - box.south;
        QPainter painter{&image};
        painter.setRenderHint(QPainter::Antialiasing, true);
        auto font = painter.font();
        font.setPixelSize(std::max(11, image.height() / 85));   // was max(13, .../70) - smaller, less blocky
        font.setBold(true);
        painter.setFont(font);
        const auto lineWidth = std::max(1.3, image.height() / 1000.0);
        const QPen sustainedPen{QColor{20, 20, 22}, lineWidth};
        const QPen gustPen{QColor{220, 20, 20}, lineWidth};
        const auto off = font.pixelSize();

        for (const auto& station : stations) {
            const QPointF at{(station.lon - box.west) / spanLon * image.width(),
                             (box.north - station.lat) / spanLat * image.height()};
            const auto temp = nearestValue(gLon, gLat, t, station.lon, station.lat);
            if (temp < -998.0) {
                continue;
            }
            painter.setPen(sustainedPen);
            painter.setBrush(QColor{20, 20, 22});
            painter.drawEllipse(at, 2.4, 2.4);
            painter.setBrush(Qt::NoBrush);

            auto text = [&] (const QString& s, double dx, double dy, const QColor& colour) {
                painter.setPen(QColor{255, 255, 255});
                for (int ox = -1; ox <= 1; ox += 1) {
                    for (int oy = -1; oy <= 1; oy += 1) {
                        painter.drawText(at + QPointF{dx + ox, dy + oy}, s);
                    }
                }
                painter.setPen(colour);
                painter.drawText(at + QPointF{dx, dy}, s);
            };
            text(QString::number(qRound(temp)), -off * 2.6, -off * 0.3, QColor{170, 20, 20});
            if (haveTd) {
                const auto dew = nearestValue(gLon, gLat, td, station.lon, station.lat);
                if (dew > -998.0) {
                    text(QString::number(qRound(dew)), -off * 2.6, off * 1.1, QColor{20, 110, 20});
                }
            }
            text(station.label, -off * 1.2, off * 2.4, QColor{40, 40, 45});

            if (haveWind) {
                const auto uu = nearestValue(gLon, gLat, u, station.lon, station.lat);
                const auto vv = nearestValue(gLon, gLat, v, station.lon, station.lat);
                if (std::abs(uu) < 300.0 && std::abs(vv) < 300.0) {
                    const auto speed = std::hypot(uu, vv);
                    const auto knots = speed * 1.943844;
                    double dirX = 0.0;
                    double dirY = 0.0;
                    if (speed > 0.01) {
                        dirX = -uu / speed;
                        dirY = vv / speed;
                    }
                    painter.setPen(sustainedPen);
                    drawBarb(painter, at, dirX, dirY, knots);
                    if (haveGust) {
                        const auto g = nearestValue(gLon, gLat, gust, station.lon, station.lat);
                        if (std::abs(g) < 300.0 && g * 1.943844 - knots > 5.0) {
                            painter.setPen(gustPen);
                            drawBarb(painter, at, dirX, dirY, g * 1.943844);
                        }
                    }
                }
            }
        }
        painter.end();
        image.save(pngPath, "PNG");
    }
}

void UtilityGrib::drawCityLabels(const QString& pngPath, const Bbox& box) {
    QImage image{pngPath};
    if (image.isNull()) {
        return;
    }
    const auto cities = pickCities(box, image.width(), image.height());
    if (cities.empty()) {
        return;
    }
    const auto spanLon = box.east - box.west;
    const auto spanLat = box.north - box.south;

    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing, true);
    auto font = painter.font();
    font.setPixelSize(std::max(11, image.height() / 85));
    font.setBold(true);
    painter.setFont(font);
    const QColor dotColor{20, 20, 20};
    const QColor textColor{20, 20, 20};
    const QColor haloColor{255, 255, 255, 220};

    for (const auto& city : cities) {
        const QPointF at{(city.lon - box.west) / spanLon * image.width(),
                         (box.north - city.lat) / spanLat * image.height()};
        painter.setPen(dotColor);
        painter.setBrush(dotColor);
        painter.drawEllipse(at, 2.2, 2.2);
        painter.setBrush(Qt::NoBrush);

        const auto textPos = at + QPointF{5, 4};
        painter.setPen(haloColor);
        for (int dx = -1; dx <= 1; dx += 1) {
            for (int dy = -1; dy <= 1; dy += 1) {
                painter.drawText(textPos + QPointF{static_cast<double>(dx), static_cast<double>(dy)}, city.label);
            }
        }
        painter.setPen(textColor);
        painter.drawText(textPos, city.label);
    }
    painter.end();
    image.save(pngPath, "PNG");
}

string UtilityGrib::render(int fieldIndex, int regionIndex, const string& forecastHour,
                           const string& runId, string& status, double& dataMin, double& dataMax,
                           string& samplePath) {
    dataMin = 0.0;
    dataMax = 0.0;
    samplePath = "";
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        status = "invalid field";
        return "";
    }
    const auto binDir = gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return "";
    }
    if (regionIndex < 0 || regionIndex >= regionCount) {
        regionIndex = 0;
    }
    const auto& field = fields[fieldIndex];
    const bool toFahrenheit = UIPreferences::unitsF && field.units == "C";

    string dateStr;
    string cycle;
    if (runId.size() == 10) {
        dateStr = runId.substr(0, 8);
        cycle = runId.substr(8, 2);
    } else if (!resolveLatestRun(dateStr, cycle)) {
        status = "no RRFS run available on NOMADS";
        return "";
    }

    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto runKey = dateStr + cycle;
    const auto dir = QString::fromStdString(cacheDir());

    const QDateTime runUtc{
        QDate{To::Int(dateStr.substr(0, 4)), To::Int(dateStr.substr(4, 2)), To::Int(dateStr.substr(6, 2))},
        QTime{To::Int(cycle), 0}, QTimeZone::utc()};
    const auto validLocal = runUtc.addSecs(3600 * forecastHourInt).toLocalTime();
    const auto localZone = QTimeZone::systemTimeZone().abbreviation(validLocal);
    status = "RRFS " + dateStr.substr(0, 4) + "-" + dateStr.substr(4, 2) + "-" + dateStr.substr(6, 2) +
        " " + cycle + "z    F" + WString::fixedLengthStringPad0(To::string(forecastHourInt), 2) +
        " valid " + validLocal.toString("ddd h:mm AP").toStdString() + " " + localZone.toStdString() +
        "    " + field.label + "    " + regionTable[regionIndex].label;

    // final image is cached per run + field + region + forecast hour.
    // "r7" is the render version - bump it whenever the drawing pipeline changes
    // so stale cached PNGs are not served.
    const auto pngPath = dir + QString::fromStdString(
        "/r7_" + runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr3 +
        (toFahrenheit ? "_f" : "") + ".png");
    const auto rangePath = pngPath + ".range";
    const auto gridPath = pngPath + ".grid";
    auto readRange = [&] {
        QFile file{rangePath};
        if (file.open(QIODevice::ReadOnly)) {
            const auto parts = QString::fromUtf8(file.readAll()).split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                dataMin = parts[0].toDouble();
                dataMax = parts[1].toDouble();
            }
            file.close();
        }
    };
    if (QFile::exists(pngPath)) {
        readRange();
        if (QFile::exists(gridPath)) {
            samplePath = gridPath.toStdString();
        }
        return pngPath.toStdString();
    }

    // downloaded grib records are cached per run + field + forecast hour (shared by all regions).
    // The plain hourly file only exists at 00/06/12/18z (the para/prod stream is 3-hourly there
    // either way); for the in-between hourly runs fall back to the sub-hourly file, taking the
    // on-the-hour step.
    const auto plainUrl = baseDir + "/rrfs." + dateStr + "/" + cycle +
        "/rrfs.t" + cycle + "z." + field.product + ".3km.f" + fhr3 + ".conus.grib2";
    const auto subhUrl = (field.product == "2dfld")
        ? baseDir + "/rrfs." + dateStr + "/" + cycle + "/rrfs.t" + cycle + "z.2dfld.3km.subh.f" + fhr3 + ".conus.grib2"
        : string{};
    string plainIdx;
    string subhIdx;
    auto ensureSlice = [&] (const string& idxMatch, const QString& slicePath) {
        {
            QFile check{slicePath};
            if (check.size() > 200 && check.open(QIODevice::ReadOnly)) {
                const auto valid = check.read(4) == QByteArray{"GRIB"};
                check.close();
                if (valid) {
                    return true;
                }
            }
        }
        auto grab = [&] (const string& url, string& idxCache, int stepHour) -> bool {
            if (idxCache.empty()) {
                idxCache = UtilityIO::getHtml(url + ".idx");
            }
            long long start = -1;
            long long end = -1;
            if (!idxByteRange(idxCache, idxMatch, start, end, stepHour)) {
                return false;
            }
            const auto slice = URL::getBytesRange(url, start, end);
            if (slice.size() < 200 || slice.left(4) != QByteArray{"GRIB"}) {
                return false;
            }
            QFile out{slicePath};
            if (!out.open(QIODevice::WriteOnly)) {
                return false;
            }
            out.write(slice);
            out.close();
            return true;
        };
        if (grab(plainUrl, plainIdx, 0)) {
            return true;
        }
        return !subhUrl.empty() && grab(subhUrl, subhIdx, forecastHourInt);
    };

    const auto gribPath = dir + QString::fromStdString("/g_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
    if (!ensureSlice(field.idxMatch, gribPath)) {
        status = field.label + " not available for f" + fhr3;
        return "";
    }

    QString contourGribPath = gribPath;
    if (!field.contourIdxMatch.empty()) {
        contourGribPath = dir + QString::fromStdString("/gc_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
        if (!ensureSlice(field.contourIdxMatch, contourGribPath)) {
            contourGribPath.clear();
        }
    }

    // per-request intermediates - uniquely named so concurrent renders do not collide
    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr3);
    const auto colorPath = dir + "/col_" + tag + ".txt";
    const auto warpPath = dir + "/w_" + tag + ".tif";
    const auto tiffPath = dir + "/c_" + tag + ".tif";
    {
        const auto table = toFahrenheit ? colorMapToFahrenheit(field.colorMap) : field.colorMap;
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(table.c_str(), static_cast<qint64>(table.size()));
            colorFile.close();
        }
    }

    const auto box = regionBbox(regionIndex);
    const auto bin = QString::fromStdString(binDir) + "/";
    const auto gdalCalc = bin + (QFile::exists(bin + "gdal_calc") ? "gdal_calc" : "gdal_calc.py");

    auto runProcess = [&status] (const QString& program, const QStringList& args, QByteArray * stdOut = nullptr) {
        QProcess process;
        process.start(program, args);
        process.waitForFinished(30000);
        const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (stdOut) {
            *stdOut = process.readAllStandardOutput();
        }
        if (!ok) {
            status = "gdal failed: " + process.readAllStandardError().left(200).toStdString();
        }
        return ok;
    };

    const auto scaledPath = dir + "/f_" + tag + ".tif";
    // Station-plot fields need a resolution floor mainRenderColumns() alone
    // doesn't provide: text legibility and grid-data fidelity are different
    // constraints. A small SPC-meso region's true 1:1 native resolution can
    // be a few hundred columns - plenty for the data, but leaves no room
    // for temp/dewpoint/station-id text to render at anything but a
    // disproportionately large, blocky floor size (found live, 2026-09-13:
    // a 385x267 tmpdewwind render made 13px station labels look chunky).
    // 3200 restores this one field type's pre-existing (pre-tiering) fixed
    // resolution; every other field still benefits from the new tiering.
    const auto fillCols = QString::number(field.windBarbs ? std::max(mainRenderColumns(box), 3200) : mainRenderColumns(box));
    auto ok = runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
             "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath});
    // Celsius -> Fahrenheit as an exact linear rescale (F = 1.8*C + 32), nodata preserved
    QString fillTif = warpPath;
    if (ok && toFahrenheit) {
        if (runProcess(gdalCalc,
                {"-A", warpPath, "--calc=A*1.8+32", "--NoDataValue=-9999",
                 "--outfile=" + scaledPath, "--overwrite", "--quiet"})) {
            fillTif = scaledPath;
        }
    }
    // gdaldem's "-alpha" does NOT make recognised-nodata pixels transparent
    // (they come out clamped-opaque to the nearest colour stop instead - see
    // renderBackground()'s identical fix) - build an explicit 0/255 mask from
    // the warp's own nodata and merge it in as the real alpha band. RRFS's
    // Lambert grid inscribed in a lat/lon bbox always leaves nodata corners
    // outside its native coverage; those need to render transparent.
    const auto maskPath = dir + "/m_" + tag + ".tif";
    const auto echoMaskPath = dir + "/em_" + tag + ".tif";
    const auto rgbPath = dir + "/rgb_" + tag + ".tif";
    const auto gdalMerge = bin + (QFile::exists(bin + "gdal_merge") ? "gdal_merge" : "gdal_merge.py");
    ok = ok && runProcess(gdalCalc, {"-A", fillTif, "--calc=255*(A!=-9999)", "--outfile=" + maskPath,
                                      "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"});
    // reflectivity's colour map is deliberately transparent below ~5 dBZ (real
    // "no echo", not nodata) - the nodata mask above only knows about the
    // RRFS domain edge, so AND in a value-based mask here too, or clear air
    // would come out solid black (colour table RGB for "no echo" is 0,0,0)
    // instead of transparent.
    const auto isReflectivity = (field.key == "refc" || field.key == "refd1km");
    if (isReflectivity) {
        ok = ok && runProcess(gdalCalc, {"-A", fillTif, "--calc=255*(A>=5)", "--outfile=" + echoMaskPath,
                                          "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"});
        if (ok) {
            const auto combinedPath = dir + "/cm_" + tag + ".tif";
            if (runProcess(gdalCalc, {"-A", maskPath, "-B", echoMaskPath, "--calc=minimum(A,B)",
                                      "--outfile=" + combinedPath, "--overwrite", "--quiet",
                                      "--type=Byte", "--NoDataValue=0"})) {
                QFile::remove(maskPath);
                QFile::rename(combinedPath, maskPath);
            }
        }
        QFile::remove(echoMaskPath);
    }
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-of", "GTiff", fillTif, colorPath, rgbPath});
    ok = ok && runProcess(gdalMerge, {"-q", "-o", tiffPath, "-separate", "-co", "PHOTOMETRIC=RGB", rgbPath, maskPath});
    for (const auto& stale : {maskPath, rgbPath}) {
        QFile::remove(stale);
    }
    if (!ok) {
        for (const auto& stale : {colorPath, warpPath, scaledPath, tiffPath}) {
            QFile::remove(stale);
        }
        return "";
    }

    // value range actually present in the rendered area, for trimming the legend
    QByteArray info;
    if (runProcess(bin + "gdalinfo", {"-mm", fillTif}, &info)) {
        const auto text = QString::fromUtf8(info);
        const auto match = QRegularExpression{R"(Computed Min/Max=(-?[0-9.]+),(-?[0-9.]+))"}.match(text);
        if (match.hasMatch()) {
            dataMin = match.captured(1).toDouble();
            dataMax = match.captured(2).toDouble();
            QFile file{rangePath};
            if (file.open(QIODevice::WriteOnly)) {
                file.write((QString::number(dataMin, 'f', 2) + " " + QString::number(dataMax, 'f', 2)).toUtf8());
                file.close();
            }
        }
    }

    // point-value sidecar: a coarse XYZ dump of the field (already in display
    // units) over the same extent as the PNG, for the hover read-out. A failure
    // here must not break the (already set) status line.
    {
        const auto savedStatus = status;
        const auto sampleCols = QString::number(sampleGridColumns(box));
        if (runProcess(bin + "gdal_translate", {"-q", "-of", "XYZ", "-outsize", sampleCols, "0", fillTif, gridPath})) {
            samplePath = gridPath.toStdString();
        } else {
            status = savedStatus;
        }
    }

    const auto burnLines = [&] (const string& geoJson, int red, int green, int blue) {
        runProcess(bin + "gdal_rasterize",
            {"-q", "-b", "1", "-b", "2", "-b", "3", "-b", "4",
             "-burn", QString::number(red), "-burn", QString::number(green),
             "-burn", QString::number(blue), "-burn", "255",
             QString::fromStdString(geoJson), tiffPath});
    };
    // optional contour lines (of this field, or of a second "derived" field),
    // generated from a smoothed coarse warp so the lines stay clean
    QString labelledContourGeoJson;
    const bool greenContours = !field.contourIdxMatch.empty();
    if (field.contourInterval > 0 && !contourGribPath.isEmpty()) {
        const auto coarsePath = dir + "/co_" + tag + ".tif";
        const auto contourRaw = dir + "/cr_" + tag + ".geojson";
        const auto contourBuf = dir + "/cb_" + tag + ".geojson";
        for (const auto& stale : {contourRaw, contourBuf}) {
            QFile::remove(stale);
        }
        const auto coarseF = dir + "/cof_" + tag + ".tif";
        const auto interval = toFahrenheit ? 10 : field.contourInterval;
        bool coarseOk = runProcess(bin + "gdalwarp",
                {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
                 "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
                 "-r", "cubicspline", "-ts", "190", "0", contourGribPath, coarsePath});
        QString contourSrc = coarsePath;
        if (coarseOk && toFahrenheit) {
            if (runProcess(gdalCalc,
                    {"-A", coarsePath, "--calc=A*1.8+32", "--NoDataValue=-9999",
                     "--outfile=" + coarseF, "--overwrite", "--quiet"})) {
                contourSrc = coarseF;
            }
        }
        if (coarseOk
            && runProcess(bin + "gdal_contour",
                {"-q", "-a", "elev", "-i", QString::number(interval), contourSrc, contourRaw})
            && runProcess(bin + "ogr2ogr",
                {"-q", "-f", "GeoJSON", contourBuf, contourRaw, "-dialect", "sqlite",
                 "-sql", "SELECT ST_Buffer(ST_SimplifyPreserveTopology(geometry, 0.06), 0.02) AS geometry FROM contour"})) {
            burnLines(contourBuf.toStdString(), greenContours ? 20 : 245, greenContours ? 70 : 245, greenContours ? 20 : 245);
            labelledContourGeoJson = contourRaw;   // keep it for labelling after PNG
        }
        QFile::remove(coarsePath);
        QFile::remove(coarseF);
        QFile::remove(contourBuf);
    }

    // optional station plots (T / Td / wind barb + station id) at WFO + ASOS sites
    std::vector<StationPoint> stationList;
    QString stnT;
    QString stnTd;
    QString stnU;
    QString stnV;
    QString stnGust;
    if (field.windBarbs) {
        stationList = pickStations(box, 3200.0, 3200.0 * (box.north - box.south) / (box.east - box.west));
        auto sampleGrid = [&] (const QString& src, const QString& xyz, bool celsiusToF) {
            const auto gridTif = xyz + ".tif";
            bool sampled = runProcess(bin + "gdalwarp",
                    {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
                     "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
                     "-r", "bilinear", "-ts", "384", "0", src, gridTif});
            if (sampled && celsiusToF) {
                const auto gf = gridTif + ".f.tif";
                if (runProcess(gdalCalc, {"-A", gridTif, "--calc=A*1.8+32", "--NoDataValue=-9999",
                                          "--outfile=" + gf, "--overwrite", "--quiet"})) {
                    QFile::remove(gridTif);
                    QFile::rename(gf, gridTif);
                }
            }
            sampled = sampled && runProcess(bin + "gdal_translate", {"-q", "-of", "XYZ", gridTif, xyz});
            QFile::remove(gridTif);
            return sampled;
        };
        const auto tdGrib = dir + QString::fromStdString("/gc_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
        const auto uGrib = dir + QString::fromStdString("/gu_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
        const auto vGrib = dir + QString::fromStdString("/gv_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
        const auto gGrib = dir + QString::fromStdString("/gg_" + runKey + "_" + field.key + "_" + fhr3 + ".grib2");
        if (!stationList.empty()) {
            stnT = dir + "/st_" + tag + ".xyz";
            if (!sampleGrid(gribPath, stnT, toFahrenheit)) {
                stnT.clear();
            }
            if (!stnT.isEmpty() && ensureSlice(":DPT:2 m above ground:", tdGrib)) {
                stnTd = dir + "/std_" + tag + ".xyz";
                if (!sampleGrid(tdGrib, stnTd, toFahrenheit)) {
                    stnTd.clear();
                }
            }
            if (!stnT.isEmpty() && ensureSlice(":UGRD:10 m above ground:", uGrib) && ensureSlice(":VGRD:10 m above ground:", vGrib)) {
                stnU = dir + "/stu_" + tag + ".xyz";
                stnV = dir + "/stv_" + tag + ".xyz";
                if (!(sampleGrid(uGrib, stnU, false) && sampleGrid(vGrib, stnV, false))) {
                    stnU.clear();
                    stnV.clear();
                } else if (ensureSlice(":GUST:surface:", gGrib)) {
                    stnGust = dir + "/stg_" + tag + ".xyz";
                    if (!sampleGrid(gGrib, stnGust, false)) {
                        stnGust.clear();
                    }
                }
            }
        }
    }

    burnLines(cwaLinesGeoJson(), 110, 110, 110);
    burnLines(stateLinesGeoJson(), 25, 25, 25);

    const auto translated = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", tiffPath, pngPath});
    for (const auto& stale : {colorPath, warpPath, scaledPath, tiffPath}) {
        QFile::remove(stale);
    }
    if (!labelledContourGeoJson.isEmpty()) {
        if (translated && QFile::exists(pngPath)) {
            labelContours(pngPath, labelledContourGeoJson, box, greenContours);
        }
        QFile::remove(labelledContourGeoJson);
    }
    if (!stnT.isEmpty()) {
        if (translated && QFile::exists(pngPath)) {
            drawStationPlots(pngPath, stationList, stnT, stnTd, stnU, stnV, stnGust, box);
        }
        for (const auto& stale : {stnT, stnTd, stnU, stnV, stnGust}) {
            QFile::remove(stale);
        }
    }
    if (!translated || !QFile::exists(pngPath)) {
        return "";
    }
    return pngPath.toStdString();
}

bool UtilityGrib::getLatestRun(string& dateStr, string& cycle) {
    return resolveLatestRun(dateStr, cycle);
}

namespace {
    // A published run+cycle+forecastHour+product's .idx text never changes
    // once NOMADS serves it - immutable, unlike a directory listing that
    // grows new entries over time (UtilitySpcPost's listDirCache needs a
    // TTL for exactly that reason; this doesn't). Every caller that fetches
    // several fields from the same file - UtilitySevereIndices' ~13 SHIP
    // inputs (2 distinct idx URLs: 2dfld + prslev), GribViewer's own
    // windBarb sub-fields - was re-downloading the identical idx text once
    // per field before this (confirmed live: the same "...f021....idx" URL
    // logged 9 separate times fetching SHIP's 2dfld inputs alone). Cached
    // for the process's lifetime, keyed by URL; a failed/empty fetch is
    // deliberately NOT cached so a transient network hiccup can still be
    // retried on the next field instead of poisoning every field after it.
    std::map<string, string> idxTextCache;
    string cachedIdxText(const string& idxUrl) {
        const auto it = idxTextCache.find(idxUrl);
        if (it != idxTextCache.end()) {
            return it->second;
        }
        auto text = UtilityIO::getHtml(idxUrl);
        if (!text.empty()) {
            idxTextCache[idxUrl] = text;
        }
        return text;
    }

    // a cached grib2 file is valid only if BOTH ends check out: a byte-range
    // fetch cut short by a network hiccup can still start with a valid
    // "GRIB" header while being truncated before GRIB2's mandatory "7777"
    // end-of-message marker - that file passed a header-only check for
    // good, kept failing every gdalwarp that touched it, and the cache
    // never re-validated it (found live, 2026-09-12: a truncated cached
    // "sp" record for f021 poisoned every SHIP render for that hour until
    // this fix). Deletes an invalid cached file so the caller re-fetches.
    bool isValidCachedGrib(const QString& path) {
        QFile check{path};
        if (check.size() <= 200 || !check.open(QIODevice::ReadOnly)) {
            return false;
        }
        const auto validHeader = check.read(4) == QByteArray{"GRIB"};
        const auto validTrailer = check.seek(check.size() - 4) && check.read(4) == QByteArray{"7777"};
        check.close();
        if (validHeader && validTrailer) {
            return true;
        }
        QFile::remove(path);
        return false;
    }
}

// same byte-range fetch as render()'s local "ensureSlice" lambda, factored out
// so renderBackground() (and any other future caller) can reuse it without
// duplicating the plain-vs-subh fallback logic
bool UtilityGrib::fetchFieldSlice(const Field& field, const string& dateStr, const string& cycle,
                                   const string& forecastHour, string& gribPathOut) {
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto gribPath = QString::fromStdString(cacheDir()) +
        QString::fromStdString("/g_" + dateStr + cycle + "_" + field.key + "_" + fhr3 + ".grib2");
    gribPathOut = gribPath.toStdString();

    if (isValidCachedGrib(gribPath)) {
        return true;
    }

    const auto plainUrl = baseDir + "/rrfs." + dateStr + "/" + cycle +
        "/rrfs.t" + cycle + "z." + field.product + ".3km.f" + fhr3 + ".conus.grib2";
    const auto subhUrl = (field.product == "2dfld")
        ? baseDir + "/rrfs." + dateStr + "/" + cycle + "/rrfs.t" + cycle + "z.2dfld.3km.subh.f" + fhr3 + ".conus.grib2"
        : string{};

    auto grab = [&] (const string& url, int stepHour) -> bool {
        const auto idx = cachedIdxText(url + ".idx");
        long long start = -1;
        long long end = -1;
        if (!idxByteRange(idx, field.idxMatch, start, end, stepHour)) {
            return false;
        }
        const auto slice = URL::getBytesRange(url, start, end);
        if (slice.size() < 200 || slice.left(4) != QByteArray{"GRIB"}) {
            return false;
        }
        QFile out{gribPath};
        if (!out.open(QIODevice::WriteOnly)) {
            return false;
        }
        out.write(slice);
        out.close();
        return true;
    };
    if (grab(plainUrl, 0)) {
        return true;
    }
    return !subhUrl.empty() && grab(subhUrl, forecastHourInt);
}

void UtilityGrib::fetchFieldSlices(const vector<Field>& fields, const string& dateStr, const string& cycle,
                                    const string& forecastHour, std::map<string, string>& gribPaths) {
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto dir = QString::fromStdString(cacheDir());
    // clusters within this many bytes of each other share one ranged GET
    // instead of one each - the user's call (2026-09-14): merging is fine
    // even when the intermediate/unwanted section is up to ~1MB, as long as
    // the fields being merged are genuinely sequential in the .idx (this is
    // resolveAgainst()'s job - it only clusters records that already sorted
    // adjacent, never reorders to force a merge). Confirmed live: HAIL to
    // PRES:surface has ~156KB of unwanted lightning fields between them and
    // is still worth merging under this cap.
    constexpr long long mergeWasteThreshold = 1000000;

    struct Resolved {
        const Field* field;
        long long start;
        long long end;
    };

    std::map<string, vector<const Field*>> byProduct;
    for (const auto& field : fields) {
        const auto gribPath = dir + QString::fromStdString("/g_" + dateStr + cycle + "_" + field.key + "_" + fhr3 + ".grib2");
        if (isValidCachedGrib(gribPath)) {
            gribPaths[field.key] = gribPath.toStdString();
            continue;
        }
        byProduct[field.product].push_back(&field);
    }

    for (const auto& entry : byProduct) {
        const auto& product = entry.first;
        auto remaining = entry.second;   // trimmed as fields resolve below
        const auto plainUrl = baseDir + "/rrfs." + dateStr + "/" + cycle +
            "/rrfs.t" + cycle + "z." + product + ".3km.f" + fhr3 + ".conus.grib2";
        const auto subhUrl = (product == "2dfld")
            ? baseDir + "/rrfs." + dateStr + "/" + cycle + "/rrfs.t" + cycle + "z.2dfld.3km.subh.f" + fhr3 + ".conus.grib2"
            : string{};

        auto resolveAgainst = [&] (const string& url, int stepHour, vector<const Field*>& stillWanted) {
            vector<Resolved> resolved;
            const auto idx = cachedIdxText(url + ".idx");
            for (auto it = stillWanted.begin(); it != stillWanted.end();) {
                long long start = -1;
                long long end = -1;
                if (idxByteRange(idx, (*it)->idxMatch, start, end, stepHour)) {
                    resolved.push_back({*it, start, end});
                    it = stillWanted.erase(it);
                } else {
                    ++it;
                }
            }
            return resolved;
        };

        auto writeSlice = [&] (const Field& field, const QByteArray& slice) {
            if (slice.size() < 200 || slice.left(4) != QByteArray{"GRIB"}) {
                return;
            }
            const auto gribPath = dir + QString::fromStdString("/g_" + dateStr + cycle + "_" + field.key + "_" + fhr3 + ".grib2");
            QFile out{gribPath};
            if (!out.open(QIODevice::WriteOnly)) {
                return;
            }
            out.write(slice);
            out.close();
            gribPaths[field.key] = gribPath.toStdString();
        };

        auto fetchResolved = [&] (const string& url, vector<Resolved> resolved) {
            std::sort(resolved.begin(), resolved.end(),
                      [] (const Resolved& a, const Resolved& b) { return a.start < b.start; });
            size_t i = 0;
            while (i < resolved.size()) {
                size_t j = i;
                auto clusterEnd = resolved[i].end;
                while (j + 1 < resolved.size() && resolved[j + 1].start - clusterEnd < mergeWasteThreshold) {
                    j += 1;
                    clusterEnd = std::max(clusterEnd, resolved[j].end);
                }
                const auto clusterStart = resolved[i].start;
                const auto blob = URL::getBytesRange(url, clusterStart, clusterEnd);
                for (size_t k = i; k <= j; k += 1) {
                    const auto offset = resolved[k].start - clusterStart;
                    const auto length = resolved[k].end - resolved[k].start + 1;
                    if (offset >= 0 && offset + length <= blob.size()) {
                        writeSlice(*resolved[k].field, blob.mid(static_cast<int>(offset), static_cast<int>(length)));
                    }
                }
                i = j + 1;
            }
        };

        const auto plainResolved = resolveAgainst(plainUrl, 0, remaining);
        if (!plainResolved.empty()) {
            fetchResolved(plainUrl, plainResolved);
        }
        if (!remaining.empty() && !subhUrl.empty()) {
            const auto subhResolved = resolveAgainst(subhUrl, forecastHourInt, remaining);
            if (!subhResolved.empty()) {
                fetchResolved(subhUrl, subhResolved);
            }
        }
    }
}

string UtilityGrib::renderBackground(const string& kind, const Bbox& bbox, const string& forecastHour,
                                      const string& runId, string& status) {
    const auto binDir = gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return "";
    }
    int fieldIndex = -1;
    const auto wantKey = (kind == "temp") ? string{"tmp2m"} : string{"refc"};
    for (size_t i = 0; i < fields.size(); i += 1) {
        if (fields[i].key == wantKey) {
            fieldIndex = static_cast<int>(i);
            break;
        }
    }
    if (fieldIndex < 0) {
        status = "unknown background field";
        return "";
    }
    const auto& field = fields[fieldIndex];
    const bool toFahrenheit = UIPreferences::unitsF && field.units == "C";

    string dateStr;
    string cycle;
    if (runId.size() == 10) {
        dateStr = runId.substr(0, 8);
        cycle = runId.substr(8, 2);
    } else if (!resolveLatestRun(dateStr, cycle)) {
        status = "no RRFS run available";
        return "";
    }
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto runKey = dateStr + cycle;
    const auto dir = QString::fromStdString(cacheDir());

    // cached per run + field + bbox + fhr - the bbox is baked into the cache
    // key since callers pass an arbitrary domain, not one of UtilityGrib's
    // own named regions. "bg1" is the render version.
    const auto bboxTag = QString::number(bbox.west, 'f', 2) + "_" + QString::number(bbox.south, 'f', 2) +
        "_" + QString::number(bbox.east, 'f', 2) + "_" + QString::number(bbox.north, 'f', 2);
    const auto pngPath = dir + QString::fromStdString("/bg1_" + runKey + "_" + field.key + "_") + bboxTag +
        QString::fromStdString("_" + fhr3 + (toFahrenheit ? "_f" : "") + ".png");
    if (QFile::exists(pngPath)) {
        return pngPath.toStdString();
    }

    string gribPathStr;
    if (!fetchFieldSlice(field, dateStr, cycle, fhr3, gribPathStr)) {
        status = field.label + " background not available for f" + fhr3;
        return "";
    }
    const auto gribPath = QString::fromStdString(gribPathStr);

    const auto tag = runKey + "_" + field.key + "_" + bboxTag.toStdString() + "_" + fhr3;
    const auto colorPath = dir + QString::fromStdString("/bgcol_" + tag + ".txt");
    const auto warpPath = dir + QString::fromStdString("/bgw_" + tag + ".tif");
    const auto scaledPath = dir + QString::fromStdString("/bgf_" + tag + ".tif");
    const auto isReflectivity = (kind == "reflectivity");
    {
        const auto table = isReflectivity ? reflectivityToGreyBase(field.colorMap)
                          : toFahrenheit ? colorMapToFahrenheit(field.colorMap) : field.colorMap;
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(table.c_str(), static_cast<qint64>(table.size()));
            colorFile.close();
        }
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    const auto gdalCalc = bin + (QFile::exists(bin + "gdal_calc") ? "gdal_calc" : "gdal_calc.py");
    auto runProcess = [&status] (const QString& program, const QStringList& args) {
        QProcess process;
        process.start(program, args);
        process.waitForFinished(30000);
        const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!ok) {
            status = "gdal failed: " + process.readAllStandardError().left(200).toStdString();
        }
        return ok;
    };

    // a background layer only needs to look good, not support hover sampling -
    // half the main pipeline's resolution keeps it cheap (and inherits the
    // same domain-span tiering, so a CONUS-wide background scales down too)
    const auto bgCols = QString::number(mainRenderColumns(bbox) / 2);
    auto ok = runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(bbox.west), fixedQ(bbox.south), fixedQ(bbox.east), fixedQ(bbox.north),
             "-r", "bilinear", "-ts", bgCols, "0", gribPath, warpPath});
    QString fillTif = warpPath;
    if (ok && toFahrenheit) {
        if (runProcess(gdalCalc, {"-A", warpPath, "--calc=A*1.8+32", "--NoDataValue=-9999",
                                  "--outfile=" + scaledPath, "--overwrite", "--quiet"})) {
            fillTif = scaledPath;
        }
    }
    // gdaldem's "-alpha" does NOT make recognised-nodata pixels transparent
    // (verified: they come out clamped-opaque to the nearest colour stop) -
    // build an explicit 0/255 mask from the warp's own nodata and merge it in
    // as the real alpha band. RRFS's Lambert grid inscribed in a lat/lon bbox
    // always leaves nodata corners outside its native coverage, and those
    // need to be transparent for a background layer, not an odd solid colour.
    const auto maskPath = dir + QString::fromStdString("/bgm_" + tag + ".tif");
    const auto rgbPath = dir + QString::fromStdString("/bgr_" + tag + ".tif");
    const auto mergedPath = dir + QString::fromStdString("/bgmg_" + tag + ".tif");
    const auto gdalMerge = bin + (QFile::exists(bin + "gdal_merge") ? "gdal_merge" : "gdal_merge.py");
    ok = ok && runProcess(gdalCalc, {"-A", fillTif, "--calc=255*(A!=-9999)", "--outfile=" + maskPath,
                                      "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"});
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-of", "GTiff", fillTif, colorPath, rgbPath});
    ok = ok && runProcess(gdalMerge, {"-q", "-o", mergedPath, "-separate", "-co", "PHOTOMETRIC=RGB", rgbPath, maskPath});
    const auto translated = ok && runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", mergedPath, pngPath});
    for (const auto& stale : {colorPath, warpPath, scaledPath, maskPath, rgbPath, mergedPath}) {
        QFile::remove(stale);
    }
    if (!translated || !QFile::exists(pngPath)) {
        return "";
    }
    if (isReflectivity) {
        QImage image{pngPath};
        if (!image.isNull()) {
            hatchLowReflectivity(image);
            image.save(pngPath, "PNG");
        }
    }
    return pngPath.toStdString();
}
