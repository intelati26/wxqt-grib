// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsRender.h"
#include "models/PaletteStore.h"
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <QCoreApplication>
#include <QProcess>
#include <QTimer>
#include <QBuffer>
#include <QDateTime>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStandardPaths>
#include "common/GlobalVariables.h"
#include "gfs/GfsCache.h"
#include "gfs/GfsChart.h"
#include "gfs/GfsClimate.h"
#include "gfs/GfsData.h"
#include "gfs/GfsModels.h"
#include "hurricane/Coast.h"
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "radar/CitiesExtended.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    // a model that follows a storm: the hurricane model
    bool isHafs(const std::string& model) {
        const auto * def = GfsModels::find(model);
        return def != nullptr && def->storm;
    }

    GfsData::Source sourceOf(const std::string& model, const std::string& storm = "") {
        const auto * def = GfsModels::find(model);
        return def ? def->source(storm) : GfsData::Source{};   // (every caller has found the model before this)
    }

    GfsData data(const QString& folder, const std::string& model, const std::string& storm = "") {
        GfsData::Config config;
        config.gdalBin = UtilityGrib::gdalBinDir();
        config.cacheFolder = folder;
        config.bytes = [] (const std::string& url, long long start, long long end) {
            return end < 0 && start == 0 ? URL::getBytesManaged(url) : URL::getBytesRangeManaged(url, start, end < 0 ? start + 4 * 1024 * 1024 * 1024LL : end);
        };
        return GfsData{config, sourceOf(model, storm)};
    }

    // the newest published run of a model, looked up at most every ten minutes
    bool newestRun(const GfsData& gfs, GfsData::Run& run) {
        static std::mutex mutex;
        static std::map<std::string, std::pair<GfsData::Run, qint64>> known;
        std::lock_guard lock{mutex};
        auto& entry = known[gfs.model().id];
        const auto now = QDateTime::currentSecsSinceEpoch();
        if (entry.first.date.empty() || now - entry.second > 600) {
            GfsData::Run found;
            if (gfs.latestRun(found)) {
                entry = {found, now};
            }
        }
        run = entry.first;
        return !run.date.empty();
    }
}

GfsRender::Session::Session() {
    // (the cache is tidied by startCacheCleanup, in the background: a screen never waits for it)
    path = GfsCache::folder();   // the same folder for every screen: a field one of them fetched is there for the others, and for the next run of the program
}

GfsRender::Session::~Session() = default;   // nothing to remove: the cache outlives the screen

QString GfsRender::Session::folder() const {
    return path;
}

QString GfsRender::Session::partialGrib(const std::string&, int) const {
    return {};   // the file of what was downloaded for a run and hour is made by GfsData::partialGrib, from the messages in the cache
}

void GfsRender::startCacheCleanup() {
    static std::atomic<bool> running{false};
    auto * timer = new QTimer{QCoreApplication::instance()};
    const auto sweep = [] {
        if (running.exchange(true)) {
            return;   // one at a time
        }
        const int hours = Utility::readPrefInt("MODEL_CACHE_HOURS", GfsCache::defaultHours);
        const auto bytes = static_cast<qint64>(Utility::readPrefInt("MODEL_CACHE_MB", GfsCache::defaultMegabytes)) * 1024 * 1024;
        std::thread{[hours, bytes] {
            GfsCache::prune(hours, bytes);
            running = false;
        }}.detach();
    };
    QObject::connect(timer, &QTimer::timeout, timer, [timer, sweep] {
        sweep();
        timer->start(6 * 3600 * 1000);   // and again every 6 hours while the program is open
    });
    timer->setSingleShot(true);
    timer->start(8000);   // after the program is up and the first screen has drawn
}

qint64 GfsRender::cacheUsage() {
    return GfsCache::usage();
}

void GfsRender::clearCache() {
    GfsCache::clear();
}

GfsRender::SourceCheck GfsRender::checkSource(Session& session, const std::string& model) {
    SourceCheck result;
    result.model = model;
    const auto started = std::chrono::steady_clock::now();
    const auto finish = [&] {
        result.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
        return result;
    };
    const auto * def = GfsModels::find(model);
    if (def == nullptr) {
        result.detail = "not a model of this build";
        return finish();
    }
    result.label = def->label;
    if (def->storm) {
        result.skipped = true;
        result.detail = "needs an active storm to look at";
        return finish();
    }
    if (UtilityGrib::gdalBinDir().empty()) {
        result.detail = "GDAL not found";
        return finish();
    }
    const auto gfs = data(session.folder(), model);
    if (gfs.model().netcdf) {   // its fields are read through GDAL's NetCDF driver, which a trimmed GDAL (the Windows package's) may not have
        static const bool hasDriver = [] {
            QProcess formats;
            formats.start(QString::fromStdString(UtilityGrib::gdalBinDir()) + "/gdalinfo", {"--formats"});
            formats.waitForFinished(15000);
            return QString::fromUtf8(formats.readAllStandardOutput()).contains("netCDF", Qt::CaseInsensitive);
        }();
        if (!hasDriver) {
            result.detail = "the GDAL in use has no NetCDF driver, so this model's fields cannot be read";
            return finish();
        }
    }
    GfsData::Run run;
    if (!gfs.latestRun(run)) {
        result.detail = "no run found on the server (offline, or the files moved)";
        return finish();
    }
    result.run = run.id();
    const GfsChart::Product * product = nullptr;
    for (const auto& p : GfsChart::products()) {
        if (p.source == model) {
            product = &p;
            break;
        }
    }
    if (product == nullptr) {
        result.detail = "run " + run.id() + " found; the model has no charts";
        return finish();
    }
    int hour = def->hours.empty() ? 0 : def->hours.front().from;
    if (hour == 0 && !GfsChart::needs(*product, 6).empty() && def->hours.size() && def->hours.front().to >= 6) {
        hour = def->hours.front().step > 6 ? def->hours.front().step : 6;   // hour 0 has no accumulations: a chart is checked at a later hour
    }
    std::string detail;
    result.ok = gfs.check(run, GfsChart::needs(*product, hour), detail);
    result.detail = "run " + run.id() + ", " + product->id + " f" + std::to_string(hour) + ": " + detail;
    return finish();
}

bool GfsRender::drawsModel(const std::string& model) {
    return GfsModels::draws(model);
}

bool GfsRender::handles(const std::string& model, const std::string& param) {
    return GfsModels::draws(model) && GfsChart::product(param, model) != nullptr;
}

bool GfsRender::latestCycle(const std::string& model, std::string& cycle, const std::string& storm, std::string * date) {
    const auto gfs = data({}, model, storm);
    GfsData::Run run;
    if (!newestRun(gfs, run)) {
        return false;
    }
    cycle = run.cycle + "Z";
    if (date != nullptr) {
        *date = run.date;
    }
    return true;
}

namespace {
    // The first set of lines of the chart from each of `others`, for the same valid time as `run` + `hour`, drawn on `chart` over the screen's own lines: one color for each model and no fill. The
    // others' runs are their newest one at or before this run's time that they make (a model that runs every 12 hours uses its 12Z run for an 18Z one), the hour moved to keep the valid time.
    bool addOverlays(GfsRender::Session& session, GfsChart::Product& chart, GfsChart::Grids& grids, const std::string& model, const GfsData::Run& run, int hour, const std::vector<std::string>& others, std::string& error) {
        if (chart.contours.empty()) {
            error = "this chart has no lines (heights, pressure) to put the other models' over; pick a chart with lines";
            return false;
        }
        static const char * const colors[] = {"#111111", "#d62728", "#1f77b4", "#2ca02c", "#9467bd"};
        auto start = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
        start.setTimeSpec(Qt::UTC);
        const auto valid = start.addSecs(static_cast<qint64>(hour) * 3600);
        static const GfsClimate climate{UtilityGrib::gdalBinDir()};
        auto first = chart.contours.front();
        chart.contours.clear();
        first.color = QColor{colors[0]};
        first.title = model + ": " + first.title;
        first.width = std::max(first.width, 1.6);
        chart.contours.push_back(first);
        std::string names = model;
        int index = 0;
        for (const auto& other : others) {
            index++;
            if (other == model || index >= 5) {
                continue;
            }
            const auto * product = GfsChart::product(chart.id, other);
            if (product == nullptr) {
                error = other + " has no chart " + chart.id;
                return false;
            }
            auto gfs = data(session.folder(), other);
            GfsData::Run newest;
            if (!newestRun(gfs, newest)) {
                error = "could not find a " + other + " run";
                return false;
            }
            auto newestTime = QDateTime::fromString(QString::fromStdString(newest.id()), "yyyyMMddHH");
            newestTime.setTimeSpec(Qt::UTC);
            auto t = std::min(start, newestTime);
            const int step = gfs.model().cycleHours;
            while (t.time().hour() % step != 0) {
                t = t.addSecs(-3600);
            }
            const auto * def = GfsModels::find(other);
            GfsChart::Grids loaded;
            GfsData::Run used;
            int usedHour = 0;
            std::string why;
            bool ok = false;
            for (int back = 0; back < 3 && !ok; back++, t = t.addSecs(-static_cast<qint64>(step) * 3600)) {
                const int h = static_cast<int>((valid.toSecsSinceEpoch() - t.toSecsSinceEpoch()) / 3600);
                bool offered = def == nullptr;
                for (const auto& range : def ? def->hours : std::vector<GfsModels::Hours>{}) {
                    offered = offered || (h >= range.from && h <= range.to && (h - range.from) % range.step == 0);
                }
                if (!offered) {
                    why = other + " has no forecast for " + valid.toString("ddd HH").toStdString() + "Z from its " + t.toString("HH").toStdString() + "Z run (hour " + std::to_string(h) + ")";
                    continue;
                }
                used = {t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
                usedHour = h;
                loaded.clear();
                ok = gfs.load(used, GfsChart::needs(*product, h), loaded, why);
            }
            if (!ok) {
                error = why.empty() ? other + " could not be read" : why;
                return false;
            }
            if (product->derive) {
                product->derive(loaded, GfsChart::Context{usedHour, used, &climate});
            }
            if (product->contours.empty() || loaded.find(product->contours.front().key) == loaded.end()) {
                error = other + "'s chart has no lines to compare";
                return false;
            }
            auto lines = product->contours.front();
            const std::string key = "overlay" + std::to_string(index) + "_" + lines.key;
            grids[key] = loaded.at(lines.key);
            lines.key = key;
            lines.color = QColor{colors[std::min(index, 4)]};
            lines.title = other + " (" + used.cycle + "Z): " + product->contours.front().title;
            lines.labelled = false;
            lines.width = std::max(lines.width, 1.6);
            lines.highsAndLows = false;
            chart.contours.push_back(lines);
            names += ", " + other;
        }
        chart.fill = nullptr;
        chart.fillTitle.clear();
        chart.fillTitleFor = nullptr;
        chart.ramp.stops.clear();
        chart.overlays.clear();
        chart.barbU.clear();
        chart.barbV.clear();
        chart.streamU.clear();
        chart.streamV.clear();
        chart.pointValues.clear();
        chart.linesOnly = true;
        chart.label = "Models compared: " + names + ", " + chart.label;
        return true;
    }
}

QByteArray GfsRender::png(Session& session, const std::string& model, const std::string& param, const std::string& sectorId, const std::string& cycle, int hour, const std::vector<std::string>& overlays, std::string& error, GfsChart::Probe * probe, const Variant& variant) {
    try {   // a chart that cannot be made yet (the standard deviations still being worked out) says why
    const auto * base = GfsChart::product(param, model);
    auto sector = GfsChart::sector(sectorId);
    GfsChart::Sector storm;   // the hurricane model's grid follows the storm: the chart is its whole grid
    const std::string stormId = isHafs(model) ? sectorId : std::string{};
    if (isHafs(model)) {
        sector = &storm;
    }
    GfsChart::Sector local;   // an area round a saved location: its kilometers, placed inside what the model covers
    double localLat = 0, localLon = 0, localKm = 0;
    std::string localName;
    if (!isHafs(model) && GfsChart::parseLocalId(sectorId, localLat, localLon, localKm, localName)) {
        double west = -180.0, south = -85.0, east = 180.0, north = 85.0;
        if (const auto * def = GfsModels::find(model)) {
            const auto source = def->source("");
            if (source.warp.enabled) {
                west = source.warp.west;
                south = source.warp.south;
                east = source.warp.east;
                north = source.warp.north;
            }
        }
        if (localLon > 180.0) {
            localLon -= 360.0;
        }
        local = GfsChart::localSector(sectorId, west, south, east, north);
        sector = &local;
    }
    auto composed = base ? GfsChart::compose(*base, overlays) : GfsChart::Product{};   // the chart with what was ticked onto it
    if (base && !variant.member.empty()) {
        composed = GfsChart::withMember(composed, variant.member);
    }
    if (base && variant.standardDeviations) {
        composed = GfsChart::withStandardDeviations(composed);
    }
    const auto * product = base ? &composed : nullptr;
    if (!product || !sector) {
        error = "no " + model + " chart for " + param + " " + sectorId;
        return {};
    }
    if (UtilityGrib::gdalBinDir().empty()) {
        error = "GDAL not found - install the 'gdal' package";
        return {};
    }
    auto gfs = data(session.folder(), model, stormId);
    if (!isHafs(model) && sector->east > sector->west && sector->west >= -180.0 && sector->east <= 180.0 && Utility::readPref("MODEL_BOX", "true").compare(0, 1, "t") == 0) {
        // a region: only its box (a few more degrees round it for the lines and the barbs at its edge) of each field is fetched, when the model's server can cut one; whole numbers, so that the
        // views of nearby regions share what was fetched
        GfsData::Box box{std::max(-180.0, std::floor(sector->west) - 3.0), std::max(-90.0, std::floor(sector->south) - 3.0), std::min(180.0, std::ceil(sector->east) + 3.0), std::min(90.0, std::ceil(sector->north) + 3.0)};
        if (box.east - box.west <= 130.0 && box.north - box.south <= 75.0) {
            gfs.setBox(box);
        }
    }
    // the newest published run; an earlier cycle of the screen's choice is that run stepped back to it
    GfsData::Run run;
    if (!newestRun(gfs, run)) {
        error = "could not find a " + model + " run on NOAA's open data";
        return {};
    }
    const bool exact = cycle.size() == 10 && std::all_of(cycle.begin(), cycle.end(), [] (char c) { return std::isdigit(static_cast<unsigned char>(c)); });   // yyyyMMddHH: that very run
    if (exact) {
        run = {cycle.substr(0, 8), cycle.substr(8, 2)};
    }
    const auto wanted = cycle.size() >= 2 ? cycle.substr(0, 2) : run.cycle;
    if (!exact && wanted != run.cycle && wanted.size() == 2 && std::isdigit(static_cast<unsigned char>(wanted[0]))) {
        auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
        t.setTimeSpec(Qt::UTC);
        const int step = gfs.model().cycleHours;
        for (int back = 0; back < 24 / step && t.toString("HH").toStdString() != wanted; back++) {
            t = t.addSecs(-static_cast<qint64>(step) * 3600);
        }
        run = {t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
    }
    GfsChart::Grids grids;
    bool loaded = gfs.load(run, GfsChart::needs(*product, hour), grids, error);
    if (!loaded) {   // the same run's 1 hour pieces, if the product has them (the first 36 hours)
        const auto pieces = GfsChart::fallbackNeeds(*product, hour);
        std::string again;
        grids.clear();
        loaded = !pieces.empty() && gfs.load(run, pieces, grids, again);
        if (loaded) {
            error.clear();
        }
    }
    if (!loaded) {
        // the blend's runs that are not at 00, 06, 12 or 18Z carry only the 1 hour amounts and fewer fields: the newest of the main runs has the rest, with the forecast hour moved to
        // keep the same valid time
        const auto * def = GfsModels::find(model);
        const auto missing = def ? def->onMissing : GfsModels::Missing::Nothing;
        if (missing == GfsModels::Missing::PreviousCycle) {   // the cycle before, six hours further on, has the same valid time (the hurricane model's waves come out after the rest)
            auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(-6 * 3600);
            const GfsData::Run earlier{t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
            grids.clear();
            std::string again;
            if (!gfs.load(earlier, GfsChart::needs(*product, hour + 6), grids, again)) {
                return {};   // the first reason stands
            }
            run = earlier;
            hour += 6;
            error.clear();
        } else if (missing == GfsModels::Missing::PreviousRun) {   // the newest run is still being posted: the runs before it have the same valid time, the hour moved on
            const int step = gfs.model().cycleHours;
            bool found = false;
            for (int back = 1; back <= 3 && !found; back++) {
                auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
                t.setTimeSpec(Qt::UTC);
                t = t.addSecs(-static_cast<qint64>(back) * step * 3600);
                const GfsData::Run earlier{t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
                grids.clear();
                std::string again;
                if (gfs.load(earlier, GfsChart::needs(*product, hour + back * step), grids, again)) {
                    run = earlier;
                    hour += back * step;
                    error.clear();
                    found = true;
                }
            }
            if (!found) {
                return {};   // the first reason stands
            }
        } else if (missing == GfsModels::Missing::MainRun) {   // a run between the main ones has fewer fields: the newest main run has the rest, the hour moved to keep the valid time
            const int cycle = std::atoi(run.cycle.c_str());
            if (cycle % 6 == 0) {
                return {};
            }
            const int earlier = cycle % 6;
            auto alternative = run;
            alternative.cycle = (cycle - earlier < 10 ? "0" : "") + std::to_string(cycle - earlier);
            grids.clear();
            std::string again;
            if (!gfs.load(alternative, GfsChart::needs(*product, hour + earlier), grids, again)) {
                return {};   // the first reason stands
            }
            run = alternative;
            hour += earlier;
            error.clear();
        } else {
            return {};
        }
    }
    if (isHafs(model)) {   // the extent of the grid of the first field
        for (const auto& need : GfsChart::needs(*product, hour)) {
            const auto found = grids.find(need.want.key);
            if (found != grids.end() && !found->second.empty() && need.want.stat != "ww3") {
                const auto& g = found->second;
                storm = GfsChart::gridSector(sectorId, g);
                break;
            }
        }
        if (storm.id.empty() && !grids.empty()) {   // only the wave file: its own grid
            const auto& g = grids.begin()->second;
            storm = GfsChart::gridSector(sectorId, g);
        }
    }
    if (!variant.overlayModels.empty() && variant.kind == Variant::Kind::None && !isHafs(model)) {   // the other models' lines over this one's
        if (!addOverlays(session, composed, grids, model, run, hour, variant.overlayModels, error)) {
            return {};
        }
    }
    GfsChart::Options options;
    {   // latitude / longitude marks: always on an area round a saved location, and on every chart when the settings pref says "all"
        const bool isLocal = sectorId.compare(0, 6, "LOCAL:") == 0;
        const auto mode = Utility::readPref("MODEL_TICKS", "local");
        options.ticks = mode == "all" || (isLocal && mode != "off");
        if (isLocal) {
            options.hasMarker = true;
            options.markerLat = localLat;
            options.markerLon = localLon;
            options.markerName = localName;
        }
    }
    static const GfsClimate climate{UtilityGrib::gdalBinDir()};
    options.climate = &climate;
    options.fahrenheit = UIPreferences::unitsF;
    options.magColors = Utility::readPref("MAG_COLORS", "true").compare(0, 1, "t") == 0;   // the settings switch: the model guidance site's color bands
    options.palettes = PaletteStore::all();   // the user's own scales (the Colors editor)
    options.lines = Coast::borders();
    {   // the cities, biggest first: where a chart's point numbers go (the median / 10th / 90th temperature chart)
        static const auto places = [] {
            CitiesExtended::create();
            auto cities = CitiesExtended::cities;
            std::stable_sort(cities.begin(), cities.end(), [] (const CityExt& a, const CityExt& b) { return a.population > b.population; });
            std::vector<std::pair<float, float>> out;
            for (const auto& city : cities) {
                out.emplace_back(static_cast<float>(city.longitude), static_cast<float>(city.latitude));
            }
            return out;
        }();
        options.places = places;
    }
    options.probe = probe;
    options.windRadii = Utility::readPref("HAFS_RADII", "true").compare(0, 1, "t") == 0;   // the quadrant wind field toggle of the hurricane model screen
    if (isHafs(model)) {   // the model's own track and wind radii for the storm, drawn on the chart
        const auto text = URL::getBytes(gfs.fileUrl(run, 0, "trak"));
        options.track = GfsChart::parseTrack(text.toStdString());
        options.trackName = model == "HAFSB" ? "HAFS-B" : "HAFS-A";
        const std::string other = model == "HAFSB" ? "HAFSA" : "HAFSB";   // the other version, for comparing
        const auto otherText = URL::getBytes(data(session.folder(), other, stormId).fileUrl(run, 0, "trak"));
        options.trackOther = GfsChart::parseTrack(otherText.toStdString());
        options.trackOtherName = other == "HAFSB" ? "HAFS-B" : "HAFS-A";
        if (product->id.compare(0, 5, "swath") == 0) {   // the swaths are of the whole parent domain: only the storm's part is shown
            storm = GfsChart::cropToTrack(storm, options.track, hour, 4.0);
        }
    }
    if (variant.kind != Variant::Kind::None && !isHafs(model)) {
        const auto now = GfsChart::fillOf(*product, grids, run, hour, &climate);
        auto note = std::string{};
        if (variant.kind == Variant::Kind::Max) {
            auto best = now;
            int first = hour, last = hour, used = 1;
            for (const int h : variant.hours) {
                if (h == hour) {
                    continue;
                }
                GfsChart::Grids more;
                std::string again;
                bool ok = gfs.load(run, GfsChart::needs(*product, h), more, again);
                if (!ok) {
                    const auto pieces = GfsChart::fallbackNeeds(*product, h);
                    more.clear();
                    ok = !pieces.empty() && gfs.load(run, pieces, more, again);
                }
                if (!ok) {
                    continue;
                }
                const auto g = GfsChart::fillOf(*product, more, run, h, &climate);
                if (g.values.size() != best.values.size()) {
                    continue;
                }
                for (size_t i = 0; i < best.values.size(); i++) {
                    if (std::isnan(best.values[i]) || (!std::isnan(g.values[i]) && g.values[i] > best.values[i])) {
                        best.values[i] = g.values[i];
                    }
                }
                first = std::min(first, h);
                last = std::max(last, h);
                used++;
            }
            note = "Maximum of " + std::to_string(used) + " hours (" + std::to_string(first) + "-" + std::to_string(last) + ")";
            options.fillOverride = best;
            options.overrideQuantity = product->quantity;
        } else {
            auto t = QDateTime::fromString(QString::fromStdString(run.id()), "yyyyMMddHH");
            t.setTimeSpec(Qt::UTC);
            t = t.addSecs(-static_cast<qint64>(variant.hoursBack) * 3600);
            const GfsData::Run earlier{t.toString("yyyyMMdd").toStdString(), t.toString("HH").toStdString()};
            GfsChart::Grids before;
            std::string again;
            if (!gfs.load(earlier, GfsChart::needs(*product, hour + variant.hoursBack), before, again)) {
                error = "the run " + std::to_string(variant.hoursBack) + " hours earlier is not available for this hour";
                return {};
            }
            const auto then = GfsChart::fillOf(*product, before, earlier, hour + variant.hoursBack, &climate);
            if (then.values.size() != now.values.size() || now.values.empty()) {
                error = "the earlier run is on another grid";
                return {};
            }
            auto delta = now;
            for (size_t i = 0; i < delta.values.size(); i++) {
                delta.values[i] = now.values[i] - then.values[i];   // NaN where either is missing
            }
            std::string unit;
            delta = GfsChart::deltaInUserUnits(delta, product->quantity, options.fahrenheit, unit);
            std::vector<float> magnitudes;
            for (size_t i = 0; i < delta.values.size(); i += 7) {
                if (!std::isnan(delta.values[i])) {
                    magnitudes.push_back(std::abs(delta.values[i]));
                }
            }
            std::sort(magnitudes.begin(), magnitudes.end());
            double m = magnitudes.empty() ? 1.0 : magnitudes[static_cast<size_t>(magnitudes.size() * 0.98)];
            m = m > 100 ? std::round(m / 10) * 10 : m > 10 ? std::round(m) : m > 1 ? std::round(m * 2) / 2 : std::max(0.1, std::round(m * 10) / 10);
            options.overrideRamp = {{{-m, QColor{"#2b5fbf"}}, {-m / 2, QColor{"#8fb6e8"}}, {-m / 8, QColor{"#f1f4f8"}}, {m / 8, QColor{"#f1f4f8"}}, {m / 2, QColor{"#f0a08a"}}, {m, QColor{"#c0392b"}}}};
            options.fillOverride = delta;
            options.overrideQuantity = GfsChart::Quantity::Other;
            note = "Change since the run " + std::to_string(variant.hoursBack) + " h earlier" + (unit.empty() ? "" : " (" + unit + ")");
        }
        options.overrideNote = note;
    }
    const auto image = GfsChart::render(*product, *sector, grids, run, hour, options);
    if (image.isNull()) {
        error = product->derive ? "the chart could not be drawn (some charts need a connection the first time, for the climatology, or the record is not in this run at this hour)" : "the " + model + " chart could not be drawn";
        return {};
    }
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
    } catch (const std::runtime_error& problem) {
        error = problem.what();
        return {};
    }
}

std::vector<std::string> GfsRender::hafsStorms(const std::string& model, std::string& cycle) {
    std::vector<std::string> storms;
    if (!isHafs(model)) {
        return storms;
    }
    const std::string letter = model == "HAFSB" ? "b" : "a";
    auto t = QDateTime::currentDateTimeUtc();
    t = QDateTime{t.date(), QTime{t.time().hour() / 6 * 6, 0}, Qt::UTC};
    for (int back = 0; back < 6 && storms.empty(); back++, t = t.addSecs(-6 * 3600)) {   // the newest cycle that has any
        const auto folder = "https://nomads.ncep.noaa.gov/pub/data/nccf/com/hafs/prod/hfs" + letter + "." + t.toString("yyyyMMdd").toStdString() + "/" + t.toString("HH").toStdString() + "/";
        const auto text = QString::fromUtf8(URL::getBytes(folder));
        std::set<std::string> found;
        const QRegularExpression pattern{"href=\"([0-9]{2}[lecwsa])\\." + QString::fromStdString(t.toString("yyyyMMddHH").toStdString()) + "\\.hfs" + QString::fromStdString(letter) + "\\.storm\\.atm\\.f126\\.grb2\""};
        for (auto it = pattern.globalMatch(text); it.hasNext();) {
            found.insert(it.next().captured(1).toStdString());
        }
        storms.assign(found.begin(), found.end());
        cycle = t.toString("HH").toStdString() + "Z";
    }
    return storms;
}

std::vector<GfsChart::TrackPoint> GfsRender::hafsTrack(const std::string& model, const std::string& storm, std::string& cycle) {
    const auto gfs = data({}, model, storm);
    GfsData::Run run;
    if (!isHafs(model) || !newestRun(gfs, run)) {
        return {};
    }
    cycle = run.cycle + "Z";
    return GfsChart::parseTrack(URL::getBytes(gfs.fileUrl(run, 0, "trak")).toStdString());
}
