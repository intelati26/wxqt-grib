// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/HomeThumbnails.h"
#include <algorithm>
#include <QFile>
#include "common/GlobalVariables.h"
#include "misc/UtilityOpcImages.h"
#include "models/UtilityGrib.h"
#include "spc/UtilitySpcCompmap.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "spc/UtilitySpcSwo.h"
#include "util/DownloadImage.h"
#include "util/UtilityIO.h"
#include "vis/UtilityGoes.h"
#include "vis/UtilityGoesFullDisk.h"
#include "wpc/UtilityWpcRainfallOutlook.h"
#include "settings/Location.h"
#include "util/Utility.h"

namespace {
    const string gribLastFieldPref{"GRIB_LAST_FIELD"};
    const string gribLastRegionPref{"GRIB_LAST_REGION"};

    int indexOfLabel(const vector<string>& labels, const string& label) {
        const auto found = std::find(labels.begin(), labels.end(), label);
        return found == labels.end() ? 0 : static_cast<int>(found - labels.begin());
    }
}

const vector<HomeThumbnails::Entry>& HomeThumbnails::all() {
    // existing tokens keep their names (saved preferences); the order here is the default order
    static const vector<Entry> entries{
        {"RADAR_MOSAIC", "Radar Mosaic", "radarmosaicnws.png", false, true},
        {"VISIBLE_SATELLITE", "Visible Satellite", "baseline_cloud_black_48dp.png", false, true},
        {"USWARN", "Alerts", "uswarn.png", false, false},
        {"ANALYSIS_RADAR_AND_WARNINGS", "Analysis", "fmap.png", false, false},
        {"RTMA_TEMP", "RTMA Temp", "rtma.png", false, false},
        {"SPC_MESO_MSLP", "SPC Meso - MSLP", "", true, false},
        {"SPC_MESO_500MB", "SPC Meso - 500mb", "", true, false},
        {"SPC_SOUNDING", "SPC Sounding (nearest site)", "spcsoundings.png", true, false},
        {"SPC_DAY1", "SPC Convective Outlook Day 1", "day1.png", false, false},
        {"SPC_DAY2", "SPC Convective Outlook Day 2", "day2.png", false, false},
        {"SPC_DAY3", "SPC Convective Outlook Day 3", "day3.png", false, false},
        {"SPC_DAY48", "SPC Convective Outlook Day 4-8", "day48.png", false, false},
        {"SPC_STORM_REPORTS", "SPC Storm Reports - today", "report_today.png", false, false},
        {"SPC_FIRE_DAY1", "SPC Fire Weather Outlook Day 1", "fire_outlook.png", false, false},
        {"SPC_COMPMAP", "SPC Compmap", "spccompmap.png", true, false},
        {"WPC_RAINFALL_DAY1", "WPC Rainfall Outlook Day 1", "wpc_rainfall.png", false, false},
        {"OPC_SURFACE", "Ocean Prediction Center - surface", "opc.png", false, false},
        {"GOES_GLOBAL", "Global GOES", "goesfulldisk.png", false, false},
        {"LIGHTNING", "Lightning (GLM)", "lightning.png", false, false},
        {"NHC_ATLANTIC", "NHC Atlantic outlook", "nhc.png", false, false},
        {"GRIB_LATEST", "RRFS GRIB - your last field and region, latest run", "grib.png", true, false},
    };
    return entries;
}

const HomeThumbnails::Entry * HomeThumbnails::find(const string& token) {
    for (const auto& entry : all()) {
        if (entry.token == token) {
            return &entry;
        }
    }
    return nullptr;
}

QByteArray HomeThumbnails::fetch(const string& token) {
    if (token == "GRIB_LATEST") {
        // the viewer remembers the field and region last looked at; the first forecast hour of the newest run
        const auto field = indexOfLabel(UtilityGrib::fieldLabels(), Utility::readPref(gribLastFieldPref, ""));
        const auto region = indexOfLabel(UtilityGrib::regions(), Utility::readPref(gribLastRegionPref, ""));
        const auto hours = UtilityGrib::forecastHours();
        string status;
        string samplePath;
        double lo = 0.0;
        double hi = 0.0;
        const auto path = UtilityGrib::render(field, region, hours.empty() ? string{"01"} : hours.front(), "", status, lo, hi, samplePath);
        QFile file{QString::fromStdString(path)};
        return !path.empty() && file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
    }
    string url;
    if (token == "SPC_DAY1" || token == "SPC_DAY2" || token == "SPC_DAY3") {
        const auto urls = UtilitySpcSwo::getImageUrls(token.back() - '0');
        url = urls.empty() ? string{} : urls.front();
    } else if (token == "SPC_DAY48") {
        const auto urls = UtilitySpcSwo::getImageUrls(48);
        url = urls.empty() ? string{} : urls.front();
    } else if (token == "SPC_STORM_REPORTS") {
        url = DownloadImage::byProduct("STRPT");
    } else if (token == "SPC_FIRE_DAY1") {
        url = UtilitySpcFireOutlook::urls.front();
    } else if (token == "SPC_COMPMAP") {
        url = UtilitySpcCompmap::getImage(UtilitySpcCompmap::urlIndices.front());
    } else if (token == "WPC_RAINFALL_DAY1") {
        url = UtilityWpcRainfallOutlook::urls.front();
    } else if (token == "OPC_SURFACE") {
        url = UtilityOpcImages::urls.front();
    } else if (token == "GOES_GLOBAL") {
        url = UtilityGoesFullDisk::urls.front();
    } else if (token == "LIGHTNING") {
        url = UtilityGoes::getImage("GLM", "CONUS");
    } else if (token == "NHC_ATLANTIC") {
        url = GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_2d0.png";
    } else {
        url = DownloadImage::byProduct(token);
    }
    return url.empty() ? QByteArray{} : UtilityIO::downloadAsByteArray(url);
}
