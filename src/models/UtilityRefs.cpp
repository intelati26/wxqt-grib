// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityRefs.h"
#include <cctype>
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStringList>
#include <QTime>
#include <QTimeZone>
#include "objects/URL.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"

namespace {
    QString fixedQ(double value) {
        return QString::number(value, 'f', 3);
    }

    // Spread fields answer "how much do members disagree here," a
    // fundamentally different question from "what's the value" - reusing
    // UtilityGrib's raw-value colormaps (tempColorMap/capeColorMap/
    // reflColorMap) would make a high-disagreement area look identical to
    // a genuinely hot/unstable/stormy one. One pale-to-intense single-hue
    // sequential scale per variable instead, calibrated to that variable's
    // typical spread range (much narrower than its own value range).
    const string tempSpreadColorMap{
        "0 245 245 245\n" "0.5 210 225 245\n" "1 150 190 235\n"
        "2 90 150 220\n" "3 230 200 60\n" "4.5 230 90 40\n" "6 150 20 20\n"};

    const string capeSpreadColorMap{
        "0 245 245 245\n" "100 210 225 245\n" "300 150 190 235\n"
        "600 90 150 220\n" "900 230 200 60\n" "1200 230 90 40\n" "1800 150 20 20\n"};

    const string reflSpreadColorMap{
        "0 245 245 245\n" "2 245 245 245\n" "5 150 190 235\n"
        "10 90 150 220\n" "15 230 200 60\n" "20 230 90 40\n" "28 150 20 20\n"};

    bool runProcess(const QString& program, const QStringList& args, string& status) {
        QProcess process;
        process.start(program, args);
        process.waitForFinished(30000);
        const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!ok) {
            status = "gdal failed: " + process.readAllStandardError().left(200).toStdString();
        }
        return ok;
    }
}

// Stage 0 proved the pipeline with two fields (plain case + the
// reflectivity-specific low-end transparency mask). Stage 1 rounds out the
// other product types - verified live 2026-09-14 which fields each
// ensprod product type actually carries, rather than assuming they're
// uniform:
// - "mean": broad general-purpose fields (CAPE/TMP/WIND/HGT/etc) - NO REFC
//   (averaging dBZ directly isn't meteorologically meaningful the way it
//   is for temperature).
// - "sprd" (ensemble spread - how much members disagree, not a value):
//   the same broad set as "mean", PLUS REFC (spread of reflectivity IS
//   produced, even though a plain mean isn't).
// - "pmmn" (probability-matched mean): REFC ONLY - REFS's own answer to
//   "one representative reflectivity field" in place of a plain mean.
// - "lpmm"/"avrg": precipitation-only (multiple accumulation windows) -
//   not yet added here, needs a threshold/window-disambiguation approach
//   closer to what Stage 4's probability picker will need anyway; see
//   docs/refs-viewer-plan.md.
// All reuse UtilityGrib's own color tables (now public) for value fields -
// same physical quantity, same visual scale should look the same across
// viewers. Spread fields get their own dedicated colormaps (see above) -
// reusing a value colormap for "how much disagreement" would make a
// high-spread area look like a genuinely hot/unstable/stormy one.
namespace {
    // Stage 2: the five raw RRFS Ensemble members (rrfsens.*/m001-m005), a
    // reduced-but-rich per-member field set out of the 2dfldnomads file
    // (verified live 2026-09-30, all ENS=+N tagged). Same value colormaps as
    // the deterministic viewer. A member row's `product` slot holds the
    // member directory name ("m001".."m005") instead of an ensprod type -
    // isMemberProduct() tells the two apart wherever the URL is built.
    struct MemberFieldSpec {
        const char* label;
        const char* key;
        const char* units;
        const char* idxMatch;
        const string& colorMap;
    };

    vector<UtilityGrib::Field> buildFields() {
        vector<UtilityGrib::Field> result{
        UtilityGrib::Field{"Ensemble Mean 2m Temperature", "tmp2m_mean", "C", ":TMP:2 m above ground:",
                            UtilityGrib::tempColorMap, "mean"},
        UtilityGrib::Field{"Ensemble Mean Surface CAPE", "cape_mean", "J/kg", ":CAPE:surface:",
                            UtilityGrib::capeColorMap, "mean"},
        UtilityGrib::Field{"Ensemble Mean 10m Wind Speed", "wind10m_mean", "m/s", ":WIND:10 m above ground:",
                            UtilityGrib::windColorMap, "mean"},
        UtilityGrib::Field{"Ensemble Spread 2m Temperature", "tmp2m_sprd", "C", ":TMP:2 m above ground:",
                            tempSpreadColorMap, "sprd"},
        UtilityGrib::Field{"Ensemble Spread Surface CAPE", "cape_sprd", "J/kg", ":CAPE:surface:",
                            capeSpreadColorMap, "sprd"},
        UtilityGrib::Field{"Ensemble Spread Composite Reflectivity", "refc_sprd", "dBZ",
                            ":REFC:entire atmosphere", reflSpreadColorMap, "sprd"},
        UtilityGrib::Field{"Probability-Matched Mean Composite Reflectivity", "refc_pmmn", "dBZ",
                            ":REFC:entire atmosphere", UtilityGrib::reflColorMap, "pmmn"},
        };
        const MemberFieldSpec memberSpecs[]{
            {"Composite Reflectivity", "refc", "dBZ", ":REFC:entire atmosphere", UtilityGrib::reflColorMap},
            {"2m Temperature", "tmp2m", "C", ":TMP:2 m above ground:", UtilityGrib::tempColorMap},
            {"Surface CAPE", "cape", "J/kg", ":CAPE:surface:", UtilityGrib::capeColorMap},
            {"Max 10m Wind Speed", "wind10m", "m/s", ":WIND:10 m above ground:", UtilityGrib::windColorMap},
            {"Surface Wind Gust", "gust", "m/s", ":GUST:surface:", UtilityGrib::windColorMap},
            {"Updraft Helicity 2-5km", "uphl25", "m2/s2", ":MXUPHL:5000-2000 m above ground:",
                UtilityGrib::uphlColorMap},
        };
        for (int member = 1; member <= 5; member += 1) {
            for (const auto& spec : memberSpecs) {
                result.push_back(UtilityGrib::Field{
                    "RRFS Ens Member " + To::string(member) + " " + spec.label,
                    string{spec.key} + "_m" + To::string(member), spec.units, spec.idxMatch, spec.colorMap,
                    "m00" + To::string(member)});
            }
        }
        return result;
    }

    bool isMemberProduct(const string& product) {
        return product.size() == 4 && product[0] == 'm' && std::isdigit(static_cast<unsigned char>(product[1]));
    }
}

const vector<UtilityGrib::Field> UtilityRefs::fields{buildFields()};

vector<string> UtilityRefs::fieldLabels() {
    vector<string> labels;
    for (const auto& field : fields) {
        labels.push_back(field.label);
    }
    return labels;
}

vector<string> UtilityRefs::regions() {
    return UtilityGrib::regions();
}

vector<std::pair<string, string>> UtilityRefs::runOptions() {
    return UtilityGrib::synopticRunOptions();
}

vector<string> UtilityRefs::forecastHours() {
    vector<string> hours;
    for (int hour = 1; hour <= 60; hour += 1) {
        hours.push_back(WString::fixedLengthStringPad0(To::string(hour), 2));
    }
    return hours;
}

string UtilityRefs::cacheDir() {
    auto path = QDir::tempPath() + "/wxqt_refs";
    QDir{}.mkpath(path);
    return path.toStdString();
}

string UtilityRefs::render(int fieldIndex, int regionIndex, const string& forecastHour, const string& runId,
                            string& status, double& dataMin, double& dataMax, string& samplePath) {
    dataMin = 0.0;
    dataMax = 0.0;
    samplePath = "";
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        status = "invalid field";
        return "";
    }
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return "";
    }

    string dateStr;
    string cycle;
    if (!UtilityGrib::resolveSynopticRun(runId, dateStr, cycle)) {
        status = "no REFS run available";
        return "";
    }
    const auto forecastHourInt = To::Int(forecastHour);
    const auto fhr2 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 2);
    const auto runKey = dateStr + cycle;
    const auto& field = fields[fieldIndex];
    const auto regionLabelList = UtilityGrib::regions();
    const auto regionLabel = (regionIndex >= 0 && regionIndex < static_cast<int>(regionLabelList.size()))
        ? regionLabelList[regionIndex] : string{"Unknown"};
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto dir = QString::fromStdString(cacheDir());

    const QDateTime runUtc{
        QDate{To::Int(dateStr.substr(0, 4)), To::Int(dateStr.substr(4, 2)), To::Int(dateStr.substr(6, 2))},
        QTime{To::Int(cycle), 0}, QTimeZone::utc()};
    const auto validLocal = runUtc.addSecs(3600 * forecastHourInt).toLocalTime();
    const auto localZone = QTimeZone::systemTimeZone().abbreviation(validLocal);
    status = "REFS " + dateStr.substr(0, 4) + "-" + dateStr.substr(4, 2) + "-" + dateStr.substr(6, 2) +
        " " + cycle + "z    F" + fhr2 + " valid " + validLocal.toString("ddd h:mm AP").toStdString() +
        " " + localZone.toStdString() + "    " + field.label + "    " + regionLabel;

    // "rf2" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + QString::fromStdString(
        "/rf2_" + runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2 + ".png");
    const auto rangePath = pngPath + ".range";
    const auto gridPath = pngPath + ".grid";
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

    // ensprod files are one-per-run+cycle+hour (all fields of that product
    // type in one file) - cached per run+field+hour, shared across regions,
    // same shape as UtilityGrib::render()'s own grib2 cache.
    // Member rows come from the sibling rrfsens.* tree (com/rrfs/, 3-digit
    // forecast hour); ensprod rows from com/refs/refs.* (2-digit hour).
    const auto url = isMemberProduct(field.product)
        ? "https://nomads.ncep.noaa.gov/pub/data/nccf/com/rrfs/" + UtilityGrib::dataStream() +
            "/rrfsens." + dateStr + "/" + cycle + "/" + field.product + "/rrfs.t" + cycle + "z." +
            field.product + ".2dfldnomads.3km.f" + WString::fixedLengthStringPad0(To::string(forecastHourInt), 3) +
            ".conus.grib2"
        : "https://nomads.ncep.noaa.gov/pub/data/nccf/com/refs/" + UtilityGrib::dataStream() +
            "/refs." + dateStr + "/" + cycle + "/ensprod/refs.t" + cycle + "z." + field.product + ".f" + fhr2 +
            ".conus.grib2";
    const auto gribPath = dir + QString::fromStdString("/g_" + runKey + "_" + field.key + "_" + fhr2 + ".grib2");
    {
        bool haveValidCache = false;
        {
            QFile check{gribPath};
            if (check.size() > 200 && check.open(QIODevice::ReadOnly)) {
                const auto validHeader = check.read(4) == QByteArray{"GRIB"};
                const auto validTrailer = check.seek(check.size() - 4) && check.read(4) == QByteArray{"7777"};
                check.close();
                haveValidCache = validHeader && validTrailer;
            }
            if (!haveValidCache) {
                QFile::remove(gribPath);
            }
        }
        if (!haveValidCache) {
            const auto idx = UtilityIO::getHtml(url + ".idx");
            long long start = -1;
            long long end = -1;
            if (idx.empty() || !UtilityGrib::idxByteRange(idx, field.idxMatch, start, end)) {
                status = field.label + " not available for f" + fhr2;
                return "";
            }
            const auto slice = URL::getBytesRange(url, start, end);
            if (slice.size() < 200 || slice.left(4) != QByteArray{"GRIB"}) {
                status = field.label + " fetch failed";
                return "";
            }
            QFile out{gribPath};
            if (!out.open(QIODevice::WriteOnly)) {
                status = "cache write failed";
                return "";
            }
            out.write(slice);
            out.close();
        }
    }

    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2);
    const auto colorPath = dir + "/col_" + tag + ".txt";
    const auto warpPath = dir + "/w_" + tag + ".tif";
    const auto tiffPath = dir + "/c_" + tag + ".tif";
    {
        // leading "nv" entry = colour for recognised-nodata pixels (transparent)
        const auto table = "nv 0 0 0 0\n" + field.colorMap;
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(table.c_str(), static_cast<qint64>(table.size()));
            colorFile.close();
        }
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    const auto fillCols = QString::number(UtilityGrib::mainRenderColumns(box));

    auto ok = runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
             "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath}, status);
    // Colorize straight to RGBA - the "nv 0 0 0 0" entry written into the
    // colour table above makes recognised-nodata pixels transparent, and
    // reflectivity's own alpha-0 stops below 5 dBZ are honored by -alpha,
    // so no gdal_calc mask / gdal_merge is needed (both are Python scripts
    // the portable builds don't bundle). Same reasoning and verification as
    // UtilityGrib::render().
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-alpha", "-of", "GTiff", warpPath, colorPath, tiffPath}, status);
    if (!ok) {
        for (const auto& stale : {colorPath, warpPath, tiffPath}) {
            QFile::remove(stale);
        }
        return "";
    }

    QByteArray info;
    {
        QProcess process;
        process.start(bin + "gdalinfo", {"-mm", warpPath});
        process.waitForFinished(30000);
        info = process.readAllStandardOutput();
    }
    {
        const auto text = QString::fromUtf8(info);
        const auto match = QRegularExpression{R"(Computed Min/Max=(-?[0-9.]+),(-?[0-9.]+))"}.match(text);
        if (match.hasMatch()) {
            dataMin = match.captured(1).toDouble();
            dataMax = match.captured(2).toDouble();
            QFile rangeFile{rangePath};
            if (rangeFile.open(QIODevice::WriteOnly)) {
                rangeFile.write((QString::number(dataMin, 'f', 2) + " " + QString::number(dataMax, 'f', 2)).toUtf8());
            }
        }
    }

    // point-value sidecar for the hover read-out, same adaptive column
    // count UtilityGrib/UtilitySevereIndices' own hover grids use
    {
        const auto savedStatus = status;
        const auto sampleCols = QString::number(UtilityGrib::mainRenderColumns(box) > 1000 ? 220 : 400);
        if (runProcess(bin + "gdal_translate", {"-q", "-of", "XYZ", "-outsize", sampleCols, "0", warpPath, gridPath}, status)) {
            samplePath = gridPath.toStdString();
        }
        status = savedStatus;
    }

    const auto burnLines = [&] (const string& geoJson, int red, int green, int blue) {
        runProcess(bin + "gdal_rasterize",
            {"-q", "-b", "1", "-b", "2", "-b", "3", "-b", "4",
             "-burn", QString::number(red), "-burn", QString::number(green),
             "-burn", QString::number(blue), "-burn", "255",
             QString::fromStdString(geoJson), tiffPath}, status);
    };
    burnLines(UtilityGrib::cwaLinesGeoJson(), 110, 110, 110);
    burnLines(UtilityGrib::stateLinesGeoJson(), 25, 25, 25);

    const auto translated = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", tiffPath, pngPath}, status);
    for (const auto& stale : {colorPath, warpPath, tiffPath}) {
        QFile::remove(stale);
    }
    if (!translated || !QFile::exists(pngPath)) {
        return "";
    }
    return pngPath.toStdString();
}
