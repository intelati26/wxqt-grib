// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/RenderLock.h"
#include "models/UtilitySevereIndices.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include <QVector>
#include "objects/WString.h"
#include "util/To.h"

namespace {
    // CONUS, matching UtilityGrib::regionTable / UtilitySpcPost::domains
    const UtilityGrib::Bbox conus{-125.0, 24.0, -66.0, 50.0};

    QString fixedQ(double value) {
        return QString::number(value, 'f', 3);
    }

    bool runProcess(const QString& program, const QStringList& args, QByteArray* stdOut = nullptr) {
        QProcess process;
        process.start(program, args);
        process.waitForFinished(30000);
        const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (stdOut) {
            *stdOut = process.readAllStandardOutput();
        }
        return ok;
    }

    // gdalinfo -mm's "Computed Min/Max" line, respecting NoData
    bool computedMinMax(const QString& bin, const QString& path, double& dataMin, double& dataMax) {
        QByteArray info;
        if (!runProcess(bin + "gdalinfo", {"-mm", path}, &info)) {
            return false;
        }
        const auto text = QString::fromUtf8(info);
        const auto match = QRegularExpression{R"(Computed Min/Max=(-?[0-9.]+),(-?[0-9.]+))"}.match(text);
        if (!match.hasMatch()) {
            return false;
        }
        dataMin = match.captured(1).toDouble();
        dataMax = match.captured(2).toDouble();
        return true;
    }

    // auto-scaled grayscale PNG, for eyeballing a raw or derived grid
    bool dumpGrayscalePng(const QString& bin, const QString& tifPath, const QString& pngPath) {
        return runProcess(bin + "gdal_translate",
                {"-q", "-of", "PNG", "-ot", "Byte", "-scale", "-a_nodata", "0", tifPath, pngPath});
    }

    // hover sample-grid column count - same tiering as UtilityGrib's own
    // sampleGridColumns() (a small enough function that a fourth copy here
    // is fine, unlike mainRenderColumns' geometry math which is shared)
    int sampleGridColumns(const UtilityGrib::Bbox& box) {
        const auto span = std::max(box.east - box.west, box.north - box.south);
        if (span > 30.0) {
            return 220;
        }
        if (span > 15.0) {
            return 350;
        }
        return 450;
    }

    // SHIP's own interpretation breakpoints (see docs/derived-severe-indices-plan.md):
    // <1 not significant, 1-4 favorable for significant hail, >4 very high
    const string shipColorMap{
        "0 0 0 40\n"
        "0.5 40 60 200\n"
        "1.0 60 170 90\n"
        "1.5 230 220 60\n"
        "2.0 240 150 40\n"
        "3.0 220 40 40\n"
        "4.0 170 30 150\n"
        "6.0 255 255 255\n"};

    // STP's own typical range runs roughly 0-10 (vs SHIP's 0-6) - same
    // 8-stop escalating style, breakpoints picked to match SPC's published
    // STP interpretation (>1 = most sig. tornadoes, <1 = most non-tor.
    // supercells).
    const string stpColorMap{
        "0.0 0 0 40\n"
        "0.5 40 60 200\n"
        "1.0 60 170 90\n"
        "2.0 230 220 60\n"
        "3.0 240 150 40\n"
        "5.0 220 40 40\n"
        "8.0 170 30 150\n"
        "12.0 255 255 255\n"};
}

// The "Parametric" combo's row table (mirrors UtilityGrib::fields) - SHIP
// and fixed-layer STP; effective-layer STP deferred (see
// docs/refs-viewer-plan.md-style note in docs/derived-severe-indices-plan.md
// - it needs a full vertical-profile parcel computation RRFS doesn't ship as
// ready fields). colorMap lets IndexViewer build a real legend the same way
// GribViewer does from UtilityGrib::fields[i].colorMap, rather than a
// label-only list.
const vector<UtilitySevereIndices::IndexEntry> UtilitySevereIndices::indices{
    {"SHIP (Significant Hail Parameter)", "ship", "index", shipColorMap},
    {"STP (Significant Tornado Parameter, fixed layer)", "stp", "index", stpColorMap},
};

// SHIP's raw inputs (see docs/derived-severe-indices-plan.md field-mapping
// table, verified against a live RRFS run 2026-09-12). No colorMap needed -
// this is a debug dump, auto-scaled per field rather than styled.
const vector<UtilityGrib::Field> UtilitySevereIndices::shipInputFields{
    {"HAILCAST Max Diameter", "hail", "m", ":HAIL:surface:", ""},
    {"MU CAPE (180-0mb)", "mucape", "J/kg", ":CAPE:180-0 mb above ground:", ""},
    {"2m Temperature", "t2", "C", ":TMP:2 m above ground:", ""},
    {"2m Dewpoint", "d2", "C", ":DPT:2 m above ground:", ""},
    {"Surface Pressure", "sp", "Pa", ":PRES:surface:", ""},
    {"0-6km Shear U", "ushr6", "m/s", ":VUCSH:0-6000 m above ground:", ""},
    {"0-6km Shear V", "vshr6", "m/s", ":VVCSH:0-6000 m above ground:", ""},
    {"0C Isotherm Height", "fzlmsl", "m", ":HGT:0C isotherm:", ""},
    {"Terrain Height", "terrain", "m", ":HGT:surface:", ""},
    {"700mb Temperature", "t700", "C", ":TMP:700 mb:", "", "prslev"},
    {"500mb Temperature", "t500", "C", ":TMP:500 mb:", "", "prslev"},
    {"700mb Height", "hgt700", "gpm", ":HGT:700 mb:", "", "prslev"},
    {"500mb Height", "hgt500", "gpm", ":HGT:500 mb:", "", "prslev"},
};

// STP (fixed layer)'s inputs NOT already in shipInputFields - 0-6km shear
// (ushr6/vshr6) and terrain height are shared, reused directly by index
// rather than redefined here (see computeStpGrid()). Verified against a
// live RRFS run 2026-09-14.
const vector<UtilityGrib::Field> UtilitySevereIndices::stpInputFields{
    {"Surface CAPE", "sbcape", "J/kg", ":CAPE:surface:", ""},
    {"Surface CIN", "sbcin", "J/kg", ":CIN:surface:", ""},
    {"0-1km Storm Relative Helicity", "srh1", "m2/s2", ":HLCY:1000-0 m above ground:", ""},
    {"LCL Height (MSL)", "lcl", "m", ":HGT:level of adiabatic condensation from sfc:", ""},
};

bool UtilitySevereIndices::gdalAvailable() {
    return !UtilityGrib::gdalBinDir().empty();
}

string UtilitySevereIndices::cacheDir() {
    auto path = QDir::tempPath() + "/wxqt_severeindices";
    QDir{}.mkpath(path);
    return path.toStdString();
}

bool UtilitySevereIndices::fetchAndWarpAll(const vector<UtilityGrib::Field>& wanted, const UtilityGrib::Bbox& box,
                                            const string& forecastHour, const string& runId, string& runKeyOut,
                                            string& fhr3Out, std::map<string, QString>& warpPaths, string& status) {
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return false;
    }

    string dateStr;
    string cycle;
    if (!resolveSynopticRun(runId, dateStr, cycle)) {
        status = "no RRFS run available";
        return false;
    }
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto runKey = dateStr + cycle;
    const auto dir = QString::fromStdString(cacheDir());
    const auto bin = QString::fromStdString(binDir) + "/";
    const auto cols = QString::number(UtilityGrib::mainRenderColumns(box));

    // NOMADS pacing for these fetches is handled once, centrally, by
    // URL.cpp's per-host throttle - no sleep needed here. fetchFieldSlices
    // batches the idx-text fetch (one per product file, not one per field)
    // and merges genuinely-adjacent fields' byte ranges into single ranged
    // GETs where it's worth the extra bytes - see its own header comment.
    std::map<string, string> gribPaths;
    UtilityGrib::fetchFieldSlices(wanted, dateStr, cycle, fhr3, gribPaths);

    for (const auto& field : wanted) {
        const auto found = gribPaths.find(field.key);
        if (found == gribPaths.end()) {
            status = field.label + " not available for f" + fhr3;
            return false;
        }
        const auto gribPath = QString::fromStdString(found->second);
        const auto tag = runKey + "_" + field.key + "_" + fhr3;
        const auto warpPath = dir + QString::fromStdString("/w_" + tag + ".tif");

        const auto warpOk = runProcess(bin + "gdalwarp",
                {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
                 "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
                 "-r", "bilinear", "-ts", cols, "0", gribPath, warpPath});
        if (!warpOk) {
            status = "gdalwarp failed for " + field.label;
            return false;
        }
        warpPaths[field.key] = warpPath;
    }

    runKeyOut = runKey;
    fhr3Out = fhr3;
    return true;
}

vector<UtilitySevereIndices::FieldCheck> UtilitySevereIndices::debugFetchShipInputs(
        const string& forecastHour, const string& runId, string& status) {
    vector<FieldCheck> results;
    const auto binDir = UtilityGrib::gdalBinDir();
    const auto bin = QString::fromStdString(binDir) + "/";

    string runKey;
    string fhr3;
    std::map<string, QString> warpPaths;
    // fetchAndWarpAll bails out on the first missing field, but for this
    // debug dump we want a per-field pass/fail report instead - fetch one
    // field at a time so a missing HAIL record (say) doesn't hide whether
    // the other 12 are fine.
    for (const auto& field : shipInputFields) {
        FieldCheck check;
        check.label = field.label;
        check.key = field.key;
        check.units = field.units;

        string oneStatus;
        std::map<string, QString> onePath;
        if (!fetchAndWarpAll({field}, conus, forecastHour, runId, runKey, fhr3, onePath, oneStatus)) {
            check.error = oneStatus;
            results.push_back(check);
            continue;
        }
        const auto warpPath = onePath[field.key];
        computedMinMax(bin, warpPath, check.dataMin, check.dataMax);

        const auto pngPath = QString::fromStdString(cacheDir()) +
            QString::fromStdString("/p_" + runKey + "_" + field.key + "_" + fhr3 + ".png");
        const auto pngOk = dumpGrayscalePng(bin, warpPath, pngPath);
        QFile::remove(warpPath);
        QFile::remove(warpPath + ".aux.xml");
        if (!pngOk) {
            check.error = "gdal_translate failed";
            results.push_back(check);
            continue;
        }

        check.ok = true;
        check.pngPath = pngPath.toStdString();
        results.push_back(check);
    }

    status = "fetched " + To::string(static_cast<int>(results.size())) + " fields for " + runKey + " f" + fhr3;
    return results;
}

vector<UtilitySevereIndices::DerivedCheck> UtilitySevereIndices::debugComputeDerivedGrids(
        const string& forecastHour, const string& runId, string& status) {
    vector<DerivedCheck> results;
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return results;
    }
    const auto bin = QString::fromStdString(binDir) + "/";

    const vector<UtilityGrib::Field> needed{
        shipInputFields[3],   // d2
        shipInputFields[4],   // sp
        shipInputFields[5],   // ushr6
        shipInputFields[6],   // vshr6
        shipInputFields[7],   // fzlmsl
        shipInputFields[8],   // terrain
        shipInputFields[9],   // t700
        shipInputFields[10],  // t500
        shipInputFields[11],  // hgt700
        shipInputFields[12],  // hgt500
    };
    string runKey;
    string fhr3;
    std::map<string, QString> warp;
    if (!fetchAndWarpAll(needed, conus, forecastHour, runId, runKey, fhr3, warp, status)) {
        return results;
    }
    const auto dir = QString::fromStdString(cacheDir());
    const auto tagBase = runKey + "_" + fhr3;

    auto compute = [&] (const string& key, const string& label, const string& units, const string& formula,
                         const QStringList& inputs, const UtilityGrib::CalcFn& fn) {
        DerivedCheck check;
        check.label = label;
        check.key = key;
        check.units = units;
        check.formula = formula;

        const auto outPath = dir + QString::fromStdString("/d_" + tagBase + "_" + key + ".tif");
        if (!UtilityGrib::calcRaster(bin, inputs, fn, outPath)) {
            check.error = "raster calc failed";
            results.push_back(check);
            return;
        }
        computedMinMax(bin, outPath, check.dataMin, check.dataMax);
        const auto pngPath = dir + QString::fromStdString("/dp_" + tagBase + "_" + key + ".png");
        if (!dumpGrayscalePng(bin, outPath, pngPath)) {
            check.error = "gdal_translate failed";
            QFile::remove(outPath);
            results.push_back(check);
            return;
        }
        QFile::remove(outPath);
        check.ok = true;
        check.pngPath = pngPath.toStdString();
        results.push_back(check);
    };

    // mixing ratio from dewpoint (Bolton 1980 saturation vapor pressure at
    // Td, i.e. actual vapor pressure since Td already reflects actual
    // moisture) and surface pressure - sp is native Pa, divide by 100 for
    // the hPa this formula wants. Output in g/kg (the 622 folds in the
    // 0.622 molecular-weight ratio and the kg->g *1000).
    compute("mixr", "Mixing Ratio (surface)", "g/kg",
            "622*e/((sp/100)-e), e=6.112*exp(17.67*Td/(Td+243.5))",
            {warp["d2"], warp["sp"]},
            [](const double* v) { const auto e = 6.112 * std::exp(17.67 * v[0] / (v[0] + 243.5)); return 622.0 * e / ((v[1] / 100.0) - e); });

    // 700-500mb lapse rate via direct height difference (both HGT and TMP
    // already fetched at both levels, so no hypsometric approximation needed)
    compute("lr75", "700-500mb Lapse Rate", "C/km",
            "(T700-T500)/((HGT500-HGT700)/1000)",
            {warp["t700"], warp["t500"], warp["hgt700"], warp["hgt500"]},
            [](const double* v) { return (v[0] - v[1]) / ((v[3] - v[2]) / 1000.0); });

    // 0-6km shear magnitude from native U/V components
    compute("shear6", "0-6km Shear Magnitude", "m/s",
            "sqrt(ushr6^2 + vshr6^2)",
            {warp["ushr6"], warp["vshr6"]},
            [](const double* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); });

    // freezing level height above ground (both already MSL/geopotential-consistent)
    compute("fzlagl", "Freezing Level AGL", "m",
            "HGT:0C isotherm - HGT:surface",
            {warp["fzlmsl"], warp["terrain"]},
            [](const double* v) { return v[0] - v[1]; });

    for (const auto& entry : warp) {
        QFile::remove(entry.second);
        QFile::remove(entry.second + ".aux.xml");
    }

    status = "computed " + To::string(static_cast<int>(results.size())) + " derived grids for " + runKey + " f" + fhr3;
    return results;
}

UtilitySevereIndices::ShipGrid UtilitySevereIndices::computeShipGrid(
        const UtilityGrib::Bbox& box, const string& forecastHour, const string& runId) {
    ShipGrid result;
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        result.error = "GDAL not found - install the 'gdal' package";
        return result;
    }
    const auto bin = QString::fromStdString(binDir) + "/";

    const vector<UtilityGrib::Field> needed{
        shipInputFields[1],   // mucape
        shipInputFields[3],   // d2
        shipInputFields[4],   // sp
        shipInputFields[5],   // ushr6
        shipInputFields[6],   // vshr6
        shipInputFields[7],   // fzlmsl
        shipInputFields[8],   // terrain
        shipInputFields[9],   // t700
        shipInputFields[10],  // t500
        shipInputFields[11],  // hgt700
        shipInputFields[12],  // hgt500
    };
    string runKey;
    string fhr3;
    std::map<string, QString> warp;
    if (!fetchAndWarpAll(needed, box, forecastHour, runId, runKey, fhr3, warp, result.error)) {
        return result;
    }
    result.runKey = runKey;
    result.fhr3 = fhr3;
    const auto dir = QString::fromStdString(cacheDir());
    // bbox baked into the intermediate-file tag - two different regions at
    // the same run/hour must not collide on the same scratch filenames
    const auto bboxTag = QString::number(box.west, 'f', 2) + "_" + QString::number(box.south, 'f', 2) +
        "_" + QString::number(box.east, 'f', 2) + "_" + QString::number(box.north, 'f', 2);
    const auto tagBase = runKey + "_" + fhr3 + "_" + bboxTag.toStdString();

    auto calcTo = [&] (const string& key, const QStringList& inputs, const UtilityGrib::CalcFn& fn) -> QString {
        const auto outPath = dir + QString::fromStdString("/s_" + tagBase + "_" + key + ".tif");
        return UtilityGrib::calcRaster(bin, inputs, fn, outPath) ? outPath : QString{};
    };

    const auto mixrPath = calcTo("mixr",
            {warp["d2"], warp["sp"]},
            [](const double* v) { const auto e = 6.112 * std::exp(17.67 * v[0] / (v[0] + 243.5)); return 622.0 * e / ((v[1] / 100.0) - e); });
    const auto lr75Path = calcTo("lr75",
            {warp["t700"], warp["t500"], warp["hgt700"], warp["hgt500"]},
            [](const double* v) { return (v[0] - v[1]) / ((v[3] - v[2]) / 1000.0); });
    const auto shear6Path = calcTo("shear6",
            {warp["ushr6"], warp["vshr6"]},
            [](const double* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); });

    QString shipPath;
    if (!mixrPath.isEmpty() && !lr75Path.isEmpty() && !shear6Path.isEmpty()) {
        // SHIP = MUCAPE * MIXR * LR75 * (-T500) * SHEAR6 / 42,000,000, with
        // SHEAR6 clamped to [7,27] m/s, MIXR clamped to [11,13.6] g/kg, T500
        // capped at -5.5 C (warmer values clamped down to -5.5); then three
        // conditional rescalers multiply the result down when MUCAPE, LR75,
        // or the freezing level are on the low side. Computed per pixel in
        // C++ via calcRaster (the former gdal_calc expression, ported 1:1).
        shipPath = calcTo("ship",
                {warp["mucape"], mixrPath, lr75Path, warp["t500"], shear6Path, warp["fzlmsl"], warp["terrain"]},
                [](const double* v) {
                    const auto cape = v[0], lr75 = v[2], frz = v[5] - v[6];
                    return (cape * std::clamp(v[1], 11.0, 13.6) * lr75 * (-std::min(v[3], -5.5)) *
                            std::clamp(v[4], 7.0, 27.0) / 42000000.0)
                        * (cape < 1300.0 ? cape / 1300.0 : 1.0) * (lr75 < 5.8 ? lr75 / 5.8 : 1.0)
                        * (frz < 2400.0 ? frz / 2400.0 : 1.0);
                });
    }

    for (const auto& path : {mixrPath, lr75Path, shear6Path}) {
        QFile::remove(path);
        QFile::remove(path + ".aux.xml");
    }
    // keep one raw input warp around (renamed so it survives the sweep
    // below) - gdal_calc doesn't propagate NoData through the formula's
    // arithmetic, so shipPath's own values can't distinguish true
    // off-domain nodata from real data; this untouched warp still can.
    const auto nodataRefPath = dir + QString::fromStdString("/nd_" + tagBase + ".tif");
    if (!shipPath.isEmpty() && warp.count("mucape")) {
        QFile::rename(warp["mucape"], nodataRefPath);
        warp.erase("mucape");
    }
    for (const auto& entry : warp) {
        QFile::remove(entry.second);
        QFile::remove(entry.second + ".aux.xml");
    }

    if (shipPath.isEmpty()) {
        QFile::remove(nodataRefPath);
        result.error = "SHIP formula calculation failed";
        return result;
    }
    computedMinMax(bin, shipPath, result.dataMin, result.dataMax);
    result.shipTifPath = shipPath;
    result.nodataRefPath = nodataRefPath;

    // HAILCAST max-diameter contour geometry, meters -> inches - same
    // coarse-warp/smooth/burn/label recipe UtilityGrib uses for height-field
    // contours, except nearest-neighbor at the fill's own resolution instead
    // of a coarse cubicspline: HAILCAST is a spiky, storm-scale field, and
    // the smooth-field recipe was verified (2026-09-12) to smooth a real
    // 2.56in peak down to 0.59in, below every contour level.
    string hailGribPath;
    if (UtilityGrib::fetchFieldSlice(shipInputFields[0], runKey.substr(0, 8), runKey.substr(8, 2), fhr3, hailGribPath)) {
        const auto coarsePath = dir + QString::fromStdString("/hc_" + tagBase + ".tif");
        const auto inchPath = dir + QString::fromStdString("/hi_" + tagBase + ".tif");
        const auto contourRaw = dir + QString::fromStdString("/hr_" + tagBase + ".geojson");
        const auto contourBuf = dir + QString::fromStdString("/hb_" + tagBase + ".geojson");
        for (const auto& stale : {contourRaw, contourBuf}) {
            QFile::remove(stale);
        }
        const auto cols = QString::number(UtilityGrib::mainRenderColumns(box));
        const auto coarseOk = runProcess(bin + "gdalwarp",
                {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
                 "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
                 "-r", "near", "-ts", cols, "0", QString::fromStdString(hailGribPath), coarsePath})
            && UtilityGrib::calcRaster(bin, {coarsePath}, [](const double* v) { return v[0] * 39.3701; }, inchPath);
        if (coarseOk
            && runProcess(bin + "gdal_contour",
                {"-q", "-a", "elev", "-fl", "0.75", "-fl", "1.0", "-fl", "1.5", "-fl", "2.0", "-fl", "2.5",
                 inchPath, contourRaw})
            && runProcess(bin + "ogr2ogr",
                {"-q", "-f", "GeoJSON", contourBuf, contourRaw, "-dialect", "sqlite",
                 "-sql", "SELECT ST_Buffer(ST_SimplifyPreserveTopology(geometry, 0.06), 0.02) AS geometry FROM contour"})) {
            result.hailContourBufPath = contourBuf;
            result.hailContourRawPath = contourRaw;
        }
        QFile::remove(coarsePath);
        QFile::remove(inchPath);
    }

    result.ok = true;
    return result;
}

UtilitySevereIndices::StpGrid UtilitySevereIndices::computeStpGrid(
        const UtilityGrib::Bbox& box, const string& forecastHour, const string& runId) {
    StpGrid result;
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        result.error = "GDAL not found - install the 'gdal' package";
        return result;
    }
    const auto bin = QString::fromStdString(binDir) + "/";

    const vector<UtilityGrib::Field> needed{
        stpInputFields[0],   // sbcape
        stpInputFields[1],   // sbcin
        stpInputFields[2],   // srh1
        stpInputFields[3],   // lcl (MSL - subtracted to AGL below)
        shipInputFields[5],  // ushr6
        shipInputFields[6],  // vshr6
        shipInputFields[8],  // terrain
    };
    string runKey;
    string fhr3;
    std::map<string, QString> warp;
    if (!fetchAndWarpAll(needed, box, forecastHour, runId, runKey, fhr3, warp, result.error)) {
        return result;
    }
    result.runKey = runKey;
    result.fhr3 = fhr3;
    const auto dir = QString::fromStdString(cacheDir());
    // bbox baked into the intermediate-file tag, same reasoning as SHIP's
    // own tagBase - two different regions at the same run/hour must not
    // collide on the same scratch filenames. "_stp" suffix keeps SHIP's and
    // STP's scratch files from colliding with each other too.
    const auto bboxTag = QString::number(box.west, 'f', 2) + "_" + QString::number(box.south, 'f', 2) +
        "_" + QString::number(box.east, 'f', 2) + "_" + QString::number(box.north, 'f', 2);
    const auto tagBase = runKey + "_" + fhr3 + "_" + bboxTag.toStdString() + "_stp";

    auto calcTo = [&] (const string& key, const QStringList& inputs, const UtilityGrib::CalcFn& fn) -> QString {
        const auto outPath = dir + QString::fromStdString("/s_" + tagBase + "_" + key + ".tif");
        return UtilityGrib::calcRaster(bin, inputs, fn, outPath) ? outPath : QString{};
    };

    const auto shear6Path = calcTo("shear6",
            {warp["ushr6"], warp["vshr6"]},
            [](const double* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1]); });
    // "level of adiabatic condensation from sfc" is geopotential HGT like
    // every other HGT: field in this file (MSL) - same convention SHIP
    // already relies on for its own freezing-level-AGL term - subtract
    // terrain to get the AGL height the sbLCL term actually wants.
    const auto lclAglPath = calcTo("lclagl",
            {warp["lcl"], warp["terrain"]},
            [](const double* v) { return v[0] - v[1]; });

    QString stpPath;
    if (!shear6Path.isEmpty() && !lclAglPath.isEmpty()) {
        // STP (fixed layer, Thompson et al. 2003) = (SBCAPE/1500) x
        // LCL-term x (SRH1/150) x shear-term x CIN-term, each conditional
        // term capped per SPC's published breakpoints
        // (spc.noaa.gov/exper/mesoanalysis/help/help_stor.html) - same
        // where()-chain idiom the SHIP formula already uses.
        stpPath = calcTo("stp",
                {warp["sbcape"], lclAglPath, warp["srh1"], shear6Path, warp["sbcin"]},
                [](const double* v) {
                    const auto lcl = v[1], shear = v[3], cin = v[4];
                    return (v[0] / 1500.0)
                        * (lcl < 1000.0 ? 1.0 : (lcl > 2000.0 ? 0.0 : (2000.0 - lcl) / 1000.0))
                        * (v[2] / 150.0)
                        * (shear > 30.0 ? 1.5 : (shear < 12.5 ? 0.0 : shear / 20.0))
                        * (cin > -50.0 ? 1.0 : (cin < -200.0 ? 0.0 : (200.0 + cin) / 150.0));
                });
    }

    for (const auto& path : {shear6Path, lclAglPath}) {
        QFile::remove(path);
        QFile::remove(path + ".aux.xml");
    }
    // keep one raw input warp around (renamed so it survives the cleanup
    // below), same nodata-masking reasoning as SHIP's own nodataRefPath.
    const auto nodataRefPath = dir + QString::fromStdString("/nd_" + tagBase + ".tif");
    if (!stpPath.isEmpty() && warp.count("sbcape")) {
        QFile::rename(warp["sbcape"], nodataRefPath);
        warp.erase("sbcape");
    }
    for (const auto& entry : warp) {
        QFile::remove(entry.second);
        QFile::remove(entry.second + ".aux.xml");
    }

    if (stpPath.isEmpty()) {
        QFile::remove(nodataRefPath);
        result.error = "STP formula calculation failed";
        return result;
    }
    computedMinMax(bin, stpPath, result.dataMin, result.dataMax);
    result.tifPath = stpPath;
    result.nodataRefPath = nodataRefPath;
    result.ok = true;
    return result;
}

UtilitySevereIndices::ShipCheck UtilitySevereIndices::debugComputeShip(
        const string& forecastHour, const string& runId, string& status) {
    ShipCheck result;
    const auto grid = computeShipGrid(conus, forecastHour, runId);
    if (!grid.ok) {
        status = grid.error;
        result.error = grid.error;
        return result;
    }
    result.dataMin = grid.dataMin;
    result.dataMax = grid.dataMax;

    const auto bin = QString::fromStdString(UtilityGrib::gdalBinDir()) + "/";
    const auto dir = QString::fromStdString(cacheDir());
    const auto tagBase = grid.runKey + "_" + grid.fhr3;

    const auto grayPath = dir + QString::fromStdString("/ship_gray_" + tagBase + ".png");
    dumpGrayscalePng(bin, grid.shipTifPath, grayPath);
    result.grayPngPath = grayPath.toStdString();

    const auto colorPath = dir + QString::fromStdString("/shipcolor_" + tagBase + ".txt");
    {
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(shipColorMap.c_str(), static_cast<qint64>(shipColorMap.size()));
            colorFile.close();
        }
    }
    // color-relief to a GeoTIFF (not straight to PNG) so the HAILCAST
    // contour lines can be burned in with correct georeferencing
    const auto coloredTif = dir + QString::fromStdString("/ship_rgb_" + tagBase + ".tif");
    const auto colorOk = runProcess(bin + "gdaldem",
            {"color-relief", "-q", "-of", "GTiff", grid.shipTifPath, colorPath, coloredTif});
    QFile::remove(colorPath);
    QFile::remove(grid.shipTifPath);
    QFile::remove(grid.shipTifPath + ".aux.xml");
    QFile::remove(grid.nodataRefPath);   // debug dump doesn't do nodata masking

    if (!colorOk) {
        status = "gdaldem color-relief failed";
        result.error = status;
        for (const auto& stale : {grid.hailContourBufPath, grid.hailContourRawPath}) {
            QFile::remove(stale);
        }
        return result;
    }

    if (!grid.hailContourBufPath.isEmpty()) {
        runProcess(bin + "gdal_rasterize",
                {"-q", "-b", "1", "-b", "2", "-b", "3", "-burn", "255", "-burn", "255", "-burn", "255",
                 grid.hailContourBufPath, coloredTif});
        QFile::remove(grid.hailContourBufPath);
    }

    const auto colorizedPath = dir + QString::fromStdString("/ship_color_" + tagBase + ".png");
    const auto translated = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", coloredTif, colorizedPath});
    QFile::remove(coloredTif);
    QFile::remove(coloredTif + ".aux.xml");

    if (!translated) {
        status = "gdal_translate failed";
        result.error = status;
        QFile::remove(grid.hailContourRawPath);
        return result;
    }

    if (!grid.hailContourRawPath.isEmpty()) {
        UtilityGrib::labelContours(colorizedPath, grid.hailContourRawPath, conus, true, 2, "\"");
        QFile::remove(grid.hailContourRawPath);
    }

    result.ok = true;
    result.pngPath = colorizedPath.toStdString();
    status = "computed SHIP for " + grid.runKey + " f" + grid.fhr3 +
        (grid.hailContourRawPath.isEmpty() ? " (no hail contours)" : " with hail contours");
    return result;
}

vector<string> UtilitySevereIndices::indexLabels() {
    vector<string> labels;
    for (const auto& entry : indices) {
        labels.push_back(entry.label);
    }
    return labels;
}

vector<string> UtilitySevereIndices::regionLabels() {
    return UtilityGrib::regions();
}

vector<std::pair<string, string>> UtilitySevereIndices::runOptions() {
    return UtilityGrib::synopticRunOptions();
}

vector<string> UtilitySevereIndices::forecastHours(int cycle) {
    return UtilityGrib::forecastHours(cycle);
}

bool UtilitySevereIndices::resolveSynopticRun(const string& runId, string& dateStr, string& cycle) {
    return UtilityGrib::resolveSynopticRun(runId, dateStr, cycle);
}

string UtilitySevereIndices::render(int indexIndex, int regionIndex, const string& forecastHour,
                                     const string& runId, string& status, double& dataMin, double& dataMax,
                                     string& samplePath) {
    samplePath = "";
    if (indexIndex < 0 || indexIndex >= static_cast<int>(indices.size())) {
        status = "unsupported index";
        return "";
    }
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return "";
    }

    string dateStr;
    string cycle;
    if (!resolveSynopticRun(runId, dateStr, cycle)) {
        status = "no RRFS run available";
        return "";
    }
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr3 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 3);
    const auto runKey = dateStr + cycle;
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto regionLabelList = UtilityGrib::regions();
    const auto regionLabel = (regionIndex >= 0 && regionIndex < static_cast<int>(regionLabelList.size()))
        ? regionLabelList[regionIndex] : string{"Unknown"};

    const QDateTime runUtc{
        QDate{To::Int(dateStr.substr(0, 4)), To::Int(dateStr.substr(4, 2)), To::Int(dateStr.substr(6, 2))},
        QTime{To::Int(cycle), 0}, QTimeZone::utc()};
    const auto validLocal = runUtc.addSecs(3600 * forecastHourInt).toLocalTime();
    const auto localZone = QTimeZone::systemTimeZone().abbreviation(validLocal);
    const auto indexKeyUpper = QString::fromStdString(indices[indexIndex].key).toUpper().toStdString();
    status = indexKeyUpper + " " + dateStr.substr(0, 4) + "-" + dateStr.substr(4, 2) + "-" + dateStr.substr(6, 2) +
        " " + cycle + "z    F" + fhr3 + " valid " + validLocal.toString("ddd h:mm AP").toStdString() +
        " " + localZone.toStdString() + "    " + regionLabel;

    const auto dir = QString::fromStdString(cacheDir());
    // "si2" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + QString::fromStdString(
        "/si2_" + runKey + "_" + To::string(indexIndex) + "_" + To::string(regionIndex) + "_" + fhr3 + ".png");
    const RenderLock renderLock{pngPath};   // see objects/RenderLock.h
    const auto gridPath = pngPath + ".grid";
    const auto rangePath = pngPath + ".range";
    if (QFile::exists(pngPath)) {
        QFile rangeFile{rangePath};
        if (rangeFile.open(QIODevice::ReadOnly)) {
            const auto parts = QString::fromUtf8(rangeFile.readAll()).split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                dataMin = parts[0].toDouble();
                dataMax = parts[1].toDouble();
            }
        }
        if (QFile::exists(gridPath)) {
            samplePath = gridPath.toStdString();
        }
        return pngPath.toStdString();
    }

    // both grid computers share the same shape (a raw value tif + a
    // nodata-reference warp, plus SHIP-only contour geometry) - pull the
    // fields the rest of this function needs into common locals rather than
    // forking the whole compositing pipeline below per index.
    QString tifPath;
    QString nodataRefPath;
    QString hailContourBufPath;
    QString hailContourRawPath;
    string gridError;
    bool gridOk = false;
    if (indexIndex == 0) {
        const auto grid = computeShipGrid(box, forecastHour, runId);
        gridOk = grid.ok;
        gridError = grid.error;
        tifPath = grid.shipTifPath;
        nodataRefPath = grid.nodataRefPath;
        hailContourBufPath = grid.hailContourBufPath;
        hailContourRawPath = grid.hailContourRawPath;
        dataMin = grid.dataMin;
        dataMax = grid.dataMax;
    } else {
        const auto grid = computeStpGrid(box, forecastHour, runId);
        gridOk = grid.ok;
        gridError = grid.error;
        tifPath = grid.tifPath;
        nodataRefPath = grid.nodataRefPath;
        dataMin = grid.dataMin;
        dataMax = grid.dataMax;
    }
    if (!gridOk) {
        status = gridError;
        return "";
    }
    {
        QFile rangeFile{rangePath};
        if (rangeFile.open(QIODevice::WriteOnly)) {
            rangeFile.write((QString::number(dataMin, 'f', 2) + " " + QString::number(dataMax, 'f', 2)).toUtf8());
        }
    }

    return finishIndexRender(indexIndex, tifPath, nodataRefPath, hailContourBufPath, hailContourRawPath, box,
                             pngPath, runKey + "_" + To::string(indexIndex) + "_" + To::string(regionIndex) + "_" + fhr3,
                             status, samplePath);
}

// Pixel-wise maximum of an index over a run of forecast hours (e.g. "24hr
// max SHIP" - the worst-case footprint of the threat across the day, the
// natural companion to the SPC day-1 outlook). Each hour's value grid comes
// from the same computeShipGrid / computeStpGrid the single-hour render
// uses (their grib slices are cached, so a repeat is warp + arithmetic
// only); the maximum is one calcRaster over all of them, then the shared
// colorize/PNG step. Hours that fail to compute are skipped and reported in
// the status line. HAILCAST contours are not drawn on a composite.
string UtilitySevereIndices::renderMax(int indexIndex, int regionIndex, const vector<string>& hours,
                                        const string& runId, string& status, double& dataMin, double& dataMax,
                                        string& samplePath) {
    samplePath = "";
    dataMin = 0.0;
    dataMax = 0.0;
    if (indexIndex < 0 || indexIndex >= static_cast<int>(indices.size()) || hours.empty()) {
        status = "unsupported index or empty hour range";
        return "";
    }
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return "";
    }
    string dateStr;
    string cycle;
    if (!resolveSynopticRun(runId, dateStr, cycle)) {
        status = "no RRFS run available";
        return "";
    }
    const auto runKey = dateStr + cycle;
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto regionLabelList = UtilityGrib::regions();
    const auto regionLabel = (regionIndex >= 0 && regionIndex < static_cast<int>(regionLabelList.size()))
        ? regionLabelList[regionIndex] : string{"Unknown"};
    const auto first = To::Int(hours.front());
    const auto last = To::Int(hours.back());
    const auto rangeText = "F" + WString::fixedLengthStringPad0(To::string(first), 3) + "-F" +
        WString::fixedLengthStringPad0(To::string(last), 3);
    const auto indexKeyUpper = QString::fromStdString(indices[indexIndex].key).toUpper().toStdString();
    status = indexKeyUpper + " MAX " + dateStr.substr(0, 4) + "-" + dateStr.substr(4, 2) + "-" + dateStr.substr(6, 2) +
        " " + cycle + "z    " + rangeText + " (" + To::string(static_cast<int>(hours.size())) + " hrs)    " + regionLabel;

    const auto dir = QString::fromStdString(cacheDir());
    // "sm1" is the render version - bump it whenever the drawing pipeline changes
    const auto tagBase = runKey + "_" + To::string(indexIndex) + "_" + To::string(regionIndex) + "_" +
        To::string(first) + "_" + To::string(last) + "_" + To::string(static_cast<int>(hours.size()));
    const auto pngPath = dir + QString::fromStdString("/sm1_" + tagBase + ".png");
    const RenderLock renderLock{pngPath};   // see objects/RenderLock.h
    const auto gridPath = pngPath + ".grid";
    const auto rangePath = pngPath + ".range";
    if (QFile::exists(pngPath)) {
        QFile rangeFile{rangePath};
        if (rangeFile.open(QIODevice::ReadOnly)) {
            const auto parts = QString::fromUtf8(rangeFile.readAll()).split(' ', Qt::SkipEmptyParts);
            if (parts.size() == 2) {
                dataMin = parts[0].toDouble();
                dataMax = parts[1].toDouble();
            }
        }
        if (QFile::exists(gridPath)) {
            samplePath = gridPath.toStdString();
        }
        return pngPath.toStdString();
    }

    QStringList hourGrids;
    int skipped = 0;
    string lastError;
    for (const auto& hour : hours) {
        QString tif;
        QString nodataRef;
        QStringList scratch;
        string error;
        bool ok = false;
        if (indexIndex == 0) {
            const auto grid = computeShipGrid(box, hour, runId);
            ok = grid.ok;
            error = grid.error;
            tif = grid.shipTifPath;
            nodataRef = grid.nodataRefPath;
            scratch << grid.hailContourBufPath << grid.hailContourRawPath;
        } else {
            const auto grid = computeStpGrid(box, hour, runId);
            ok = grid.ok;
            error = grid.error;
            tif = grid.tifPath;
            nodataRef = grid.nodataRefPath;
        }
        for (const auto& file : scratch) {
            QFile::remove(file);
        }
        QFile::remove(nodataRef);
        if (ok && !tif.isEmpty()) {
            // keep this hour's grid under a name unique to this composite
            const auto kept = dir + QString::fromStdString("/smh_" + tagBase + "_" + hour + ".tif");
            QFile::remove(kept);
            if (QFile::rename(tif, kept)) {
                hourGrids << kept;
            }
            QFile::remove(tif + ".aux.xml");
        } else {
            skipped += 1;
            lastError = error;
            QFile::remove(tif);
        }
    }
    if (hourGrids.isEmpty()) {
        status = "no hours could be computed" + (lastError.empty() ? string{} : " (" + lastError + ")");
        return "";
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    const auto maxPath = dir + QString::fromStdString("/smx_" + tagBase + ".tif");
    const auto count = static_cast<int>(hourGrids.size());
    const auto ok = UtilityGrib::calcRaster(bin, hourGrids, [count] (const double * v) {
        double best = v[0];
        for (int i = 1; i < count; i += 1) {
            best = std::max(best, v[i]);
        }
        return best;
    }, maxPath);
    for (const auto& file : hourGrids) {
        QFile::remove(file);
        QFile::remove(file + ".aux.xml");
    }
    if (!ok) {
        status = "max composite calculation failed";
        return "";
    }
    computedMinMax(bin, maxPath, dataMin, dataMax);
    {
        QFile rangeFile{rangePath};
        if (rangeFile.open(QIODevice::WriteOnly)) {
            rangeFile.write((QString::number(dataMin, 'f', 2) + " " + QString::number(dataMax, 'f', 2)).toUtf8());
        }
    }
    if (skipped > 0) {
        status += "    (" + To::string(skipped) + " hr skipped)";
    }
    return finishIndexRender(indexIndex, maxPath, QString{}, QString{}, QString{}, box, pngPath, "max_" + tagBase,
                             status, samplePath);
}

// Colorize a finished index value grid into the final PNG (fill, optional
// HAILCAST contours, hover sidecar). Shared by the single-hour render() and
// the max-over-range composite. Consumes tifPath / nodataRefPath / the hail
// contour scratch files.
string UtilitySevereIndices::finishIndexRender(int indexIndex, const QString& tifPath, const QString& nodataRefPath,
                                               const QString& hailContourBufPath, const QString& hailContourRawPath,
                                               const UtilityGrib::Bbox& box, const QString& pngPath,
                                               const string& tagBaseText, string& status, string& samplePath) {
    const auto bin = QString::fromStdString(UtilityGrib::gdalBinDir()) + "/";
    const auto dir = QString::fromStdString(cacheDir());
    const auto gridPath = pngPath + ".grid";
    const auto tagBase = tagBaseText;
    const auto indexKeyUpper = QString::fromStdString(indices[indexIndex].key).toUpper().toStdString();

    // Nodata pixels are made transparent by the leading "nv 0 0 0 0" entry in
    // the color table below. Neither index's color table has a "should also
    // be transparent" low bin the way reflectivity/probability do - 0 is a
    // real, meaningful value ("definitely not favorable") - so nodata is the
    // only thing that goes transparent. calcRaster propagates -9999 through
    // the formula, so the formula's own output carries the right nodata
    // (the old gdal_calc path did not, which is why a separate raw-input
    // mask used to be needed - found live 2026-09-13).
    auto ok = true;
    const auto colorPath = dir + QString::fromStdString("/sicolor_" + tagBase + ".txt");
    {
        const auto& colorMap = indices[indexIndex].colorMap;
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            // leading "nv" entry: nodata pixels come out fully transparent
            const auto table = "nv 0 0 0 0\n" + colorMap;
            colorFile.write(table.c_str(), static_cast<qint64>(table.size()));
            colorFile.close();
        }
    }
    const auto coloredTif = dir + QString::fromStdString("/rgba_" + tagBase + ".tif");
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-alpha", "-of", "GTiff", tifPath, colorPath, coloredTif});
    QFile::remove(colorPath);
    QFile::remove(tifPath);
    QFile::remove(tifPath + ".aux.xml");
    QFile::remove(nodataRefPath);

    if (!ok) {
        status = "gdal failed compositing " + indexKeyUpper + " fill";
        for (const auto& stale : {hailContourBufPath, hailContourRawPath, coloredTif}) {
            QFile::remove(stale);
        }
        return "";
    }

    if (!hailContourBufPath.isEmpty()) {
        runProcess(bin + "gdal_rasterize",
                {"-q", "-b", "1", "-b", "2", "-b", "3", "-b", "4", "-burn", "255", "-burn", "255", "-burn", "255",
                 "-burn", "255", hailContourBufPath, coloredTif});
        QFile::remove(hailContourBufPath);
    }

    // point-value sidecar for the hover read-out, same adaptive column count
    // as UtilityGrib/UtilitySpcPost's own hover grids
    const auto sampleCols = QString::number(sampleGridColumns(box));
    if (runProcess(bin + "gdal_translate", {"-q", "-of", "XYZ", "-outsize", sampleCols, "0", coloredTif, gridPath})) {
        samplePath = gridPath.toStdString();
    }

    const auto translated = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", coloredTif, pngPath});
    QFile::remove(coloredTif);
    QFile::remove(coloredTif + ".aux.xml");

    if (!translated) {
        status = "gdal_translate failed";
        QFile::remove(hailContourRawPath);
        return "";
    }

    if (!hailContourRawPath.isEmpty()) {
        UtilityGrib::labelContours(pngPath, hailContourRawPath, box, true, 2, "\"");
        QFile::remove(hailContourRawPath);
    }

    return pngPath.toStdString();
}
