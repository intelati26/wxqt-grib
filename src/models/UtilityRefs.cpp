// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityRefs.h"
#include <algorithm>
#include <cctype>
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFont>
#include <QImage>
#include <QPainter>
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

    // Stage 3: paintball rows. Each is one member-field key + an exceedance
    // threshold; the row's `product` is "pb" and render() dispatches to
    // renderPaintball(). idxMatch/units repeat the member field's so the
    // row is self-describing in the combo, but the per-member fetch uses the
    // member rows themselves (found by "<memberKey>_m<N>").
    struct PaintballSpec {
        const char* label;
        const char* key;
        const char* memberKey;
        const char* units;
        double threshold;
    };
    const PaintballSpec paintballSpecs[]{
        {"Paintball Composite Reflectivity >= 20 dBZ", "pb_refc20", "refc", "dBZ", 20.0},
        {"Paintball Composite Reflectivity >= 40 dBZ", "pb_refc40", "refc", "dBZ", 40.0},
        {"Paintball Updraft Helicity 2-5km >= 25", "pb_uphl25", "uphl25", "m2/s2", 25.0},
        {"Paintball Updraft Helicity 2-5km >= 75", "pb_uphl75", "uphl25", "m2/s2", 75.0},
        {"Paintball Surface CAPE >= 1000", "pb_cape1000", "cape", "J/kg", 1000.0},
        {"Paintball Surface CAPE >= 2500", "pb_cape2500", "cape", "J/kg", 2500.0},
        {"Paintball Wind Gust >= 50 kt", "pb_gust50", "gust", "m/s", 25.7},
    };
    // fixed member -> colour key (5 members confirmed; headroom for 8)
    const int memberColors[8][3]{
        {228, 26, 28}, {55, 126, 184}, {77, 175, 74}, {152, 78, 163},
        {255, 127, 0}, {166, 86, 40}, {247, 129, 191}, {90, 90, 90}};

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
        for (const auto& spec : paintballSpecs) {
            result.push_back(UtilityGrib::Field{spec.label, spec.key, spec.units, "", "", "pb"});
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

// Fetches (or reuses the cached) single-record GRIB2 slice for one field/
// hour. ensprod files are one-per-run+cycle+hour (all fields of that product
// type in one file) - cached per run+field+hour, shared across regions, same
// shape as UtilityGrib::render()'s own grib2 cache.
bool UtilityRefs::fetchFieldGrib(const UtilityGrib::Field& field, const string& dateStr, const string& cycle,
                                  int forecastHourInt, QString& gribPathOut, string& status) {
    const auto fhr2 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 2);
    const auto dir = QString::fromStdString(cacheDir());
    const auto runKey = dateStr + cycle;
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
    gribPathOut = gribPath;
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
    if (haveValidCache) {
        return true;
    }
    const auto idx = UtilityIO::getHtml(url + ".idx");
    long long start = -1;
    long long end = -1;
    if (idx.empty() || !UtilityGrib::idxByteRange(idx, field.idxMatch, start, end)) {
        status = field.label + " not available for f" + fhr2;
        return false;
    }
    const auto slice = URL::getBytesRange(url, start, end);
    if (slice.size() < 200 || slice.left(4) != QByteArray{"GRIB"}) {
        status = field.label + " fetch failed";
        return false;
    }
    QFile out{gribPath};
    if (!out.open(QIODevice::WriteOnly)) {
        status = "cache write failed";
        return false;
    }
    out.write(slice);
    out.close();
    return true;
}

// Paintball plot: each available RRFS Ensemble member's area of exceedance
// for the row's threshold, drawn as a translucent blob in that member's own
// colour, all overlaid on one map - where blobs stack the members agree.
// Python-free: per-member colorize is a 3-stop gdaldem table (transparent
// below the threshold, member colour at alpha 110 at/above it), members are
// composited with QPainter, lines burned afterwards, legend drawn last.
string UtilityRefs::renderPaintball(const UtilityGrib::Field& field, int regionIndex, const string& dateStr,
                                     const string& cycle, int forecastHourInt, const string& binDir,
                                     string& status) {
    const PaintballSpec * spec = nullptr;
    for (const auto& candidate : paintballSpecs) {
        if (field.key == candidate.key) {
            spec = &candidate;
        }
    }
    if (spec == nullptr) {
        status = "invalid paintball field";
        return "";
    }
    const auto fhr2 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 2);
    const auto runKey = dateStr + cycle;
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto dir = QString::fromStdString(cacheDir());
    // "pb1" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + QString::fromStdString(
        "/pb1_" + runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2 + ".png");
    if (QFile::exists(pngPath)) {
        return pngPath.toStdString();
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    const auto fillCols = QString::number(UtilityGrib::mainRenderColumns(box));
    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2);
    QImage canvas;
    vector<int> drawn;
    string lastError;
    for (int member = 1; member <= 5; member += 1) {
        const auto memberKey = string{spec->memberKey} + "_m" + To::string(member);
        const UtilityGrib::Field * memberField = nullptr;
        for (const auto& candidate : fields) {
            if (candidate.key == memberKey) {
                memberField = &candidate;
            }
        }
        QString gribPath;
        string memberStatus;
        if (memberField == nullptr || !fetchFieldGrib(*memberField, dateStr, cycle, forecastHourInt, gribPath, memberStatus)) {
            lastError = memberStatus;
            continue;
        }
        const auto suffix = tag + "_m" + QString::number(member);
        const auto colorPath = dir + "/pbcol_" + suffix + ".txt";
        const auto warpPath = dir + "/pbw_" + suffix + ".tif";
        const auto tiffPath = dir + "/pbc_" + suffix + ".tif";
        const auto memberPng = dir + "/pbm_" + suffix + ".png";
        {
            const auto* rgb = memberColors[member - 1];
            const auto colorText = QString{"nv 0 0 0 0\n%1 %2 %3 %4 0\n%5 %2 %3 %4 110\n1000000 %2 %3 %4 110\n"}
                .arg(spec->threshold - 0.001, 0, 'f', 3).arg(rgb[0]).arg(rgb[1]).arg(rgb[2])
                .arg(spec->threshold, 0, 'f', 3).toUtf8();
            QFile colorFile{colorPath};
            if (colorFile.open(QIODevice::WriteOnly)) {
                colorFile.write(colorText);
                colorFile.close();
            }
        }
        const auto ok =
            runProcess(bin + "gdalwarp",
                {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
                 "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
                 "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath}, memberStatus)
            && runProcess(bin + "gdaldem", {"color-relief", "-q", "-alpha", "-of", "GTiff", warpPath, colorPath, tiffPath}, memberStatus)
            && runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", tiffPath, memberPng}, memberStatus);
        QImage layer;
        if (ok && layer.load(memberPng)) {
            layer = layer.convertToFormat(QImage::Format_ARGB32_Premultiplied);
            if (canvas.isNull()) {
                canvas = QImage{layer.size(), QImage::Format_ARGB32_Premultiplied};
                canvas.fill(Qt::transparent);
            }
            if (layer.size() == canvas.size()) {
                QPainter painter{&canvas};
                painter.drawImage(0, 0, layer);
                drawn.push_back(member);
            }
        } else {
            lastError = memberStatus;
        }
        for (const auto& stale : {colorPath, warpPath, tiffPath, memberPng}) {
            QFile::remove(stale);
        }
    }
    if (canvas.isNull() || drawn.empty()) {
        status = "no ensemble members available for f" + fhr2 + (lastError.empty() ? "" : " (" + lastError + ")");
        return "";
    }

    const auto compPng = dir + "/pbcomp_" + tag + ".png";
    const auto compTif = dir + "/pbcomp_" + tag + ".tif";
    canvas.convertToFormat(QImage::Format_ARGB32).save(compPng, "PNG");
    auto ok = runProcess(bin + "gdal_translate",
            {"-q", "-of", "GTiff", "-a_srs", "EPSG:4326", "-a_ullr", fixedQ(box.west), fixedQ(box.north),
             fixedQ(box.east), fixedQ(box.south), compPng, compTif}, status);
    const auto burnLines = [&] (const string& geoJson, int red, int green, int blue) {
        runProcess(bin + "gdal_rasterize",
            {"-q", "-b", "1", "-b", "2", "-b", "3", "-b", "4",
             "-burn", QString::number(red), "-burn", QString::number(green),
             "-burn", QString::number(blue), "-burn", "255",
             QString::fromStdString(geoJson), compTif}, status);
    };
    if (ok) {
        burnLines(UtilityGrib::cwaLinesGeoJson(), 110, 110, 110);
        burnLines(UtilityGrib::stateLinesGeoJson(), 25, 25, 25);
        ok = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", compTif, pngPath}, status);
    }
    for (const auto& stale : {compPng, compTif, compTif + ".aux.xml"}) {
        QFile::remove(stale);
    }
    if (!ok || !QFile::exists(pngPath)) {
        return "";
    }

    // legend key: threshold + one swatch per drawn member, top-left
    QImage finalImage;
    if (finalImage.load(pngPath)) {
        finalImage = finalImage.convertToFormat(QImage::Format_ARGB32);
        QPainter painter{&finalImage};
        painter.setRenderHint(QPainter::Antialiasing);
        const int unit = std::max(11, finalImage.width() / 75);
        QFont font = painter.font();
        font.setPixelSize(unit);
        font.setBold(true);
        painter.setFont(font);
        const auto title = QString::fromStdString(string{"Members >= "} +
            QString::number(spec->threshold, 'f', 0).toStdString() + " " + spec->units);
        const int rowHeight = unit + 6;
        const int boxWidth = std::max(unit * 14, painter.fontMetrics().horizontalAdvance(title) + unit);
        const int boxHeight = rowHeight * (static_cast<int>(drawn.size()) + 1) + 8;
        painter.fillRect(QRect{6, 6, boxWidth, boxHeight}, QColor{255, 255, 255, 215});
        painter.setPen(QColor{30, 30, 30});
        painter.drawText(QPoint{6 + unit / 2, 6 + rowHeight}, title);
        int row = 1;
        for (const auto member : drawn) {
            const auto* rgb = memberColors[member - 1];
            const int top = 6 + 4 + rowHeight * row + 3;
            painter.fillRect(QRect{6 + unit / 2, top, unit * 2, unit}, QColor{rgb[0], rgb[1], rgb[2]});
            painter.drawRect(QRect{6 + unit / 2, top, unit * 2, unit});
            painter.drawText(QPoint{6 + unit / 2 + unit * 2 + 6, top + unit - 1}, QString{"Member %1"}.arg(member));
            row += 1;
        }
        painter.end();
        finalImage.save(pngPath, "PNG");
    }
    return pngPath.toStdString();
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

    if (field.product == "pb") {
        return renderPaintball(field, regionIndex, dateStr, cycle, forecastHourInt, binDir, status);
    }

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

    QString gribPath;
    if (!fetchFieldGrib(field, dateStr, cycle, forecastHourInt, gribPath, status)) {
        return "";
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
