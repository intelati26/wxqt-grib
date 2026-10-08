// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/MapLayer.h"
#include "mapkit/MrmsLayer.h"
#include "mapkit/ObsLayers.h"
#include "mapkit/RiverLayers.h"
#include "mapkit/TropicalLayers.h"

// Every layer the master map offers: add a new kind of map data here, with its class, and it appears in the tree.
vector<std::unique_ptr<MapLayer>> MapCatalog::makeLayers() {
    vector<std::unique_ptr<MapLayer>> layers;
    layers.push_back(std::make_unique<MrmsLayer>());
    layers.push_back(std::make_unique<StationLayer>(true));
    layers.push_back(std::make_unique<StationLayer>(false));
    layers.push_back(std::make_unique<GaugeLayer>());
    layers.push_back(std::make_unique<DamLayer>());
    layers.push_back(std::make_unique<BuoyLayer>());
    layers.push_back(std::make_unique<ActiveStormsLayer>());
    layers.push_back(std::make_unique<OutlookLayer>());
    layers.push_back(std::make_unique<WindProbabilityLayer>());
    return layers;
}

// Saved sets of layers and a region: the old Rivers, Tropical and Surface observation screens as views of the master map.
const vector<MapCatalog::Preset>& MapCatalog::presets() {
    static const vector<Preset> all{
        {"Surface observations (United States)", {"obs/airports", "obs/mesonet"}, 20.0, 55.0, -127.0, -65.0},
        {"Rain and rivers (United States)", {"radar/mrms", "rivers/gauges", "rivers/dams"}, 20.0, 55.0, -127.0, -65.0},
        {"Rivers and water (United States)", {"rivers/gauges", "rivers/dams", "rivers/buoys"}, 20.0, 55.0, -127.0, -65.0},
        {"Tropical Atlantic", {"tropical/storms", "tropical/outlook", "tropical/windprob", "rivers/buoys"}, 5.0, 50.0, -100.0, -10.0},
        {"Tropical East Pacific", {"tropical/storms", "tropical/outlook", "tropical/windprob", "rivers/buoys"}, 0.0, 40.0, -150.0, -80.0},
        {"Gulf Coast: storm, warnings, stations, gauges", {"tropical/storms", "tropical/windprob", "obs/airports", "rivers/gauges"}, 22.0, 36.0, -100.0, -78.0},
    };
    return all;
}
