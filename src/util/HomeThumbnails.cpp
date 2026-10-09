// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/HomeThumbnails.h"
#include <algorithm>
#include <map>
#include <QFile>
#include "common/GlobalVariables.h"
#include "misc/UtilityOpcImages.h"
#include "mrms/UtilityMrms.h"
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
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
        {"TROPICAL_ATL_SAT", "Tropical overview - Atlantic (GOES satellite)", "nhc.png#2", false, false},
        {"TROPICAL_EPAC_SAT", "Tropical overview - East Pacific (GOES satellite)", "nhc.png#2", false, false},
        {"TROPICAL_WPAC_SAT", "Tropical overview - West Pacific (Himawari satellite, Guam sector)", "nhc.png#2", false, false},
        {"MRMS_RADAR", "MRMS radar - composite reflectivity around your location", "mcd_tile.png", false, false},
        {"MRMS_LATEST", "MRMS - latest scan of your last product, around your location", "mcd_tile.png", false, false},
        {"GRIB_LATEST", "Model viewer - your last model, chart and area, newest run", "grib.png", true, false},
    };
    return entries;
}

namespace {
    struct Words {
        string caption;
        string tip;
    };
    const std::map<string, Words>& words() {
        static const std::map<string, Words> table{
        {"RADAR_MOSAIC", {"Radar mosaic", "National Weather Service radar mosaic: reflectivity from the radars of one region, joined into one picture."}},
        {"VISIBLE_SATELLITE", {"Satellite", "GOES satellite picture of your area (the product and sector last used in the satellite viewer)."}},
        {"USWARN", {"Warnings", "Map of active NWS warnings across the United States."}},
        {"ANALYSIS_RADAR_AND_WARNINGS", {"Analysis", "WPC national analysis chart: radar, fronts and warnings together."}},
        {"RTMA_TEMP", {"Temperature now", "Real-Time Mesoscale Analysis: the current temperature analysis on a 2.5 km grid."}},
        {"SPC_MESO_MSLP", {"Mesoanalysis: sea level pressure", "SPC mesoscale analysis of mean sea level pressure, updated hourly."}},
        {"SPC_MESO_500MB", {"Mesoanalysis: 500 mb", "SPC mesoscale analysis of the 500 mb (mid-level) height and wind pattern, updated hourly."}},
        {"SPC_SOUNDING", {"Sounding", "Skew-T sounding from the SPC site nearest your location."}},
        {"SPC_DAY1", {"Severe outlook: day 1", "SPC categorical convective outlook for today."}},
        {"SPC_DAY2", {"Severe outlook: day 2", "SPC categorical convective outlook for tomorrow."}},
        {"SPC_DAY3", {"Severe outlook: day 3", "SPC convective outlook for day 3."}},
        {"SPC_DAY48", {"Severe outlook: days 4-8", "SPC probabilistic severe weather outlook for days 4 to 8."}},
        {"SPC_STORM_REPORTS", {"Storm reports today", "Preliminary tornado, hail and wind reports received by SPC today."}},
        {"SPC_FIRE_DAY1", {"Fire weather: day 1", "SPC fire weather outlook for today."}},
        {"SPC_COMPMAP", {"Compmap", "SPC mesoanalysis composite map: surface, upper air and radar features together."}},
        {"WPC_RAINFALL_DAY1", {"Rainfall outlook: day 1", "WPC excessive rainfall outlook for the next 24 hours."}},
        {"OPC_SURFACE", {"Ocean surface analysis", "Ocean Prediction Center surface analysis for the open waters."}},
        {"GOES_GLOBAL", {"Global satellite", "Full-disk satellite view (GOES, Himawari or Meteosat, as last chosen)."}},
        {"LIGHTNING", {"Lightning", "GOES Geostationary Lightning Mapper: recent lightning over the United States."}},
        {"NHC_ATLANTIC", {"Atlantic outlook", "National Hurricane Center two-day tropical weather outlook for the Atlantic."}},
        {"TROPICAL_ATL_SAT", {"Tropical Atlantic", "GOES-19 tropical Atlantic view: true colour by day, infrared at night."}},
        {"TROPICAL_EPAC_SAT", {"Eastern Pacific", "GOES-19 eastern Pacific view: true colour by day, infrared at night."}},
        {"TROPICAL_WPAC_SAT", {"Western Pacific", "Himawari infrared (colour-enhanced) view of the western Pacific from the Guam sector."}},
        {"MRMS_RADAR", {"MRMS radar", "Multi-Radar Multi-Sensor composite reflectivity around your location."}},
        {"MRMS_LATEST", {"MRMS product", "Latest scan of the MRMS product you looked at last, around your location."}},
        {"GRIB_LATEST", {"Model chart", "The model, chart and area you last looked at in the Model Viewer, from the newest run."}},
        };
        return table;
    }
}

string HomeThumbnails::caption(const string& token) {
    const auto found = words().find(token);
    return found == words().end() ? string{} : found->second.caption;
}

string HomeThumbnails::tip(const string& token) {
    const auto found = words().find(token);
    return found == words().end() ? string{} : found->second.tip;
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
    if (token == "MRMS_LATEST" || token == "MRMS_RADAR") {
        // the product last looked at in the MRMS viewer (else composite reflectivity), the newest scan, drawn around the
        // current location - or over all of CONUS when the preference MRMS_THUMB_EXTENT is "conus"
        // the radar tile always shows composite reflectivity (the first product); the other one follows the viewer
        const auto wanted = token == "MRMS_RADAR" ? UtilityMrms::products().front().id : Utility::readPref("MRMS_LAST_PRODUCT", UtilityMrms::products().front().id);
        auto product = UtilityMrms::products().front();
        auto known = std::find_if(UtilityMrms::products().begin(), UtilityMrms::products().end(), [&wanted] (const auto& p) { return p.id == wanted; });
        if (known != UtilityMrms::products().end()) {
            product = *known;
        } else {
            vector<UtilityMrms::Product> more;
            string ignored;
            if (UtilityMrms::discoverMore(more, ignored)) {
                const auto other = std::find_if(more.begin(), more.end(), [&wanted] (const auto& p) { return p.id == wanted; });
                if (other != more.end()) {
                    product = *other;
                }
            }
        }
        const auto here = Location::getLatLonCurrent();
        const bool conus = Utility::readPref("MRMS_THUMB_EXTENT", "regional") == "conus";
        QByteArray png;
        string error;
        const bool ok = conus
            ? UtilityMrms::thumbnail(product, 23.0, 50.0, -127.0, -65.0, 900, png, error)
            : UtilityMrms::thumbnail(product, here.lat() - 3.8, here.lat() + 3.8, here.lon() - 5.5, here.lon() + 5.5, 900, png, error);
        return ok ? png : QByteArray{};
    }
    if (token == "GRIB_LATEST") {
        // the model screen's last model, chart and area, drawn from the newest run (the model's first forecast hour after the start)
        auto model = Utility::readPref("NCEP", "RRFS");
        if (!GfsModels::draws(model) || GfsModels::find(model)->storm) {
            model = "RRFS";
        }
        const auto * def = GfsModels::find(model);
        const auto hours = def->hours.empty() ? GfsModels::Hours{} : def->hours.front();
        const int hour = hours.from + (hours.from + hours.step <= hours.to ? hours.step : 0);
        GfsRender::Session session;
        string error;
        const auto png = GfsRender::png(session, model, Utility::readPref("MODELNCEPPARAMLASTUSED", "500_wnd_ht"), Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS"), "", hour, {}, error);
        return png;
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
    } else if (token == "TROPICAL_ATL_SAT") {
        url = UtilityGoes::getImage("GEOCOLOR", "taw");
    } else if (token == "TROPICAL_EPAC_SAT") {
        url = UtilityGoes::getImage("GEOCOLOR", "eep");
    } else if (token == "TROPICAL_WPAC_SAT") {
        url = "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMCOL.JPG";
    } else if (token == "NHC_ATLANTIC") {
        url = GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_2d0.png";
    } else {
        url = DownloadImage::byProduct(token);
    }
    return url.empty() ? QByteArray{} : UtilityIO::downloadAsByteArray(url);
}
