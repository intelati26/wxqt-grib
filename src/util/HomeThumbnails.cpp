// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/HomeThumbnails.h"
#include "misc/FeedViewer.h"
#include "misc/NiceWeatherCache.h"
#include <QBuffer>
#include "space/UtilitySun.h"
#include <algorithm>
#include <map>
#include <QFile>
#include <QImage>
#include "common/GlobalVariables.h"
#include "misc/UtilityOpcImages.h"
#include "mrms/UtilityMrms.h"
#include "gfs/GfsModels.h"
#include "gfs/GfsChart.h"
#include "gfs/GfsRender.h"
#include "gfs/WpcPppLoader.h"
#include "mapkit/GlobeMapView.h"
#include "mapkit/WorldRender.h"
#include <QBuffer>
#include <QPainter>
#include "spc/UtilitySpcCompmap.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "spc/UtilitySpcSwo.h"
#include "util/DownloadImage.h"
#include "util/UtilityIO.h"
#include "vis/UtilityGoes.h"
#include "vis/UtilityGoesFullDisk.h"
#include "wpc/UtilityWpcRainfallOutlook.h"
#include <atomic>
#include <thread>
#include <QDir>
#include <QFileInfo>
#include "settings/Location.h"
#include "util/Utility.h"
#include "gfs/GfsCache.h"
#include "gfs/AirNowLoader.h"
#include "mapkit/AirNowLayer.h"
#include "radar/RadarGeomInfo.h"
#include <numbers>

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
        {"RTMA_AREA", "RTMA temperature and wind - your last model area", "grib.png", true, false},
        {"RTMA_SECOND", "RTMA second picture - your last model area (the product is set here or by its button)", "grib.png", true, false},
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
        {"WORLD_SST", "World sea surface temperature (NOAA/NESDIS daily 5 km blend)", "opc.png#2", false, false},
        {"SUN_REGIONS", "The Sun with its numbered active regions and a heliographic grid (SDO/HMI, NOAA SWPC)", "goes16.png", false, false},
        {"AIRNOW", "Air quality - EPA AirNow, today (the cities, in the EPA's colors)", "", true, false},
        {"MRMS_RADAR", "MRMS radar - composite reflectivity around your location", "mcd_tile.png", false, false},
        {"MRMS_LATEST", "MRMS - latest scan of your last product, around your location", "mcd_tile.png", false, false},
        {"CFS_30DAY", "CFSv2 next 30 days against the normal: temperature (colors) and precipitation (lines), CONUS", "baseline_show_chart_black_48dp.png#2", false, false},
        {"WPC_PPP_24", "WPC median rain, snow and ice - next 24 hours", "baseline_show_chart_black_48dp.png#2", false, false},
        {"WPC_PPP_36", "WPC median rain, snow and ice - next 36 hours", "baseline_show_chart_black_48dp.png#2", false, false},
        {"WPC_PPP_48", "WPC median rain, snow and ice - next 48 hours", "baseline_show_chart_black_48dp.png#2", false, false},
        {"WPC_PPP_72", "WPC median rain, snow and ice - next 72 hours", "baseline_show_chart_black_48dp.png#2", false, false},
        {"NICE_DAY1", "Nice weather outlook, day 1 (from the National Blend of Models: a work in progress)", "grib.png", true, false},
        {"GRIB_LATEST", "Model viewer - your last model, chart and area, newest run", "grib.png", true, false},
        {"VORTEX_10MB", "Polar vortex - 10 mb wind and height, north pole (GFS)", "", true, false},
        {"VORTEX_THETA", "Polar vortex - isentropic potential vorticity, 850 K, north pole (GFS)", "", true, false},
        {"IEM_FEATURE", "Chart of the day (Iowa Environmental Mesonet's daily feature: someone else's interesting data, credited and linked)", "", false, false},
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
        {"RTMA_AREA", {"Temperature now: your area", "Real-Time Mesoscale Analysis: the newest 2 m temperature and 10 m wind over the area you last used in the Model Viewer (a saved place), or the United States. Click for the Model Viewer on this chart."}},
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
        {"SUN_REGIONS", {"The Sun", "The visible disc of the Sun (SDO/HMI) with the numbered active regions (NOAA SWPC), coloured by their chance of an M or X flare, and a grid every 30 degrees: east is left, west is right, and regions rotate in at the left and out at the right in about two weeks. Click for the Space weather screen."}},
        {"WORLD_SST", {"World sea surface temperature", "Daily global sea surface temperature, a 5 km blend of satellite and in-situ measurements (NOAA/NESDIS). Click for the Climate and ocean screen, with the anomaly and the regional maps."}},
        {"AIRNOW", {"Air quality today", "EPA AirNow: the air quality of the cities across the United States today (observations and forecast, the worse of them), in the EPA's colors from Good green to Hazardous maroon. Click for the master map with the days ahead."}},
        {"MRMS_RADAR", {"MRMS radar", "Multi-Radar Multi-Sensor composite reflectivity around your location."}},
        {"MRMS_LATEST", {"MRMS product", "Latest scan of the MRMS product you looked at last, around your location."}},
        {"CFS_30DAY", {"Next 30 days", "NOAA's CFSv2 for the next 30 days against its own normal: temperature as colors (warm or cool) and precipitation as lines (green wetter, brown drier), the mean of its four newest runs. A lean of the pattern, not a forecast of days. Click for the Model Viewer."}},
        {"WPC_PPP_24", {"WPC 24 hr", "The Weather Prediction Center's probabilistic precipitation portal: the median (50th percentile) amount of rain, snow and ice in the first 24 hour period of the newest run, rain in greens, snow in blues, freezing rain in pinks. Click for the Model Viewer on this chart, with the percentiles and the chances of an amount."}},
        {"WPC_PPP_36", {"WPC 36 hr", "The Weather Prediction Center's probabilistic precipitation portal: the median (50th percentile) amount of rain, snow and ice in the first 36 hour period of the newest run. Click for the Model Viewer on this chart."}},
        {"WPC_PPP_48", {"WPC 48 hr", "The Weather Prediction Center's probabilistic precipitation portal: the median (50th percentile) amount of rain, snow and ice in the first 48 hour period of the newest run. Click for the Model Viewer on this chart."}},
        {"WPC_PPP_72", {"WPC 72 hr", "The Weather Prediction Center's probabilistic precipitation portal: the median (50th percentile) amount of rain, snow and ice in the 72 hour period of the newest run. Click for the Model Viewer on this chart."}},
        {"NICE_DAY1", {"Nice weather: day 1", "Where the daytime weather is best for being outside today or tomorrow in five levels (Decent, Fair, Nice, Beautiful, Perfect), made from the National Blend of Models' temperature, dewpoint, cloud, gusts and rain chance. A work in progress. Click for the days 1 to 7."}},
        {"GRIB_LATEST", {"Model chart", "The model, chart and area you last looked at in the Model Viewer, from the newest run."}},
        {"VORTEX_10MB", {"Polar vortex, 10 mb", "The stratospheric polar vortex as the GFS has it now: the wind speed and height at 10 mb (about 30 km) from the pole to 30 N on a polar map, the newest analysis. A click opens the master map with the same chart; its Export has the history of the vortex through the season."}},
        {"IEM_FEATURE", {"Chart of the day", "The Iowa Environmental Mesonet's daily feature (Iowa State University; Daryl Herzmann): a chart of the day with a note about it, as they publish it. A click opens the feed: the notes of the last weeks, with links to their site, to the tool that makes the same chart and to their \"Good chart\" survey (rate the feature Good, Bad or Abstain on their site)."}},
        {"VORTEX_THETA", {"Polar vortex, 850 K", "The polar vortex on the 850 K isentropic surface (near 10 mb): potential vorticity, wind and pressure as the GFS has it now, from the pole to 30 N on a polar map. The air does not cross an isentropic surface, so the vortex is its edge. A click opens the master map with the same chart."}},
        };
        return table;
    }
}

const vector<HomeThumbnails::RtmaValue>& HomeThumbnails::rtmaValues() {
    static const vector<RtmaValue> values{
        {"2m_dewp_10m_wnd", "Dew point"},
        {"10m_wnd_gust", "Wind gust"},
        {"visibility", "Visibility"},
        {"total_cloud_cover", "Cloud cover"},
        {"2m_temp_10m_wnd", "Temperature"},
    };
    return values;
}

string HomeThumbnails::rtmaSecondChart() {
    const auto chosen = Utility::readPref("RTMA_SECOND_CHART", rtmaValues().front().chart);
    for (const auto& v : rtmaValues()) {
        if (v.chart == chosen) {
            return chosen;
        }
    }
    return rtmaValues().front().chart;
}

string HomeThumbnails::caption(const string& token) {
    if (token == "RTMA_SECOND") {
        for (const auto& v : rtmaValues()) {
            if (v.chart == rtmaSecondChart()) {
                return v.label + " now";
            }
        }
    }
    const auto found = words().find(token);
    return found == words().end() ? string{} : found->second.caption;
}

string HomeThumbnails::tip(const string& token) {
    if (token == "RTMA_SECOND") {
        return "Real-Time Mesoscale Analysis: the newest analysis of the product chosen with the button under this picture or in Settings, over the area you last used in the Model Viewer (a saved place), or the United States. Click for the Model Viewer on this chart.";
    }
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

namespace {
    // a picture that really is one (an error page, an empty answer or a stub is not drawn as a tile)
    bool decodes(const QByteArray& bytes) {
        QImage image;
        return !bytes.isEmpty() && image.loadFromData(bytes) && image.width() >= 32 && image.height() >= 32;
    }
}

namespace {
    // EPA's AirNow today over the contiguous United States: the cities as dots in the EPA's colors on a white map with the state lines (the master map's layer, as a picture to keep), with a line of title
    QByteArray airNowPicture() {
        const auto today = QDate::currentDate();
        const auto points = AirNow::points(today);
        if (points.empty()) {
            return {};
        }
        constexpr double south = 23.0, north = 50.0, west = -127.0, east = -65.0;
        constexpr int width = 900;
        const auto mercator = [] (double lat) { return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0)); };
        const double top = mercator(north), bottom = mercator(south);
        const int height = static_cast<int>(std::lround(width * (top - bottom) / (east - west)));
        QImage map{width, height + 30, QImage::Format_ARGB32};
        map.fill(Qt::white);
        QPainter p{&map};
        p.setRenderHint(QPainter::Antialiasing, true);
        const auto at = [&] (double lat, double lon) { return QPointF{(lon - west) / (east - west) * width, 30 + (top - mercator(lat)) / (top - bottom) * height}; };
        {
            static std::map<RadarGeometryTypeEnum, vector<float>> lines;   // (the line files: latitude and west-positive longitude, as pairs of end points)
            auto& data = lines[StateLines];
            if (data.empty()) {
                RadarGeomInfo::loadData(RadarGeomInfo::typeToFileName.at(StateLines), data);
            }
            p.setPen(QPen{QColor{110, 110, 110}, 0.9});
            for (size_t i = 0; i + 3 < data.size(); i += 4) {
                p.drawLine(at(data[i], -data[i + 1]), at(data[i + 2], -data[i + 3]));
            }
        }
        p.setPen(QPen{QColor{30, 30, 30, 200}, 0.7});
        for (int category = 0; category < 6; category++) {   // the worse on top
            p.setBrush(AirNowLayer::color(category));
            const double radius = 3.2 + 0.5 * category;
            for (const auto& point : points) {
                if (point.category == category && point.lat >= south && point.lat <= north && point.lon >= west && point.lon <= east) {
                    p.drawEllipse(at(point.lat, point.lon), radius, radius);
                }
            }
        }
        QFont bold = p.font();
        bold.setBold(true);
        bold.setPixelSize(15);
        p.setFont(bold);
        p.setPen(Qt::black);
        p.drawText(QRect{6, 3, width - 12, 24}, Qt::AlignLeft | Qt::AlignVCenter, "Air quality: EPA AirNow, " + today.toString("ddd MMM d"));
        p.end();
        QByteArray bytes;
        QBuffer buffer{&bytes};
        buffer.open(QIODevice::WriteOnly);
        map.save(&buffer, "PNG");
        return bytes;
    }

    // The north polar view of a chart of the GFS, from the pole to 30 N, painted from the grids themselves (the master map's polar export, WorldRender), the newest analysis (the first hour of the newest
    // run at a six hourly time), with a line of title above it. Empty when the chart could not be read.
    QByteArray polarPicture(const std::string& chart, const QString& title) {
        auto when = QDateTime::currentDateTimeUtc().addSecs(-6 * 3600);   // (a time the newest run has an initial state for)
        when.setTime(QTime{when.time().hour() / 6 * 6, 0});
        const auto sources = WorldRender::loadSources(chart, {"GFS"}, when);
        if (sources.empty()) {
            return {};
        }
        const auto world = WorldRender::renderWorld(sources, static_cast<int>(GlobeMapView::Projection::PolarNorth), 0.0, 640, nullptr, true);
        if (world.isNull()) {
            return {};
        }
        QImage out{world.width(), world.height() + 38, QImage::Format_ARGB32};
        out.fill(Qt::white);
        QPainter p{&out};
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont bold = p.font();
        bold.setBold(true);
        bold.setPixelSize(15);
        p.setFont(bold);
        p.drawText(QRect{6, 3, out.width() - 12, 18}, Qt::AlignLeft | Qt::AlignVCenter, title);
        QFont plain = p.font();
        plain.setBold(false);
        plain.setPixelSize(12);
        p.setFont(plain);
        p.drawText(QRect{6, 21, out.width() - 12, 15}, Qt::AlignLeft | Qt::AlignVCenter, QString::fromStdString("GFS run " + sources.front().run + ", f" + std::to_string(sources.front().hour)) + "   valid " + when.toString("ddd d MMM HH") + "Z   (pole to 30 N)");
        p.drawImage(0, 38, world);
        p.end();
        QByteArray bytes;
        QBuffer buffer{&bytes};
        buffer.open(QIODevice::WriteOnly);
        out.save(&buffer, "PNG");
        return bytes;
    }
}

QByteArray HomeThumbnails::fetch(const string& token) {
    if (token == "IEM_FEATURE") {   // the newest item of the feed (kept 30 minutes) and its picture
        FeedViewer::Item item;
        QByteArray image;
        return FeedViewer::newest(FeedViewer::iem(), item, image) ? image : QByteArray{};
    }
    if (token == "AIRNOW") {
        return airNowPicture();
    }
    if (token == "VORTEX_10MB") {
        return polarPicture("10_wnd_ht", "Polar vortex: 10 mb wind and height");
    }
    if (token == "VORTEX_THETA") {
        return polarPicture("850K_pv", "Polar vortex: 850 K isentropic potential vorticity");
    }
    if (token == "MRMS_LATEST" || token == "MRMS_RADAR") {
        // the product last looked at in the MRMS viewer (else composite reflectivity), the newest scan, drawn around the
        // current location - or over all of CONUS when the preference MRMS_THUMB_EXTENT is "conus"
        // the radar tile always shows composite reflectivity (the first product); the other one follows the viewer
        const auto wanted = token == "MRMS_RADAR" ? UtilityMrms::products().front().id : Utility::readPref("MM_MRMS_PRODUCT", UtilityMrms::products().front().id);
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
    if (token == "CFS_30DAY") {
        // the CFSv2's next 30 days against its normal over the United States, drawn by the model screen's own renderer (the model "CFS30"). The picture of the last time is kept: it is what the tile
        // shows at once, and if it is of an earlier day it is drawn again behind it (the first of the day reads some 160 fields, 25 seconds) for the next start.
        const QString file = GfsCache::folder() + "/cfs30/tile.png";
        const auto render = [] {
            GfsRender::Session session;
            string error;
            GfsRender::Variant variant;
            variant.width = 640;   // a tile is small: the chart's text stays readable on a narrower picture
            return GfsRender::png(session, "CFS30", "cfs30_t2m_precip", "CONUS", "", 720, {}, error, nullptr, variant);
        };
        const auto keep = [file] (const QByteArray& png) {
            QDir{}.mkpath(QFileInfo{file}.absolutePath());
            QFile out{file};
            if (!png.isEmpty() && out.open(QIODevice::WriteOnly)) {
                out.write(png);
            }
        };
        QFileInfo info{file};
        QByteArray kept;
        if (info.exists()) {
            QFile in{file};
            if (in.open(QIODevice::ReadOnly)) {
                kept = in.readAll();
            }
        }
        if (decodes(kept)) {
            if (info.lastModified().toUTC().date() != QDateTime::currentDateTimeUtc().date()) {
                static std::atomic<bool> busy{false};
                if (!busy.exchange(true)) {
                    std::thread{[render, keep] {
                        keep(render());
                        busy = false;
                    }}.detach();
                }
            }
            return kept;
        }
        const auto png = render();
        keep(png);
        return png;
    }
    if (token.compare(0, 8, "WPC_PPP_") == 0) {
        // the portal's median chart of the period, from the newest run: the first period that lies wholly after the run's time (else the first the run has)
        const string hoursText = token.substr(8);
        string cycle, date;
        if (!GfsRender::latestCycle("WPC-PPP", cycle, "", &date)) {
            return {};
        }
        const GfsData::Run run{date, cycle.substr(0, 2)};   // ("06Z")
        const auto have = WpcPppLoader::hours(run, hoursText + "|qpf|p50");
        if (have.empty()) {
            return {};
        }
        int hour = have.front();
        for (const int h : have) {
            if (h >= std::atoi(hoursText.c_str())) {
                hour = h;
                break;
            }
        }
        GfsRender::Session session;
        string error;
        GfsRender::Variant variant;
        variant.width = 640;
        return GfsRender::png(session, "WPC-PPP", "ppp_" + hoursText + "h_median", "CONUS", run.id(), hour, {}, error, nullptr, variant);
    }
    if (token == "NICE_DAY1") {   // the day the Nice weather pages show (made once, by whichever asks first; the picture is scaled to the tile)
        string error;
        const auto day = NiceWeatherCache::make(0, error);
        return day ? day->png : QByteArray{};
    }
    if (token == "RTMA_AREA") {
        // the model screen's last area (RTMA covers the United States only: an area it cannot draw is drawn as CONUS, the NWS areas and saved places as they are), from the newest analysis
        const auto sector = GfsChart::areaFor("RTMA", Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS"));
        GfsRender::Session session;
        string error;
        return GfsRender::png(session, "RTMA", "2m_temp_10m_wnd", sector, "", 0, {}, error);
    }
    if (token == "RTMA_SECOND") {
        const auto sector = GfsChart::areaFor("RTMA", Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS"));
        GfsRender::Session session;
        string error;
        return GfsRender::png(session, "RTMA", rtmaSecondChart(), sector, "", 0, {}, error);
    }
    if (token == "GRIB_LATEST") {
        // the model screen's last model, chart and area, drawn from the newest run (the model's first forecast hour after the start)
        auto model = Utility::readPref("NCEP", "RRFS");
        auto param = Utility::readPref("MODELNCEPPARAMLASTUSED", "500_wnd_ht");
        auto sector = Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS");
        if (!GfsModels::isForecast(model)) {   // an analysis (RTMA, URMA) or the PPP is on the model screen: the last model that looks ahead, with its chart and area
            const auto last = Utility::readPref("MODEL_LAST_FORECAST", "");
            if (GfsModels::isForecast(last)) {
                model = last;
                param = Utility::readPref("MODEL_LAST_FORECAST_PARAM", param);
                sector = Utility::readPref("MODEL_LAST_FORECAST_SECTOR", sector);
            }
        }
        if (!GfsModels::isForecast(model) || GfsModels::find(model)->storm) {   // (none looked at yet: the RRFS and its first chart)
            model = "RRFS";
            param = "500_wnd_ht";
        }
        const auto * def = GfsModels::find(model);
        const auto hours = def->hours.empty() ? GfsModels::Hours{} : def->hours.front();
        const int hour = hours.from + (hours.from + hours.step <= hours.to ? hours.step : 0);
        GfsRender::Session session;
        string error;
        const auto png = GfsRender::png(session, model, param, sector, "", hour, {}, error);
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
        // the product last chosen on the Global GOES screen; when its server does not answer with a picture, the GOES full disk of NESDIS's own site
        std::vector<string> candidates;
        const auto chosen = static_cast<size_t>(std::max(0, Utility::readPrefInt("GOESFULLDISK_IMG_FAV_URL", 0)));
        if (chosen < UtilityGoesFullDisk::urls.size()) {
            candidates.push_back(UtilityGoesFullDisk::urls[chosen]);
        }
        candidates.push_back("https://cdn.star.nesdis.noaa.gov/GOES19/ABI/FD/GEOCOLOR/678x678.jpg");
        candidates.push_back("https://cdn.star.nesdis.noaa.gov/GOES18/ABI/FD/GEOCOLOR/678x678.jpg");
        for (const auto& candidate : candidates) {
            const auto bytes = UtilityIO::downloadAsByteArray(candidate);
            if (decodes(bytes)) {
                return bytes;
            }
        }
        return {};
    } else if (token == "LIGHTNING") {
        url = UtilityGoes::getImage("GLM", "CONUS");
    } else if (token == "TROPICAL_ATL_SAT") {
        url = UtilityGoes::getImage("GEOCOLOR", "taw");
    } else if (token == "TROPICAL_EPAC_SAT") {
        url = UtilityGoes::getImage("GEOCOLOR", "eep");
    } else if (token == "TROPICAL_WPAC_SAT") {
        url = "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMCOL.JPG";
    } else if (token == "SUN_REGIONS") {
        const auto picture = UtilitySun::labelled(800);
        if (picture.isNull()) {
            return {};
        }
        QByteArray png;
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        picture.save(&buffer, "PNG");
        return png;
    } else if (token == "WORLD_SST") {
        url = "https://www.ospo.noaa.gov/data/cb/sst/sst.daily.current.png";
    } else if (token == "NHC_ATLANTIC") {
        url = GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_2d0.png";
    } else {
        url = DownloadImage::byProduct(token);
    }
    if (url.empty()) {
        return {};
    }
    const auto bytes = UtilityIO::downloadAsByteArray(url);
    return decodes(bytes) ? bytes : QByteArray{};
}
