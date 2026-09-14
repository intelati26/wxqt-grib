// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilitySpcPost.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <set>
#include <thread>
#include <QByteArray>
#include <QRandomGenerator>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTimeZone>
#include "common/GlobalVariables.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityLog.h"

namespace {
    const string baseUrl{"https://nomads.ncep.noaa.gov/pub/data/nccf/com/spc_post/prod"};

    // matches an Apache directory-listing anchor, e.g. href="spc_post.20260910/"
    const QRegularExpression hrefRe{R"RX(href="([^"?/][^"]*)")RX"};

    // turbo-ish 10-100% probability palette (gdaldem color-relief format)
    const string probColorMap{
        "0 0 0 0 0\n"
        "9 0 0 0 0\n"
        "10 61 38 168\n"
        "20 43 100 224\n"
        "30 30 156 220\n"
        "40 33 195 150\n"
        "50 100 210 80\n"
        "60 180 220 50\n"
        "70 250 210 40\n"
        "80 250 140 30\n"
        "90 230 60 30\n"
        "100 170 10 10\n"};

    struct ListDirCacheEntry {
        vector<string> entries;
        qint64 atMs{0};
    };
    std::map<string, ListDirCacheEntry> listDirCache;
    constexpr qint64 listDirCacheMs = 120000;   // 2 minutes

    string buildPattern(const string& fieldRegex) {
        return R"(spc_post\.t(\d{2})z\.)" + fieldRegex + R"(\.f(\d{3})\.grib2$)";
    }

    string fixed(double value, int decimals) {
        return QString::number(value, 'f', decimals).toStdString();
    }

    QString fixedQ(double value) {
        return QString::number(value, 'f', 3);
    }

    // hover sample-grid column count, scaled down for wide domains so total
    // cell count (render + parse time) doesn't grow with the square of a
    // domain's linear size - same tiering as UtilityGrib's sampleGridColumns()
    int sampleGridColumns(const UtilitySpcPost::Domain& domain) {
        const auto span = std::max(domain.east - domain.west, domain.north - domain.south);
        if (span > 30.0) {
            return 220;
        }
        if (span > 15.0) {
            return 350;
        }
        return 450;
    }

    // main visual render resolution - shares UtilityGrib::mainRenderColumns'
    // implementation rather than keeping a second copy of the geometry math
    int mainRenderColumns(const UtilitySpcPost::Domain& domain) {
        return UtilityGrib::mainRenderColumns({domain.west, domain.south, domain.east, domain.north});
    }
}

const vector<UtilitySpcPost::Product> UtilitySpcPost::products{
    {"thunder_1hr",  "thunder",    R"(hrefct_1hr)",  "HREFCT Thunderstorm Probability (1-hr)"},
    {"thunder_4hr",  "thunder",    R"(hrefct_4hr)",  "HREFCT Thunderstorm Probability (4-hr)"},
    {"thunder_full", "thunder",    R"(hrefct_full)", "HREFCT Thunderstorm Probability (Full Period)"},
    {"ltgdensity_4hr", "ltgdensity", R"(hrefld_4hr)", "Calibrated Lightning Density (4-hr)"},
    {"severe_tor_4hr",   "severe", R"(href_cal_gefs_tor_(\d{2})\.4hr)",   "HREF/GEFS Calibrated Tor Probability (4-hr)"},
    {"severe_tor_24hr",  "severe", R"(href_cal_gefs_tor_(\d{2})\.24hr)",  "HREF/GEFS Calibrated Tor Probability (24-hr)"},
    {"severe_wind_4hr",  "severe", R"(href_cal_gefs_wind_(\d{2})\.4hr)",  "HREF/GEFS Calibrated Wind Probability (4-hr)"},
    {"severe_wind_24hr", "severe", R"(href_cal_gefs_wind_(\d{2})\.24hr)", "HREF/GEFS Calibrated Wind Probability (24-hr)"},
    {"severe_hail_4hr",  "severe", R"(href_cal_gefs_hail_(\d{2})\.4hr)",  "HREF/GEFS Calibrated Hail Probability (4-hr)"},
    {"severe_hail_24hr", "severe", R"(href_cal_gefs_hail_(\d{2})\.24hr)", "HREF/GEFS Calibrated Hail Probability (24-hr)"},
    {"gefsmlp_tor_std",     "gefs_mlp", R"(gefsmlp_tor_std)",     "GEFS-MLP Tor Probability (Any Severe)"},
    {"gefsmlp_tor_sig",     "gefs_mlp", R"(gefsmlp_tor_sig)",     "GEFS-MLP Tor Probability (Significant)"},
    {"gefsmlp_wind_std",    "gefs_mlp", R"(gefsmlp_wind_std)",    "GEFS-MLP Wind Probability (Any Severe)"},
    {"gefsmlp_wind_sig",    "gefs_mlp", R"(gefsmlp_wind_sig)",    "GEFS-MLP Wind Probability (Significant)"},
    {"gefsmlp_hail_std",    "gefs_mlp", R"(gefsmlp_hail_std)",    "GEFS-MLP Hail Probability (Any Severe)"},
    {"gefsmlp_hail_sig",    "gefs_mlp", R"(gefsmlp_hail_sig)",    "GEFS-MLP Hail Probability (Significant)"},
    {"gefsmlp_severe_std",  "gefs_mlp", R"(gefsmlp_severe_std)",  "GEFS-MLP Severe Probability (Any Severe)"},
    {"gefsmlp_severe_sig",  "gefs_mlp", R"(gefsmlp_severe_sig)",  "GEFS-MLP Severe Probability (Significant)"},
};

// CONUS + the same SPC mesoscale-analysis sectors GribViewer's regionTable uses
// (matching bboxes/labels for consistency across the app)
const vector<UtilitySpcPost::Domain> UtilitySpcPost::domains{
    {"CONUS",              -125.0, 24.0,  -66.0,  50.0},
    {"Midwest",              -98.1, 34.4,  -84.9,  43.6},
    {"North Central",       -103.1, 40.0,  -89.9,  49.3},
    {"Central",             -102.9, 33.0,  -89.7,  42.2},
    {"South Central",       -103.3, 27.2,  -90.1,  36.4},
    {"Northeast",             -83.7, 39.1,  -70.5,  48.3},
    {"Central East",         -88.5, 32.3,  -75.3,  41.5},
    {"Southeast",            -92.2, 26.4,  -79.0,  35.6},
    {"Southwest",           -119.1, 31.2, -105.9,  40.4},
    {"Northwest",           -119.3, 40.0, -106.1,  49.2},
    {"Great Lakes",          -92.5, 39.4,  -79.3,  48.6},
    {"Intermountain West",  -117.2, 36.4, -104.0,  45.6},
};

vector<string> UtilitySpcPost::productLabels() {
    vector<string> labels;
    for (const auto& product : products) {
        labels.push_back(product.label);
    }
    return labels;
}

vector<string> UtilitySpcPost::domainLabels() {
    vector<string> labels;
    for (const auto& domain : domains) {
        labels.push_back(domain.label);
    }
    return labels;
}

vector<std::pair<string, int>> UtilitySpcPost::boundaryOptions() {
    return {
        {"State Lines", BoundaryState},
        {"NWS CWAs", BoundaryCwa},
        {"County Lines", BoundaryCounty},
        {"Highways", BoundaryHighway},
        {"Lakes", BoundaryLake},
        {"Canada", BoundaryCanada},
        {"Mexico", BoundaryMexico},
        {"Cities", BoundaryCity},
    };
}

vector<string> UtilitySpcPost::backgroundLabels() {
    return {"None", "RRFS 2m Temperature", "RRFS Reflectivity"};
}

string UtilitySpcPost::gdalBinDir() {
    const auto found = QStandardPaths::findExecutable("gdalwarp");
    if (found.isEmpty()) {
        return "";
    }
    return QFileInfo{found}.absolutePath().toStdString();
}

bool UtilitySpcPost::gdalAvailable() {
    return !gdalBinDir().empty();
}

string UtilitySpcPost::probabilityColorMap() {
    return probColorMap;
}

string UtilitySpcPost::cacheDir() {
    auto path = QDir::tempPath() + "/wxqt_spcpost";
    QDir{}.mkpath(path);
    return path.toStdString();
}

vector<string> UtilitySpcPost::listDir(const string& url) {
    const auto nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto cached = listDirCache.find(url);
    if (cached != listDirCache.end() && nowMs - cached->second.atMs < listDirCacheMs) {
        return cached->second.entries;
    }
    const auto html = UtilityIO::getHtml(url);
    vector<string> entries;
    auto it = hrefRe.globalMatch(QString::fromStdString(html));
    while (it.hasNext()) {
        const auto name = it.next().captured(1).toStdString();
        if (name != "../" && name != "/") {
            entries.push_back(name);
        }
    }
    listDirCache[url] = {entries, nowMs};
    return entries;
}

vector<string> UtilitySpcPost::listDateDirs() {
    auto entries = listDir(baseUrl + "/");
    vector<string> dateDirs;
    for (const auto& entry : entries) {
        if (WString::startsWith(entry, "spc_post.")) {
            dateDirs.push_back(entry);
        }
    }
    std::sort(dateDirs.begin(), dateDirs.end(), std::greater<string>{});
    return dateDirs;
}

bool UtilitySpcPost::resolveRun(int productIndex, const string& runId, RunFiles& out) {
    if (productIndex < 0 || productIndex >= static_cast<int>(products.size())) {
        return false;
    }
    static std::map<string, RunFiles> cache;
    static std::map<string, qint64> cacheAtMs;
    const auto cacheKey = To::string(productIndex) + "_" + (runId.empty() ? string{"latest"} : runId);
    const auto nowMs = QDateTime::currentMSecsSinceEpoch();
    const auto cached = cacheAtMs.find(cacheKey);
    if (cached != cacheAtMs.end() && nowMs - cached->second < 600000) {
        out = cache[cacheKey];
        return true;
    }

    const auto& product = products[productIndex];
    const QRegularExpression re{QString::fromStdString(buildPattern(product.fieldRegex))};

    string dateStr;
    string cycle;
    if (runId.size() == 10) {
        dateStr = runId.substr(0, 8);
        cycle = runId.substr(8, 2);
    } else {
        bool found = false;
        for (const auto& dateDirRaw : listDateDirs()) {
            const auto candidateDate = dateDirRaw.substr(9, 8);   // "spc_post." is 9 chars
            const auto files = listDir(baseUrl + "/" + dateDirRaw + product.subdir + "/");
            std::set<string> cycles;
            for (const auto& file : files) {
                const auto match = re.match(QString::fromStdString(file));
                if (match.hasMatch()) {
                    cycles.insert(match.captured(1).toStdString());
                }
            }
            if (!cycles.empty()) {
                dateStr = candidateDate;
                cycle = *cycles.rbegin();   // largest / latest cycle in this day's listing
                found = true;
                break;
            }
        }
        if (!found) {
            UtilityLog::d("UtilitySpcPost: no recent run found for " + product.key);
            return false;
        }
    }

    const auto files = listDir(baseUrl + "/spc_post." + dateStr + "/" + product.subdir + "/");
    std::map<int, std::pair<string, string>> byHour;   // fhr -> (filename, env or "")
    for (const auto& file : files) {
        const auto match = re.match(QString::fromStdString(file));
        if (!match.hasMatch() || match.captured(1).toStdString() != cycle) {
            continue;
        }
        const auto groupCount = match.lastCapturedIndex();
        const auto fhr = match.captured(groupCount).toInt();
        const auto env = groupCount >= 3 ? match.captured(2).toStdString() : string{};
        const auto existing = byHour.find(fhr);
        if (existing == byHour.end()) {
            byHour[fhr] = {file, env};
            continue;
        }
        auto& [bestFile, bestEnv] = existing->second;
        if (!env.empty() && !bestEnv.empty()) {
            if (env == cycle && bestEnv != cycle) {
                bestFile = file;
                bestEnv = env;
            } else if (bestEnv != cycle && env > bestEnv) {
                bestFile = file;
                bestEnv = env;
            }
        }
    }
    if (byHour.empty()) {
        UtilityLog::d("UtilitySpcPost: no files matched " + product.key + " cycle " + cycle +
            " under spc_post." + dateStr + "/" + product.subdir);
        return false;
    }

    out.dateStr = dateStr;
    out.cycle = cycle;
    out.hours.clear();
    out.filenames.clear();
    for (const auto& [fhr, fileEnv] : byHour) {
        const auto fhr3 = WString::fixedLengthStringPad0(To::string(fhr), 3);
        out.hours.push_back(fhr3);
        out.filenames[fhr3] = fileEnv.first;
    }

    cache[cacheKey] = out;
    cacheAtMs[cacheKey] = nowMs;
    return true;
}

vector<std::pair<string, string>> UtilitySpcPost::runOptions(int productIndex) {
    vector<std::pair<string, string>> options{{"Latest", ""}};
    if (productIndex < 0 || productIndex >= static_cast<int>(products.size())) {
        return options;
    }
    const auto& product = products[productIndex];
    const QRegularExpression re{QString::fromStdString(buildPattern(product.fieldRegex))};

    constexpr int maxDaysChecked = 6;
    constexpr size_t maxOptions = 16;
    int daysChecked = 0;
    for (const auto& dateDirRaw : listDateDirs()) {
        if (daysChecked >= maxDaysChecked || options.size() > maxOptions) {
            break;
        }
        daysChecked += 1;
        const auto dateStr = dateDirRaw.substr(9, 8);
        const auto files = listDir(baseUrl + "/" + dateDirRaw + product.subdir + "/");
        if (files.empty()) {
            continue;
        }
        std::set<string, std::greater<string>> cycles;
        for (const auto& file : files) {
            const auto match = re.match(QString::fromStdString(file));
            if (match.hasMatch()) {
                cycles.insert(match.captured(1).toStdString());
            }
        }
        const auto qDate = QDate::fromString(QString::fromStdString(dateStr), "yyyyMMdd");
        for (const auto& cyc : cycles) {
            if (options.size() > maxOptions) {
                break;
            }
            const auto label = qDate.toString("MMM d").toStdString() + " " + cyc + "z";
            options.emplace_back(label, dateStr + cyc);
        }
    }
    return options;
}

vector<string> UtilitySpcPost::forecastHours(int productIndex, const string& runId) {
    RunFiles files;
    if (!resolveRun(productIndex, runId, files)) {
        return {};
    }
    return files.hours;
}

string UtilitySpcPost::render(int productIndex, int domainIndex, int boundaryFlags, int backgroundIndex,
                              const string& forecastHour, const string& runId, string& status,
                              string& samplePath) {
    samplePath = "";
    if (productIndex < 0 || productIndex >= static_cast<int>(products.size())) {
        status = "invalid product";
        return "";
    }
    const auto binDir = gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        UtilityLog::d("UtilitySpcPost: gdalwarp not found on PATH");
        return "";
    }
    if (domainIndex < 0 || domainIndex >= static_cast<int>(domains.size())) {
        domainIndex = 0;
    }
    const auto& product = products[productIndex];
    const auto& domain = domains[domainIndex];

    RunFiles run;
    if (!resolveRun(productIndex, runId, run)) {
        status = "no " + product.label + " run available on NOMADS";
        UtilityLog::d("UtilitySpcPost: resolveRun failed for " + product.key + " runId=" + (runId.empty() ? string{"latest"} : runId));
        return "";
    }
    const auto fhr3 = forecastHour.size() == 3 ? forecastHour : WString::fixedLengthStringPad0(forecastHour, 3);
    const auto fileEntry = run.filenames.find(fhr3);
    if (fileEntry == run.filenames.end()) {
        status = product.label + " not available for f" + fhr3;
        UtilityLog::d("UtilitySpcPost: f" + fhr3 + " not in filenames map (" + To::string(static_cast<int>(run.filenames.size())) +
            " hours known) for run " + run.dateStr + run.cycle);
        return "";
    }
    const auto& fileName = fileEntry->second;
    const auto runKey = run.dateStr + run.cycle;
    const auto dir = QString::fromStdString(cacheDir());

    const QDateTime runUtc{
        QDate{To::Int(run.dateStr.substr(0, 4)), To::Int(run.dateStr.substr(4, 2)), To::Int(run.dateStr.substr(6, 2))},
        QTime{To::Int(run.cycle), 0}, QTimeZone::utc()};
    const auto forecastHourInt = To::Int(fhr3);
    const auto validUtc = runUtc.addSecs(3600 * forecastHourInt);
    const auto validLocal = validUtc.toLocalTime();
    const auto localZone = QTimeZone::systemTimeZone().abbreviation(validLocal);
    status = "SPC Post " + run.dateStr.substr(0, 4) + "-" + run.dateStr.substr(4, 2) + "-" + run.dateStr.substr(6, 2) +
        " " + run.cycle + "z    F" + fhr3 + " valid " + validLocal.toString("ddd h:mm AP").toStdString() +
        " " + localZone.toStdString() + "    " + product.label + "    " + domain.label;

    // final image cached per run + product + domain + boundary + background + forecast hour.
    // "sp4" is the render version - bump it whenever the drawing pipeline changes.
    // boundaryFlags is now a bitmask (see UtilitySpcPost::BoundaryState etc.),
    // not a combo index, but folds into this same cache-key slot unchanged.
    const auto pngPath = dir + QString::fromStdString(
        "/sp4_" + runKey + "_" + product.key + "_" + To::string(domainIndex) + "_" +
        To::string(boundaryFlags) + "_" + To::string(backgroundIndex) + "_" + fhr3 + ".png");
    if (QFile::exists(pngPath)) {
        const auto sidecar = pngPath + ".grid";
        if (QFile::exists(sidecar)) {
            samplePath = sidecar.toStdString();
        }
        return pngPath.toStdString();
    }

    const auto gribPath = dir + QString::fromStdString("/g_" + runKey + "_" + product.key + "_" + fhr3 + ".grib2");
    if (!QFile::exists(gribPath) || QFileInfo{gribPath}.size() < 200) {
        // no cycle-hour subdirectory - all of a day's cycles live in one
        // per-product folder, differentiated by the "tCCz" in the filename
        // (matches the listing path used in resolveRun() above)
        const auto url = baseUrl + "/spc_post." + run.dateStr + "/" + product.subdir + "/" + fileName;
        // be gentle on NOMADS - a short randomized pause before each NEW grib2 fetch
        // (skipped when the file is already cached) avoids hammering the server with
        // a tight request burst during an animation sweep, which can trigger a block
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 + QRandomGenerator::global()->bounded(500)));
        const auto bytes = UtilityIO::downloadAsByteArray(url);
        if (bytes.size() < 200 || bytes.left(4) != QByteArray{"GRIB"}) {
            status = product.label + " download failed for f" + fhr3;
            UtilityLog::d("UtilitySpcPost: download failed for " + url + " (" + To::string(static_cast<int>(bytes.size())) + " bytes)");
            return "";
        }
        QFile out{gribPath};
        if (!out.open(QIODevice::WriteOnly)) {
            return "";
        }
        out.write(bytes);
        out.close();
    }

    const auto tag = QString::fromStdString(runKey + "_" + product.key + "_" + To::string(domainIndex) + "_" + fhr3);
    const auto colorPath = dir + "/col_" + tag + ".txt";
    const auto warpPath = dir + "/w_" + tag + ".tif";
    const auto tiffPath = dir + "/c_" + tag + ".tif";
    {
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(probColorMap.c_str(), static_cast<qint64>(probColorMap.size()));
            colorFile.close();
        }
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    auto runProcess = [&status] (const QString& program, const QStringList& args) {
        QProcess process;
        process.start(program, args);
        process.waitForFinished(30000);
        const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
        if (!ok) {
            const auto err = process.readAllStandardError();
            status = "gdal failed: " + err.left(200).toStdString();
            UtilityLog::d("UtilitySpcPost: " + program.toStdString() + " " + args.join(" ").toStdString() +
                " failed: " + QString::fromUtf8(err).toStdString());
        }
        return ok;
    };

    const auto fillCols = QString::number(mainRenderColumns(domain));
    auto ok = runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(domain.west), fixedQ(domain.south), fixedQ(domain.east), fixedQ(domain.north),
             "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath});

    // gdaldem color-relief -alpha does NOT make nodata pixels transparent -
    // it clamps them opaque to the nearest colormap stop instead (verified
    // directly on GDAL 3.13.3 - see UtilityGrib's main pipeline, which had
    // the same bug). Build an explicit nodata mask, colorize WITHOUT
    // -alpha, then merge the two into a correct 4-band RGBA tif.
    //
    // probColorMap's own <10% bin is ALSO meant to be transparent (its "0 0
    // 0 0 0"/"9 0 0 0 0" rows - "no meaningful forecast signal here", the
    // same intent reflectivity's <5dBZ "no echo" bin has) - the nodata-only
    // mask above doesn't know that, so AND in a value-based mask too, or a
    // quiet day (real probabilities all under 10%, as confirmed live against
    // this domain/run) renders as a solid opaque black rectangle instead of
    // transparent.
    const auto maskPath = dir + "/m_" + tag + ".tif";
    const auto sigMaskPath = dir + "/sm_" + tag + ".tif";
    const auto rgbPath = dir + "/rgb_" + tag + ".tif";
    const auto gdalCalc = bin + (QFile::exists(bin + "gdal_calc") ? "gdal_calc" : "gdal_calc.py");
    const auto gdalMerge = bin + (QFile::exists(bin + "gdal_merge") ? "gdal_merge" : "gdal_merge.py");
    ok = ok && runProcess(gdalCalc, {"-A", warpPath, "--calc=255*(A!=-9999)", "--outfile=" + maskPath,
                                      "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"});
    ok = ok && runProcess(gdalCalc, {"-A", warpPath, "--calc=255*(A>=10)", "--outfile=" + sigMaskPath,
                                      "--overwrite", "--quiet", "--type=Byte", "--NoDataValue=0"});
    if (ok) {
        const auto combinedPath = dir + "/cm_" + tag + ".tif";
        if (runProcess(gdalCalc, {"-A", maskPath, "-B", sigMaskPath, "--calc=minimum(A,B)",
                                  "--outfile=" + combinedPath, "--overwrite", "--quiet",
                                  "--type=Byte", "--NoDataValue=0"})) {
            QFile::remove(maskPath);
            QFile::rename(combinedPath, maskPath);
        }
    }
    QFile::remove(sigMaskPath);
    ok = ok && runProcess(bin + "gdaldem", {"color-relief", "-q", "-of", "GTiff", warpPath, colorPath, rgbPath});
    ok = ok && runProcess(gdalMerge, {"-q", "-o", tiffPath, "-separate", "-co", "PHOTOMETRIC=RGB", rgbPath, maskPath});
    for (const auto& stale : {maskPath, rgbPath}) {
        QFile::remove(stale);
    }
    if (!ok) {
        for (const auto& stale : {colorPath, warpPath, tiffPath}) {
            QFile::remove(stale);
        }
        return "";
    }

    // point-value sidecar for the hover read-out: a coarse XYZ dump of the raw
    // 0-100% probability field (before boundary lines are burned into tiffPath),
    // same extent as the final PNG. A failure here must not break the
    // (already set) status line.
    {
        const auto savedStatus = status;
        const auto gridPath = pngPath + ".grid";
        if (runProcess(bin + "gdal_translate",
                {"-q", "-of", "XYZ", "-outsize", QString::number(sampleGridColumns(domain)), "0", warpPath, gridPath})) {
            samplePath = gridPath.toStdString();
        } else {
            status = savedStatus;
        }
    }

    auto burnLines = [&] (const string& geoJson, int red, int green, int blue) {
        runProcess(bin + "gdal_rasterize",
            {"-q", "-b", "1", "-b", "2", "-b", "3", "-b", "4",
             "-burn", QString::number(red), "-burn", QString::number(green),
             "-burn", QString::number(blue), "-burn", "255",
             QString::fromStdString(geoJson), tiffPath});
    };
    // independent toggles, not mutually exclusive - always composited in
    // this fixed order regardless of which combination is set: geographic
    // reference lines first (thinnest/densest), CWAs next, state lines last
    // so they stay visually prominent on top of everything else.
    if (boundaryFlags & BoundaryLake) {
        burnLines(UtilityGrib::lakeLinesGeoJson(), 70, 130, 180);
    }
    if (boundaryFlags & BoundaryCanada) {
        burnLines(UtilityGrib::canadaLinesGeoJson(), 120, 90, 160);
    }
    if (boundaryFlags & BoundaryMexico) {
        burnLines(UtilityGrib::mexicoLinesGeoJson(), 160, 110, 60);
    }
    if (boundaryFlags & BoundaryHighway) {
        burnLines(UtilityGrib::highwayLinesGeoJson(), 190, 70, 40);
    }
    if (boundaryFlags & BoundaryCounty) {
        burnLines(UtilityGrib::countyLinesGeoJson(), 150, 130, 90);
    }
    if (boundaryFlags & BoundaryCwa) {
        burnLines(UtilityGrib::cwaLinesGeoJson(), 30, 110, 210);
    }
    if (boundaryFlags & BoundaryState) {
        burnLines(UtilityGrib::stateLinesGeoJson(), 20, 20, 20);
    }

    const auto translated = runProcess(bin + "gdal_translate", {"-q", "-of", "PNG", tiffPath, pngPath});
    for (const auto& stale : {colorPath, warpPath, tiffPath}) {
        QFile::remove(stale);
    }
    if (!translated || !QFile::exists(pngPath)) {
        return "";
    }

    // optional RRFS temp/reflectivity background under the product's own
    // shading (transparent below ~10% probability, so the background shows
    // through there) - aligned to the RRFS run/lead whose valid time is
    // closest to this product's own valid time, not just "latest RRFS"
    if (backgroundIndex > 0) {
        string rrfsDate;
        string rrfsCycle;
        if (UtilityGrib::getLatestRun(rrfsDate, rrfsCycle)) {
            const QDateTime rrfsRunUtc{
                QDate{To::Int(rrfsDate.substr(0, 4)), To::Int(rrfsDate.substr(4, 2)), To::Int(rrfsDate.substr(6, 2))},
                QTime{To::Int(rrfsCycle), 0}, QTimeZone::utc()};
            auto leadHours = static_cast<int>(std::llround(rrfsRunUtc.secsTo(validUtc) / 3600.0));
            const auto maxLead = (To::Int(rrfsCycle) % 6 == 0) ? 84 : 18;
            leadHours = std::clamp(leadHours, 1, maxLead);
            const UtilityGrib::Bbox bbox{domain.west, domain.south, domain.east, domain.north};
            const auto kind = (backgroundIndex == 1) ? string{"temp"} : string{"reflectivity"};
            string backgroundStatus;
            const auto backgroundPath = UtilityGrib::renderBackground(
                kind, bbox, To::string(leadHours), rrfsDate + rrfsCycle, backgroundStatus);
            if (!backgroundPath.empty()) {
                const QImage background{QString::fromStdString(backgroundPath)};
                const QImage foreground{pngPath};
                if (!background.isNull() && !foreground.isNull()) {
                    QImage merged{foreground.size(), QImage::Format_ARGB32};
                    merged.fill(Qt::transparent);
                    QPainter painter{&merged};
                    painter.drawImage(merged.rect(), background);   // stretch-to-fit covers any rounding mismatch
                    painter.drawImage(0, 0, foreground);
                    painter.end();
                    merged.save(pngPath, "PNG");
                }
            }
        }
    }

    // drawn last, on top of every line layer and the optional background,
    // same as the other text overlays (contour labels) do elsewhere
    if (boundaryFlags & BoundaryCity) {
        UtilityGrib::drawCityLabels(pngPath, {domain.west, domain.south, domain.east, domain.north});
    }

    return pngPath.toStdString();
}
