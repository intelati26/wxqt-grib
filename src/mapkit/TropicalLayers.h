// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TROPICALLAYERS_H
#define TROPICALLAYERS_H

#include <memory>
#include <vector>
#include "hurricane/HurricaneData.h"
#include "hurricane/EnsembleStyle.h"
#include "hurricane/UtilityEnsembleStats.h"
#include "mapkit/MapLayer.h"
#include "tropical/UtilityCira.h"

// The tropical screens' drawings as layers: every active storm with its track so far, NHC's forecast, cone and watches / warnings (inland zones too);
// the Tropical Weather Outlook's areas; and NHC's wind speed probability bands.
class ActiveStormsLayer : public MapLayer {
public:
    string id() const override { return "tropical/storms"; }
    string path() const override { return "Tropical/Active storms: track, forecast, cone, warnings"; }
    string source() const override { return "NOAA National Hurricane Center, NWS watches and warnings"; }
    string tip() const override { return "Every active storm and invest of the Atlantic, East and Central Pacific: the best track so far, NHC's forecast and cone, the watches and warnings; click one for its full screen"; }
    int order() const override { return 70; }
    int refreshSeconds() const override { return 600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Storm {
        HurricaneData::StormEntry entry;
        std::shared_ptr<HurricaneData::StormData> data;
        std::shared_ptr<HurricaneData::GisData> gis;
        string basin;
    };
    bool loading{false};
    string error;
    bool showCone{true};
    bool showWarnings{true};
    bool showInland{true};
    bool showForecast{true};
    std::shared_ptr<vector<Storm>> storms;
};

// Every active storm outside the Atlantic and East Pacific the NHC covers - the Western Pacific (typhoons), North Indian Ocean and Southern Hemisphere - from CIRA / RAMMB's real-time
// pages (JTWC's fixes and forecast): the track so far, the forecast with its hours, the intensity at each fix. Click one for its CIRA page (pictures, loops, JTWC / JMA text, SAR winds).
class WorldStormsLayer : public MapLayer {
public:
    string id() const override { return "tropical/world-storms"; }
    string path() const override { return "Tropical/Storms of the Western Pacific, Indian Ocean and Southern Hemisphere"; }
    string source() const override { return "CIRA / RAMMB TC Real-Time, JTWC"; }
    string tip() const override { return "Every active typhoon, cyclone and invest outside the NHC's basins: the track so far and the forecast, coloured by intensity; click one for its page with the SAR winds"; }
    int order() const override { return 71; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Storm {
        UtilityCira::Storm info;
        UtilityCira::StormPage page;
    };
    bool loading{false};
    string error;
    std::shared_ptr<vector<Storm>> storms;
};

class OutlookLayer : public MapLayer {
public:
    string id() const override { return "tropical/outlook"; }
    string path() const override { return "Tropical/Development areas (Tropical Weather Outlook)"; }
    string source() const override { return "NOAA National Hurricane Center"; }
    string tip() const override { return "NHC's areas of possible tropical development with their 2 and 7 day chances"; }
    int order() const override { return 30; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool loading{false};
    std::shared_ptr<HurricaneData::OutlookData> data;
};

class WindProbabilityLayer : public MapLayer {
public:
    string id() const override { return "tropical/windprob"; }
    string path() const override { return "Tropical/Wind probabilities (NHC, 5 days)"; }
    string source() const override { return "NOAA National Hurricane Center"; }
    string tip() const override { return "NHC's chance of sustained winds of at least 34, 50 or 64 kt in the next five days, all active storms together"; }
    int order() const override { return 25; }
    int refreshSeconds() const override { return 1800; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    bool loading{false};
    int threshold{0};   // 0 / 1 / 2: 34 / 50 / 64 kt
    std::shared_ptr<HurricaneData::WspData> data;
};

// The ensembles' raw tracks, every member of every system and every forecast hour, whether or not a storm has a name or a screen of its own: NOAA's GEFS (the systems NHC has files for), ECMWF's
// IFS and AIFS ensembles, and DeepMind's FNV3 and WeatherNext 3 for the whole world. Colored by model, by forecast hour or by the wind of the member there.
class EnsembleSpaghettiLayer : public MapLayer {
public:
    string id() const override { return "tropical/spaghetti"; }
    string path() const override { return "Tropical/Ensemble tracks: GEFS, ECMWF and DeepMind, every member (spaghetti)"; }
    string source() const override { return "NOAA / NHC ATCF (GEFS), ECMWF open data (CC BY 4.0), Google DeepMind Weather Lab. " + EnsembleStyle::deepMindCredit(); }
    string tip() const override { return "All the members of GEFS, the ECMWF IFS and AIFS ensembles and DeepMind's FNV3 / WeatherNext 3, for every system they track (named or not), colored by model, forecast hour or wind. Hover a line for the member and hour; click a storm of the Atlantic or Pacific for its screen"; }
    int order() const override { return 73; }
    int refreshSeconds() const override { return 3600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override {
        hostPointer = &host;
        refresh(host);
    }

private:
    struct Run {
        string label;
        string cycle;
        vector<UtilityEcmwfTracks::Storm> storms;
        vector<vector<UtilityEnsembleStats::Hour>> means;
    };
    static constexpr int modelCount = 5;
    static int modelIndex(const string& label);
    int limitHours() const;
    QColor colorOf(const Run& run, int hour, double wind) const;
    bool loading{false};
    string error;
    std::shared_ptr<vector<Run>> runs;
    bool showModel[modelCount]{true, true, true, true, true};   // GEFS, IFS ENS, AIFS ENS, FNV3, WNV3
    bool showMembers{true};
    bool showMean{true};
    bool showDays{false};
    int colorMode{0};      // 0 model, 1 forecast hour, 2 wind
    int hoursChoice{0};
    MapHost * hostPointer{nullptr};
};

// DeepMind's large (1000 member) ensemble: where storms that do not exist yet may form, and how many members form them. One point per member that forms the storm, one cluster per
// possible storm, labelled with the share of the members. The file is large (about 37 MB), so it is read only when this layer is on.
class DeepMindGenesisLayer : public MapLayer {
public:
    string id() const override { return "tropical/deepmind-genesis"; }
    string path() const override { return "Tropical/Ensemble tracks: where new storms may form (DeepMind 1000 members)"; }
    string source() const override { return "Google DeepMind Weather Lab. " + EnsembleStyle::deepMindCredit(); }
    string tip() const override { return "From DeepMind's 1000-member cyclone ensemble: each dot is a member that forms a new storm, there; the label is the share of the members that do. Reads a 37 MB file"; }
    int order() const override { return 71; }
    int refreshSeconds() const override { return 21600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF&, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override {
        hostPointer = &host;
        refresh(host);
    }

private:
    struct Cluster {
        string track;
        vector<UtilityWeatherLab::GenesisPoint> points;   // those inside the time window
        double lat{0.0}, lon{0.0};                        // where most of them form
        double hourMedian{0.0}, pressureMedian{0.0}, windMedian{0.0};
        double chance{0.0};                               // share of all members
    };
    vector<Cluster> clusters() const;   // those that pass the filters
    static QColor colorFor(double chance);
    bool loading{false};
    string error;
    string cycle;
    int members{0};
    std::shared_ptr<vector<UtilityWeatherLab::Genesis>> data;
    int chanceChoice{2};     // the least chance shown: 5% to begin with (1% fills the tropics with dots)
    int windowChoice{0};     // how far ahead
    MapHost * hostPointer{nullptr};
};

#endif  // TROPICALLAYERS_H
