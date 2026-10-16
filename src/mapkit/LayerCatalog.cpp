// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MapLayer.h"
#include "mapkit/ForecastPointLayer.h"
#include "mapkit/GoesLayer.h"
#include "mapkit/GibsLayer.h"
#include "mapkit/ClimateGridLayer.h"
#include "mapkit/CfsOutlookLayer.h"
#include "mapkit/ConeHistoryLayer.h"
#include "mapkit/OutlookHistoryLayer.h"
#include "mapkit/CpcPrecipLayer.h"
#include "mapkit/GoesHistoryLayer.h"
#include "mapkit/RadarHistoryLayer.h"
#include "mapkit/StormReportsLayer.h"
#include "mapkit/LightningCastLayer.h"
#include "mapkit/MrmsLayer.h"
#include "mapkit/MrmsOverlayLayers.h"
#include "mapkit/ObsLayers.h"
#include "mapkit/RiverLayers.h"
#include "mapkit/SpaceLayers.h"
#include "mapkit/TornadoLayer.h"
#include "mapkit/TropicalLayers.h"

// Every layer the master map offers: add a new kind of map data here, with its class, and it appears in the tree.
vector<std::unique_ptr<MapLayer>> MapCatalog::makeLayers() {
    vector<std::unique_ptr<MapLayer>> layers;
    layers.push_back(std::make_unique<GoesLayer>());
    layers.push_back(std::make_unique<GibsLayer>());
    layers.push_back(std::make_unique<GoesHistoryLayer>());
    layers.push_back(std::make_unique<LightningCastLayer>());
    layers.push_back(std::make_unique<CpcPrecipLayer>());
    layers.push_back(std::make_unique<ClimateGridLayer>());
    layers.push_back(std::make_unique<CfsOutlookLayer>());
    layers.push_back(std::make_unique<ConeHistoryLayer>());
    layers.push_back(std::make_unique<OutlookHistoryLayer>());
    layers.push_back(std::make_unique<MrmsLayer>());
    layers.push_back(std::make_unique<RadarHistoryLayer>());
    layers.push_back(std::make_unique<StormReportsLayer>());
    layers.push_back(std::make_unique<MrmsOverlayLayer>(MrmsOverlayLayer::Kind::Hail));
    layers.push_back(std::make_unique<MrmsOverlayLayer>(MrmsOverlayLayer::Kind::Rotation));
    layers.push_back(std::make_unique<MrmsOverlayLayer>(MrmsOverlayLayer::Kind::Chance));
    layers.push_back(std::make_unique<MrmsOverlayLayer>(MrmsOverlayLayer::Kind::Lightning));
    layers.push_back(std::make_unique<AuroraLayer>());
    layers.push_back(std::make_unique<StationLayer>(true));
    layers.push_back(std::make_unique<StationLayer>(false));
    layers.push_back(std::make_unique<GaugeLayer>());
    layers.push_back(std::make_unique<DamLayer>());
    layers.push_back(std::make_unique<BuoyLayer>());
    layers.push_back(std::make_unique<TornadoLayer>());
    layers.push_back(std::make_unique<ActiveStormsLayer>());
    layers.push_back(std::make_unique<DeepMindGenesisLayer>());
    layers.push_back(std::make_unique<EnsembleSpaghettiLayer>());
    layers.push_back(std::make_unique<OutlookLayer>());
    layers.push_back(std::make_unique<WindProbabilityLayer>());
    layers.push_back(std::make_unique<ForecastPointLayer>());
    return layers;
}

// Saved sets of layers and a region: the old Rivers, Tropical and Surface observation screens as views of the master map.
const vector<MapCatalog::Preset>& MapCatalog::presets() {
    static const vector<Preset> all{
        {"Surface observations (United States)", {"obs/airports", "obs/mesonet"}, 20.0, 55.0, -127.0, -65.0},
        {"Rain and rivers (United States)", {"radar/mrms", "rivers/gauges", "rivers/dams"}, 20.0, 55.0, -127.0, -65.0},
        {"Rivers and water (United States)", {"rivers/gauges", "rivers/dams", "rivers/buoys"}, 20.0, 55.0, -127.0, -65.0},
        {"Ensemble tracks and where new storms may form (DeepMind, world)", {"tropical/spaghetti", "tropical/deepmind-genesis"}, -50.0, 55.0, -180.0, 180.0},
        {"Raw ensemble spaghetti: every member of every system (world)", {"tropical/spaghetti"}, -50.0, 55.0, -180.0, 180.0},
        {"Raw ensemble spaghetti (Atlantic)", {"tropical/spaghetti", "tropical/storms"}, 5.0, 50.0, -100.0, -10.0},
        {"Tropical Atlantic", {"tropical/storms", "tropical/outlook", "tropical/windprob", "rivers/buoys"}, 5.0, 50.0, -100.0, -10.0},
        {"Tropical East Pacific", {"tropical/storms", "tropical/outlook", "tropical/windprob", "rivers/buoys"}, 0.0, 40.0, -150.0, -80.0},
        {"Severe storms: radar, hail, rotation and lightning (United States)", {"radar/mrms", "radar/mrms-hail", "radar/mrms-rotation", "radar/mrms-lightning", "obs/airports"}, 24.0, 50.0, -105.0, -75.0},
        {"Tornado tracks and radar (United States)", {"severe/tornadoes", "radar/mrms", "obs/airports"}, 24.0, 50.0, -105.0, -75.0},
        {"Polar orbiters: true color and night lights (world)", {"satellite/polar"}, -50.0, 70.0, -180.0, 180.0},
        {"Satellite and storms (Atlantic)", {"satellite/goes", "tropical/storms", "tropical/outlook", "tropical/windprob"}, 5.0, 50.0, -100.0, -10.0},
        {"Aurora and the weather (North America)", {"space/aurora", "radar/mrms", "obs/airports"}, 25.0, 75.0, -170.0, -50.0},
        {"Forecast points (your saved locations)", {"forecast/points", "obs/airports"}, 20.0, 55.0, -127.0, -65.0},
        {"Gulf Coast: storm, warnings, stations, gauges", {"tropical/storms", "tropical/windprob", "obs/airports", "rivers/gauges"}, 22.0, 36.0, -100.0, -78.0},
    };
    return all;
}
