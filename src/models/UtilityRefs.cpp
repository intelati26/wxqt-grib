// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/RenderLock.h"
#include "models/UtilityRefs.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <QByteArray>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QColor>
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

    // Threshold-driven rows (Stages 3-4). One table drives three row kinds
    // per variable: "Paintball" (each RRFS Ensemble member's exceedance
    // area), "Member Probability" (members-exceeding / members, 5-member
    // ensemble) and "REFS Probability" (REFS's own pre-computed `prob` bands,
    // ~14-source pool - only a fixed set of thresholds exists there, so a
    // requested threshold snaps to the nearest). memberKey ties a row to the
    // per-member field rows above (nullptr = REFS-only variable); probIdx is
    // the idx prefix inside refs.*.prob (nullptr = not published there).
    struct ThresholdSpec {
        const char* label;
        const char* key;
        const char* memberKey;
        const char* units;
        double defaultThreshold;
        const char* probLabel;
        const char* probIdx;
        vector<double> probThresholds;   // as published in the idx (text like ">25.4")
        // accumulation records only: window length in hours (1, 3) or 0 for
        // "since forecast start"; the idx text is "<h-w>-<h> hour acc fcst",
        // so it depends on the forecast hour. -1 = not an accumulation.
        int probWindow{-1};
        // published unit -> unit shown to the user (snow is published in
        // metres, shown in inches)
        double displayScale{1.0};
    };
    const vector<ThresholdSpec> thresholdSpecs{
        {"Composite Reflectivity", "refc", "refc", "dBZ", 40.0,
            "Composite Reflectivity", ":REFC:entire atmosphere", {10, 20, 30, 40, 50}},
        {"Updraft Helicity 2-5km", "uphl25", "uphl25", "m2/s2", 75.0,
            "Updraft Helicity 2-5km", ":MXUPHL:5000-2000 m above ground", {25, 75, 150}},
        {"Surface CAPE", "cape", "cape", "J/kg", 1000.0,
            "Mixed-Layer CAPE (90-0mb)", ":CAPE:90-0 mb above ground", {500, 1000, 1500, 2000, 3000}},
        {"Wind Gust", "gust", "gust", "m/s", 25.7, nullptr, nullptr, {}},
        {"1km Reflectivity", "refd1km", nullptr, "dBZ", 40.0,
            "1km Reflectivity", ":REFD:1000 m above ground", {30, 40, 50}},
        {"0-3km Helicity", "hlcy3", nullptr, "m2/s2", 200.0,
            "0-3km Storm-Relative Helicity", ":HLCY:3000-0 m above ground", {100, 200, 400}},
        {"1-hr Precipitation", "apcp1h", nullptr, "mm", 12.7,
            "1-hr Precipitation", ":APCP:surface:", {12.7, 25.4, 50.8, 76.2}, 1},
        {"3-hr Precipitation", "apcp3h", nullptr, "mm", 25.4,
            "3-hr Precipitation", ":APCP:surface:", {12.7, 25.4, 50.8, 76.2, 127}, 3},
        {"Total Precipitation", "apcptot", nullptr, "mm", 25.4,
            "Total Precipitation", ":APCP:surface:", {12.7, 25.4, 50.8, 76.2, 127}, 0},
        {"3-hr Snowfall", "snow3h", nullptr, "in", 1.0,
            "3-hr Snowfall", ":ASNOW:surface:", {0.025, 0.076, 0.152}, 3, 39.37},
        {"Total Snowfall", "snowtot", nullptr, "in", 1.0,
            "Total Snowfall", ":ASNOW:surface:", {0.025, 0.076, 0.152, 0.304}, 0, 39.37},
        {"Total Freezing Rain", "frzrtot", nullptr, "mm", 0.254,
            "Total Freezing Rain", ":FRZR:surface:", {0.254, 2.54, 6.35, 12.7}, 0},
    };

    // 0-100 % scale shared by both probability row kinds; alpha-0 below 5 %
    // (near-step stop so the ramp stays crisp)
    const string probColorMap{
        "0 0 0 0 0\n" "4.999 0 0 0 0\n" "5 200 230 245\n" "20 120 200 235\n" "40 110 190 120\n"
        "60 240 225 100\n" "80 245 150 50\n" "100 200 40 40\n"};

    const ThresholdSpec * thresholdSpecFor(const string& fieldKey) {
        for (const auto& spec : thresholdSpecs) {
            if (fieldKey == string{"pb_"} + spec.key || fieldKey == string{"pm_"} + spec.key ||
                fieldKey == string{"rp_"} + spec.key) {
                return &spec;
            }
        }
        return nullptr;
    }

    // filename-safe threshold tag, e.g. 25.7 -> "t25p7", -50 -> "tm50"
    QString thresholdTag(double threshold) {
        auto text = QString::number(threshold, 'g', 6);
        text.replace('.', 'p').replace('-', 'm');
        return "t" + text;
    }

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
        for (const auto& spec : thresholdSpecs) {
            if (spec.memberKey != nullptr) {
                result.push_back(UtilityGrib::Field{string{"Paintball "} + spec.label, string{"pb_"} + spec.key,
                                                     spec.units, "", "", "pb"});
            }
        }
        for (const auto& spec : thresholdSpecs) {
            if (spec.memberKey != nullptr) {
                result.push_back(UtilityGrib::Field{string{"Member Probability "} + spec.label,
                                                     string{"pm_"} + spec.key, "%", "", "", "pm"});
            }
        }
        for (const auto& spec : thresholdSpecs) {
            if (spec.probIdx != nullptr) {
                result.push_back(UtilityGrib::Field{string{"REFS Probability "} + spec.probLabel,
                                                     string{"rp_"} + spec.key, "%", spec.probIdx, probColorMap,
                                                     "prob"});
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

vector<string> UtilityRefs::kindLabels() {
    return {"REFS Mean / Spread / PMM", "RRFS Ensemble Member", "Paintball (members)", "Member Probability",
            "REFS Probability"};
}

int UtilityRefs::kindOf(int fieldIndex) {
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return 0;
    }
    const auto& product = fields[fieldIndex].product;
    if (isMemberProduct(product)) {
        return 1;
    }
    if (product == "pb") {
        return 2;
    }
    if (product == "pm") {
        return 3;
    }
    if (product == "prob") {
        return 4;
    }
    return 0;
}

vector<int> UtilityRefs::kindFieldIndices(int kind) {
    vector<int> result;
    for (int i = 0; i < static_cast<int>(fields.size()); i += 1) {
        if (kindOf(i) != kind) {
            continue;
        }
        if (kind == 1 && fields[i].product != "m001") {
            continue;   // one entry per variable; the member is a separate pick
        }
        result.push_back(i);
    }
    return result;
}

string UtilityRefs::variableLabel(int fieldIndex) {
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return {};
    }
    auto label = fields[fieldIndex].label;
    for (const string prefix : {"RRFS Ens Member 1 ", "RRFS Ens Member 2 ", "RRFS Ens Member 3 ",
                                 "RRFS Ens Member 4 ", "RRFS Ens Member 5 ", "Paintball ", "Member Probability ",
                                 "REFS Probability "}) {
        if (label.rfind(prefix, 0) == 0) {
            return label.substr(prefix.size());
        }
    }
    return label;
}

bool UtilityRefs::isMemberField(int fieldIndex) {
    return kindOf(fieldIndex) == 1;
}

int UtilityRefs::memberOf(int fieldIndex) {
    return isMemberField(fieldIndex) ? fields[fieldIndex].product[3] - '0' : 0;
}

int UtilityRefs::memberFieldIndex(int anyMemberRow, int member) {
    if (!isMemberField(anyMemberRow)) {
        return anyMemberRow;
    }
    const auto& key = fields[anyMemberRow].key;
    const auto wanted = key.substr(0, key.rfind("_m")) + "_m" + To::string(member);
    for (int i = 0; i < static_cast<int>(fields.size()); i += 1) {
        if (fields[i].key == wanted) {
            return i;
        }
    }
    return anyMemberRow;
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
                                  int forecastHourInt, QString& gribPathOut, string& status,
                                  const string& alsoContains) {
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
    if (idx.empty() || !UtilityGrib::idxByteRange(idx, field.idxMatch, start, end, 0, alsoContains)) {
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

QColor UtilityRefs::memberColor(int member) {
    const auto index = std::clamp(member - 1, 0, 7);
    return QColor{memberColors[index][0], memberColors[index][1], memberColors[index][2]};
}

bool UtilityRefs::memberBasis(int fieldIndex, double panelThreshold, MemberBasis& basis) {
    if (fieldIndex < 0 || fieldIndex >= static_cast<int>(fields.size())) {
        return false;
    }
    const auto& field = fields[fieldIndex];
    string base;
    if (isMemberProduct(field.product)) {
        base = field.key.substr(0, field.key.rfind("_m"));
    } else if (const auto * spec = thresholdSpecFor(field.key); spec != nullptr && spec->memberKey != nullptr) {
        base = spec->memberKey;
        basis.hasThreshold = true;
        basis.threshold = std::isnan(panelThreshold) ? spec->defaultThreshold : panelThreshold;
    } else {
        return false;
    }
    for (const auto& candidate : fields) {
        if (candidate.key == base + "_m1") {
            basis.memberKey = base;
            basis.units = candidate.units;
            const auto marker = string{"Member 1 "};
            const auto at = candidate.label.find(marker);
            basis.label = at == string::npos ? candidate.label : candidate.label.substr(at + marker.size());
            return true;
        }
    }
    return false;
}

vector<double> UtilityRefs::memberPointValues(const string& memberKey, const string& dateStr, const string& cycle,
                                               int forecastHourInt, double lon, double lat, string& status) {
    vector<double> values(memberCount, std::nan(""));
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return values;
    }
    const auto bin = QString::fromStdString(binDir) + "/";
    for (int member = 1; member <= memberCount; member += 1) {
        const auto key = memberKey + "_m" + To::string(member);
        const UtilityGrib::Field * memberField = nullptr;
        for (const auto& candidate : fields) {
            if (candidate.key == key) {
                memberField = &candidate;
            }
        }
        QString gribPath;
        string memberStatus;
        if (memberField == nullptr || !fetchFieldGrib(*memberField, dateStr, cycle, forecastHourInt, gribPath, memberStatus)) {
            status = memberStatus;
            continue;
        }
        QProcess process;
        process.start(bin + "gdallocationinfo",
                      {"-valonly", "-wgs84", gribPath, QString::number(lon, 'f', 4), QString::number(lat, 'f', 4)});
        process.waitForFinished(30000);
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            continue;
        }
        bool ok = false;
        const auto value = QString::fromUtf8(process.readAllStandardOutput()).trimmed().toDouble(&ok);
        if (ok && value > -9000.0) {
            values[member - 1] = value;
        }
    }
    return values;
}

bool UtilityRefs::usesThreshold(int fieldIndex) {
    return fieldIndex >= 0 && fieldIndex < static_cast<int>(fields.size()) &&
        thresholdSpecFor(fields[fieldIndex].key) != nullptr;
}

double UtilityRefs::defaultThreshold(int fieldIndex) {
    const auto * spec = (fieldIndex >= 0 && fieldIndex < static_cast<int>(fields.size()))
        ? thresholdSpecFor(fields[fieldIndex].key) : nullptr;
    return spec ? spec->defaultThreshold : 0.0;
}

string UtilityRefs::thresholdUnits(int fieldIndex) {
    const auto * spec = (fieldIndex >= 0 && fieldIndex < static_cast<int>(fields.size()))
        ? thresholdSpecFor(fields[fieldIndex].key) : nullptr;
    return spec ? spec->units : string{};
}

// One member's field, fetched (cached) and warped to the region bbox.
bool UtilityRefs::warpMember(const string& memberKey, int member, const string& dateStr, const string& cycle,
                              int forecastHourInt, const UtilityGrib::Bbox& box, const QString& bin,
                              const QString& warpPath, string& status) {
    const auto key = memberKey + "_m" + To::string(member);
    const UtilityGrib::Field * memberField = nullptr;
    for (const auto& candidate : fields) {
        if (candidate.key == key) {
            memberField = &candidate;
        }
    }
    QString gribPath;
    if (memberField == nullptr || !fetchFieldGrib(*memberField, dateStr, cycle, forecastHourInt, gribPath, status)) {
        return false;
    }
    return runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
             "-r", "bilinear", "-ts", QString::number(UtilityGrib::mainRenderColumns(box)), "0", gribPath, warpPath},
            status);
}

// Top-left key drawn onto a finished PNG: a title line, then either one
// swatch per (label, colour) entry or, for a probability map, a 0-100 %
// colour bar.
void UtilityRefs::drawLegend(const QString& pngPath, const QString& title,
                              const vector<std::pair<QString, QColor>>& swatches, bool probabilityBar) {
    QImage image;
    if (!image.load(pngPath)) {
        return;
    }
    image = image.convertToFormat(QImage::Format_ARGB32);
    QPainter painter{&image};
    painter.setRenderHint(QPainter::Antialiasing);
    const int unit = std::max(11, image.width() / 75);
    QFont font = painter.font();
    font.setPixelSize(unit);
    font.setBold(true);
    painter.setFont(font);
    const int rowHeight = unit + 6;
    const int pad = unit / 2;
    const int textWidth = painter.fontMetrics().horizontalAdvance(title);
    const int boxWidth = std::max({unit * 14, textWidth + unit, probabilityBar ? unit * 16 : 0});
    const int rows = probabilityBar ? 2 : static_cast<int>(swatches.size()) + 1;
    const int boxHeight = rowHeight * rows + 8 + (probabilityBar ? unit : 0);
    painter.fillRect(QRect{6, 6, boxWidth, boxHeight}, QColor{255, 255, 255, 215});
    painter.setPen(QColor{30, 30, 30});
    painter.drawText(QPoint{6 + pad, 6 + rowHeight}, title);
    if (probabilityBar) {
        const int barLeft = 6 + pad;
        const int barTop = 6 + rowHeight + 6;
        const int barWidth = boxWidth - 2 * pad;
        const struct { double pct; QColor color; } stops[]{
            {0, QColor{255, 255, 255}}, {5, QColor{200, 230, 245}}, {20, QColor{120, 200, 235}},
            {40, QColor{110, 190, 120}}, {60, QColor{240, 225, 100}}, {80, QColor{245, 150, 50}},
            {100, QColor{200, 40, 40}}};
        for (int x = 0; x < barWidth; x += 1) {
            const auto pct = 100.0 * x / (barWidth - 1);
            QColor color = stops[6].color;
            for (int k = 0; k < 6; k += 1) {
                if (pct <= stops[k + 1].pct) {
                    const auto f = (pct - stops[k].pct) / (stops[k + 1].pct - stops[k].pct);
                    color = QColor::fromRgbF(
                        stops[k].color.redF() + f * (stops[k + 1].color.redF() - stops[k].color.redF()),
                        stops[k].color.greenF() + f * (stops[k + 1].color.greenF() - stops[k].color.greenF()),
                        stops[k].color.blueF() + f * (stops[k + 1].color.blueF() - stops[k].color.blueF()));
                    break;
                }
            }
            painter.setPen(color);
            painter.drawLine(barLeft + x, barTop, barLeft + x, barTop + unit);
        }
        painter.setPen(QColor{30, 30, 30});
        font.setBold(false);
        font.setPixelSize(std::max(9, unit - 2));
        painter.setFont(font);
        for (int pct = 0; pct <= 100; pct += 25) {
            painter.drawText(QPoint{barLeft + (barWidth - 1) * pct / 100 - (pct == 100 ? unit * 2 : 0),
                                    barTop + unit + rowHeight - 2}, QString::number(pct) + (pct == 100 ? "%" : ""));
        }
    } else {
        int row = 1;
        for (const auto& swatch : swatches) {
            const int top = 6 + 4 + rowHeight * row + 3;
            painter.fillRect(QRect{6 + pad, top, unit * 2, unit}, swatch.second);
            painter.drawRect(QRect{6 + pad, top, unit * 2, unit});
            painter.drawText(QPoint{6 + pad + unit * 2 + 6, top + unit - 1}, swatch.first);
            row += 1;
        }
    }
    painter.end();
    image.save(pngPath, "PNG");
}

// Colorize a region-warped float GeoTIFF into the final PNG: colour table
// (leading "nv" = transparent nodata), min/max range file, hover-sample
// sidecar, CWA/state lines, optional legend. Consumes warpPath.
string UtilityRefs::finishRender(const QString& warpPath, const string& colorMap, const QString& tag,
                                  const UtilityGrib::Bbox& box, const QString& bin, const QString& pngPath,
                                  string& status, double& dataMin, double& dataMax, string& samplePath,
                                  const QString& legendTitle, bool probabilityLegend) {
    const auto dir = QString::fromStdString(cacheDir());
    const auto rangePath = pngPath + ".range";
    const auto gridPath = pngPath + ".grid";
    const auto colorPath = dir + "/col_" + tag + ".txt";
    const auto tiffPath = dir + "/c_" + tag + ".tif";
    {
        // leading "nv" entry = colour for recognised-nodata pixels (transparent)
        const auto table = "nv 0 0 0 0\n" + colorMap;
        QFile colorFile{colorPath};
        if (colorFile.open(QIODevice::WriteOnly)) {
            colorFile.write(table.c_str(), static_cast<qint64>(table.size()));
            colorFile.close();
        }
    }
    // Colorize straight to RGBA - the "nv 0 0 0 0" entry makes recognised-
    // nodata pixels transparent, and the table's own alpha-0 stops are
    // honored by -alpha, so no gdal_calc mask / gdal_merge is needed (both
    // are Python scripts the portable builds don't bundle). Same reasoning
    // and verification as UtilityGrib::render().
    const auto ok = runProcess(bin + "gdaldem",
            {"color-relief", "-q", "-alpha", "-of", "GTiff", warpPath, colorPath, tiffPath}, status);
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
    for (const auto& stale : {colorPath, warpPath, tiffPath, tiffPath + ".aux.xml"}) {
        QFile::remove(stale);
    }
    if (!translated || !QFile::exists(pngPath)) {
        return "";
    }
    if (!legendTitle.isEmpty()) {
        drawLegend(pngPath, legendTitle, {}, probabilityLegend);
    }
    return pngPath.toStdString();
}

// Paintball plot: each available RRFS Ensemble member's area of exceedance
// for the threshold, drawn as a translucent blob in that member's own
// colour, all overlaid on one map - where blobs stack the members agree.
// Python-free: per-member colorize is a 3-stop gdaldem table (transparent
// below the threshold, member colour at alpha 110 at/above it), members are
// composited with QPainter, lines burned afterwards, legend drawn last.
string UtilityRefs::renderPaintball(const UtilityGrib::Field& field, int regionIndex, double threshold,
                                     const string& dateStr, const string& cycle, int forecastHourInt,
                                     const string& binDir, string& status, string& samplePath) {
    const auto * spec = thresholdSpecFor(field.key);
    if (spec == nullptr || spec->memberKey == nullptr) {
        status = "invalid paintball field";
        return "";
    }
    const auto fhr2 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 2);
    const auto runKey = dateStr + cycle;
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto dir = QString::fromStdString(cacheDir());
    // "pb2" is the render version - bump it whenever the drawing pipeline changes
    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2) +
        "_" + thresholdTag(threshold);
    const auto pngPath = dir + "/pb3_" + tag + ".png";
    const auto gridPath = pngPath + ".grid";
    const RenderLock renderLock{pngPath};   // see objects/RenderLock.h
    if (QFile::exists(pngPath)) {
        if (QFile::exists(gridPath)) {
            samplePath = gridPath.toStdString();
        }
        return pngPath.toStdString();
    }

    const auto bin = QString::fromStdString(binDir) + "/";
    QImage canvas;
    vector<int> drawn;
    QStringList keptWarps;   // kept until the members-exceeding count grid is built
    string lastError;
    for (int member = 1; member <= 5; member += 1) {
        const auto suffix = tag + "_m" + QString::number(member);
        const auto colorPath = dir + "/pbcol_" + suffix + ".txt";
        const auto warpPath = dir + "/pbw_" + suffix + ".tif";
        const auto tiffPath = dir + "/pbc_" + suffix + ".tif";
        const auto memberPng = dir + "/pbm_" + suffix + ".png";
        string memberStatus;
        if (!warpMember(spec->memberKey, member, dateStr, cycle, forecastHourInt, box, bin, warpPath, memberStatus)) {
            lastError = memberStatus;
            continue;
        }
        {
            const auto* rgb = memberColors[member - 1];
            const auto colorText = QString{"nv 0 0 0 0\n%1 %2 %3 %4 0\n%5 %2 %3 %4 110\n1000000 %2 %3 %4 110\n"}
                .arg(threshold - 0.001, 0, 'f', 3).arg(rgb[0]).arg(rgb[1]).arg(rgb[2])
                .arg(threshold, 0, 'f', 3).toUtf8();
            QFile colorFile{colorPath};
            if (colorFile.open(QIODevice::WriteOnly)) {
                colorFile.write(colorText);
                colorFile.close();
            }
        }
        const auto ok =
            runProcess(bin + "gdaldem", {"color-relief", "-q", "-alpha", "-of", "GTiff", warpPath, colorPath, tiffPath}, memberStatus)
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
        for (const auto& stale : {colorPath, tiffPath, memberPng}) {
            QFile::remove(stale);
        }
        if (drawn.empty() || drawn.back() != member) {
            QFile::remove(warpPath);
        } else {
            keptWarps << warpPath;
        }
    }
    // hover sidecar: how many members are at/above the threshold in each cell
    if (!keptWarps.isEmpty()) {
        const auto countPath = dir + "/pbcount_" + tag + ".tif";
        if (UtilityGrib::calcRaster(bin, keptWarps, [threshold, n = static_cast<int>(keptWarps.size())] (const double * v) {
                int exceeding = 0;
                for (int i = 0; i < n; i += 1) {
                    exceeding += v[i] >= threshold ? 1 : 0;
                }
                return static_cast<double>(exceeding);
            }, countPath)) {
            string ignored;
            const auto sampleCols = QString::number(UtilityGrib::mainRenderColumns(box) > 1000 ? 220 : 400);
            if (runProcess(bin + "gdal_translate", {"-q", "-of", "XYZ", "-outsize", sampleCols, "0", countPath, gridPath}, ignored)) {
                samplePath = gridPath.toStdString();
            }
        }
        QFile::remove(countPath);
        QFile::remove(countPath + ".aux.xml");
        for (const auto& warp : keptWarps) {
            QFile::remove(warp);
            QFile::remove(warp + ".aux.xml");
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

    vector<std::pair<QString, QColor>> swatches;
    for (const auto member : drawn) {
        const auto* rgb = memberColors[member - 1];
        swatches.push_back({QString{"Member %1"}.arg(member), QColor{rgb[0], rgb[1], rgb[2]}});
    }
    drawLegend(pngPath, QString{"Members >= %1 %2"}.arg(threshold, 0, 'g', 6).arg(spec->units), swatches, false);
    return pngPath.toStdString();
}

// From-members probability: percent of available RRFS Ensemble members at
// or above the threshold in each pixel (a 5-member ensemble - NOT the same
// pool as REFS's own `prob` bands; see docs/refs-viewer-plan.md). The count
// grid runs through the same finishRender() as every other field, so it gets
// the hover sidecar (value = percent) for free.
string UtilityRefs::renderMemberProbability(const UtilityGrib::Field& field, int regionIndex, double threshold,
                                             const string& dateStr, const string& cycle, int forecastHourInt,
                                             const string& binDir, string& status, double& dataMin,
                                             double& dataMax, string& samplePath) {
    const auto * spec = thresholdSpecFor(field.key);
    if (spec == nullptr || spec->memberKey == nullptr) {
        status = "invalid probability field";
        return "";
    }
    const auto fhr2 = WString::fixedLengthStringPad0(To::string(forecastHourInt), 2);
    const auto runKey = dateStr + cycle;
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto dir = QString::fromStdString(cacheDir());
    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2) +
        "_" + thresholdTag(threshold);
    // "pm1" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + "/pm1_" + tag + ".png";
    const RenderLock renderLock{pngPath};   // see objects/RenderLock.h
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

    const auto bin = QString::fromStdString(binDir) + "/";
    QStringList warps;
    string lastError;
    for (int member = 1; member <= 5; member += 1) {
        const auto warpPath = dir + "/pmw_" + tag + "_m" + QString::number(member) + ".tif";
        string memberStatus;
        if (warpMember(spec->memberKey, member, dateStr, cycle, forecastHourInt, box, bin, warpPath, memberStatus)) {
            warps << warpPath;
        } else {
            lastError = memberStatus;
            QFile::remove(warpPath);
        }
    }
    if (warps.isEmpty()) {
        status = "no ensemble members available for f" + fhr2 + (lastError.empty() ? "" : " (" + lastError + ")");
        return "";
    }
    const auto countPath = dir + "/pmcount_" + tag + ".tif";
    const auto count = static_cast<double>(warps.size());
    const auto ok = UtilityGrib::calcRaster(bin, warps, [threshold, count] (const double * v) {
        int exceeding = 0;
        for (int i = 0; i < static_cast<int>(count); i += 1) {
            exceeding += v[i] >= threshold ? 1 : 0;
        }
        return 100.0 * exceeding / count;
    }, countPath);
    for (const auto& warp : warps) {
        QFile::remove(warp);
        QFile::remove(warp + ".aux.xml");
    }
    if (!ok) {
        status = "member probability calculation failed";
        return "";
    }
    return finishRender(countPath, probColorMap, tag, box, bin, pngPath, status, dataMin, dataMax, samplePath,
                        QString{"P(>= %1 %2) pointwise, %3 RRFS Ens members"}.arg(threshold, 0, 'g', 6).arg(spec->units)
                            .arg(warps.size()), true);
}

string UtilityRefs::render(int fieldIndex, int regionIndex, const string& forecastHour, const string& runId,
                            string& status, double& dataMin, double& dataMax, string& samplePath,
                            double threshold) {
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
    auto field = fields[fieldIndex];
    const auto regionLabelList = UtilityGrib::regions();
    const auto regionLabel = (regionIndex >= 0 && regionIndex < static_cast<int>(regionLabelList.size()))
        ? regionLabelList[regionIndex] : string{"Unknown"};
    const auto box = UtilityGrib::regionBbox(regionIndex);
    const auto dir = QString::fromStdString(cacheDir());

    // threshold-driven rows: resolve the requested threshold (blank/NaN ->
    // the row's default; REFS `prob` rows snap to the nearest published band)
    const auto * spec = thresholdSpecFor(field.key);
    string alsoContains;
    string thresholdNote;
    if (spec != nullptr) {
        auto value = std::isnan(threshold) ? spec->defaultThreshold : threshold;
        if (field.product == "prob") {
            // nearest published band, compared in the units shown to the user
            const auto shown = [spec] (double published) { return published * spec->displayScale; };
            double bestPublished = spec->probThresholds.front();
            for (const auto candidate : spec->probThresholds) {
                if (std::fabs(shown(candidate) - value) < std::fabs(shown(bestPublished) - value)) {
                    bestPublished = candidate;
                }
            }
            value = spec->displayScale == 1.0 ? bestPublished : std::round(shown(bestPublished) * 10.0) / 10.0;
            // accumulation records carry the forecast-hour-dependent window
            string window;
            if (spec->probWindow >= 0) {
                const auto start = spec->probWindow == 0 ? 0 : forecastHourInt - spec->probWindow;
                window = To::string(start) + "-" + To::string(forecastHourInt) + " hour acc fcst";
            }
            alsoContains = (window.empty() ? string{} : ":" + window) + ":prob >" +
                QString::number(bestPublished, 'g', 6).toStdString() + ":";
            field.key += "_" + thresholdTag(value).toStdString();
        }
        threshold = value;
        thresholdNote = "    >= " + QString::number(value, 'g', 6).toStdString() + " " + spec->units;
    }

    const QDateTime runUtc{
        QDate{To::Int(dateStr.substr(0, 4)), To::Int(dateStr.substr(4, 2)), To::Int(dateStr.substr(6, 2))},
        QTime{To::Int(cycle), 0}, QTimeZone::utc()};
    const auto validLocal = runUtc.addSecs(3600 * forecastHourInt).toLocalTime();
    const auto localZone = QTimeZone::systemTimeZone().abbreviation(validLocal);
    status = "REFS " + dateStr.substr(0, 4) + "-" + dateStr.substr(4, 2) + "-" + dateStr.substr(6, 2) +
        " " + cycle + "z    F" + fhr2 + " valid " + validLocal.toString("ddd h:mm AP").toStdString() +
        " " + localZone.toStdString() + "    " + field.label + thresholdNote + "    " + regionLabel;

    if (field.product == "pb") {
        return renderPaintball(field, regionIndex, threshold, dateStr, cycle, forecastHourInt, binDir, status, samplePath);
    }
    if (field.product == "pm") {
        return renderMemberProbability(field, regionIndex, threshold, dateStr, cycle, forecastHourInt, binDir,
                                       status, dataMin, dataMax, samplePath);
    }

    // "rf3" is the render version - bump it whenever the drawing pipeline changes
    const auto pngPath = dir + QString::fromStdString(
        "/rf3_" + runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2 + ".png");
    const RenderLock renderLock{pngPath};   // see objects/RenderLock.h
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
    if (!fetchFieldGrib(field, dateStr, cycle, forecastHourInt, gribPath, status, alsoContains)) {
        // REFS publishes the 3-hr and since-start accumulation windows only
        // at forecast hours divisible by 3 (hourly windows exist every hour)
        if (spec != nullptr && field.product == "prob" && spec->probWindow >= 0 && spec->probWindow != 1 &&
                forecastHourInt % 3 != 0) {
            status = field.label + ": this accumulation window is only published at forecast hours divisible by 3 (F03, F06, ...)";
        }
        return "";
    }

    const auto tag = QString::fromStdString(runKey + "_" + field.key + "_" + To::string(regionIndex) + "_" + fhr2);
    const auto warpPath = dir + "/w_" + tag + ".tif";
    const auto bin = QString::fromStdString(binDir) + "/";
    const auto fillCols = QString::number(UtilityGrib::mainRenderColumns(box));
    if (!runProcess(bin + "gdalwarp",
            {"-q", "-overwrite", "-t_srs", "EPSG:4326", "-dstnodata", "-9999",
             "-te", fixedQ(box.west), fixedQ(box.south), fixedQ(box.east), fixedQ(box.north),
             "-r", "bilinear", "-ts", fillCols, "0", gribPath, warpPath}, status)) {
        QFile::remove(warpPath);
        return "";
    }
    // REFC spread in clear air is fake disagreement: the RRFS members do not
    // agree on the "no echo" fill value (some use -20 dBZ, some 0), so the
    // spread reads ~8-10 dBZ across the whole country. Where NO member has
    // echo (>= 5 dBZ, the display threshold) the spread is zero by
    // definition - force it to 0 there. Uses whichever members fetched.
    QString finishWarp = warpPath;
    if (field.key == "refc_sprd") {
        QStringList inputs{warpPath};
        QStringList memberWarps;
        for (int member = 1; member <= memberCount; member += 1) {
            const auto memberWarp = dir + "/sw_" + tag + "_m" + QString::number(member) + ".tif";
            string memberStatus;
            if (warpMember("refc", member, dateStr, cycle, forecastHourInt, box, bin, memberWarp, memberStatus)) {
                inputs << memberWarp;
                memberWarps << memberWarp;
            } else {
                QFile::remove(memberWarp);
            }
        }
        if (!memberWarps.isEmpty()) {
            const auto maskedPath = dir + "/sm_" + tag + ".tif";
            const auto members = static_cast<int>(memberWarps.size());
            if (UtilityGrib::calcRaster(bin, inputs, [members] (const double * v) {
                    double strongest = v[1];
                    for (int i = 2; i <= members; i += 1) {
                        strongest = std::max(strongest, v[i]);
                    }
                    return strongest < 5.0 ? 0.0 : v[0];
                }, maskedPath)) {
                finishWarp = maskedPath;
            }
        }
        for (const auto& memberWarp : memberWarps) {
            QFile::remove(memberWarp);
            QFile::remove(memberWarp + ".aux.xml");
        }
    }
    const auto legend = field.product == "prob"
        ? QString{"REFS neighborhood P(>= %1 %2), ~14-source pool"}.arg(threshold, 0, 'g', 6).arg(spec->units) : QString{};
    if (finishWarp != warpPath) {
        QFile::remove(warpPath);   // finishRender consumes the masked copy instead
    }
    return finishRender(finishWarp, field.colorMap, tag, box, bin, pngPath, status, dataMin, dataMax, samplePath,
                        legend, true);
}
