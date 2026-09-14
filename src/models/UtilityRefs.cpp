// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityRefs.h"
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

// Stage 0: two fields chosen to prove both the plain case (temp, no
// special masking) and the reflectivity-specific low-end transparency mask
// this pipeline also needs to get right before Stage 2/3 reuse the same
// masking for the per-member paintball fill. Both reuse UtilityGrib's own
// color tables (now public) rather than a second copy - same physical
// quantities, same visual scale should look the same across viewers.
// Reflectivity is NOT in the plain "mean" file (verified live 2026-09-14 -
// REFS doesn't produce a simple ensemble-mean reflectivity, presumably
// because averaging dBZ directly isn't meteorologically meaningful the way
// it is for temperature); it IS in "pmmn" (probability-matched mean,
// REFS's own more appropriate answer to "one representative reflectivity
// field"), tagged "wt ens mean" in that file's own .idx.
const vector<UtilityGrib::Field> UtilityRefs::fields{
    UtilityGrib::Field{"Ensemble Mean 2m Temperature", "tmp2m_mean", "C", ":TMP:2 m above ground:",
                        UtilityGrib::tempColorMap, "mean"},
    UtilityGrib::Field{"Probability-Matched Mean Composite Reflectivity", "refc_pmmn", "dBZ",
                        ":REFC:entire atmosphere", UtilityGrib::reflColorMap, "pmmn"},
};

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

    // "rf1" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + QString::fromStdString(
        "/rf1_" + runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2 + ".png");
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
    const auto url = "https://nomads.ncep.noaa.gov/pub/data/nccf/com/refs/" + UtilityGrib::dataStream() +
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
    const auto maskPath = dir + "/m_" + tag + ".tif";
    const auto echoMaskPath = dir + "/em_" + tag + ".tif";
    const auto rgbPath = dir + "/rgb_" + tag + ".tif";
    const auto tiffPath = dir + "/c_" + tag + ".tif";
    {
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(field.colorMap.c_str(), static_cast<qint64>(field.colorMap.size()));
            colorFile.close();
        }
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    const auto gdalCalc = bin + (QFile::exists(bin + "gdal_calc") ? "gdal_calc" : "gdal_calc.py");
    const auto fillCols = QString::number(UtilityGrib::mainRenderColumns(box));

    auto ok = runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
             "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath}, status);
    // gdaldem's "-alpha" does NOT make recognised-nodata pixels transparent
    // (clamped-opaque to the nearest colour stop instead) - build an
    // explicit mask from the warp's own nodata, same fix UtilityGrib::
    // render()/renderBackground() already use for the identical problem.
    const auto gdalMerge = bin + (QFile::exists(bin + "gdal_merge") ? "gdal_merge" : "gdal_merge.py");
    ok = ok && runProcess(gdalCalc, {"-A", warpPath, "--calc=255*(A!=-9999)", "--outfile=" + maskPath,
                                      "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"}, status);
    // reflectivity's colour map is deliberately transparent below ~5 dBZ
    // (real "no echo", not nodata) - AND in a value-based mask too, same
    // reasoning/fix as UtilityGrib::render().
    if (ok && field.key == "refc_pmmn") {
        ok = runProcess(gdalCalc, {"-A", warpPath, "--calc=255*(A>=5)", "--outfile=" + echoMaskPath,
                                    "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"}, status);
        if (ok) {
            const auto combinedPath = dir + "/cm_" + tag + ".tif";
            if (runProcess(gdalCalc, {"-A", maskPath, "-B", echoMaskPath, "--calc=minimum(A,B)",
                                       "--outfile=" + combinedPath, "--overwrite", "--quiet",
                                       "--type=Byte", "--NoDataValue=0"}, status)) {
                QFile::remove(maskPath);
                QFile::rename(combinedPath, maskPath);
            }
        }
        QFile::remove(echoMaskPath);
    }
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-of", "GTiff", warpPath, colorPath, rgbPath}, status);
    ok = ok && runProcess(gdalMerge, {"-q", "-o", tiffPath, "-separate", "-co", "PHOTOMETRIC=RGB", rgbPath, maskPath}, status);
    for (const auto& stale : {maskPath, rgbPath}) {
        QFile::remove(stale);
    }
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
