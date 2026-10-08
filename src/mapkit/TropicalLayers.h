// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TROPICALLAYERS_H
#define TROPICALLAYERS_H

#include <memory>
#include <vector>
#include "hurricane/HurricaneData.h"
#include "hurricane/UtilityEnsembleStats.h"
#include "mapkit/MapLayer.h"

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

// Google DeepMind's Weather Lab cyclone ensembles (FNV3 and the experimental WeatherNext 3) for every storm in the world: each member's track, and the mean of the members.
// The terms of use ask for a credit wherever the data is shown, so the legend carries it (and so does an exported picture).
class DeepMindLayer : public MapLayer {
public:
    string id() const override { return "tropical/deepmind"; }
    string path() const override { return "Tropical/DeepMind Weather Lab: every cyclone in the world (experimental)"; }
    string source() const override { return "Google DeepMind Weather Lab; (c) 2024-6 Google LLC; experimental data, not for real world use"; }
    string tip() const override { return "The ensemble forecasts of DeepMind's cyclone models for every storm in the world: each member's track and the mean. Hover the mean for the winds and pressures; click a storm of the Atlantic or Pacific for its full screen"; }
    int order() const override { return 72; }
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
        vector<vector<UtilityEnsembleStats::Hour>> means;   // of each storm, as far as half its members are still a cyclone
        bool shown{true};
    };
    int limitHours() const;
    bool loading{false};
    string error;
    std::shared_ptr<vector<Run>> runs;
    bool showFnv{true};
    bool showWeatherNext{true};
    bool showMembers{true};
    bool showMean{true};
    int hoursChoice{0};
    MapHost * hostPointer{nullptr};
};

// DeepMind's large (1000 member) ensemble: where storms that do not exist yet may form, and how many members form them. One point per member that forms the storm, one cluster per
// possible storm, labelled with the share of the members. The file is large (about 37 MB), so it is read only when this layer is on.
class DeepMindGenesisLayer : public MapLayer {
public:
    string id() const override { return "tropical/deepmind-genesis"; }
    string path() const override { return "Tropical/DeepMind: where new storms may form (1000 members, experimental)"; }
    string source() const override { return "Google DeepMind Weather Lab; (c) 2024-6 Google LLC; experimental data, not for real world use"; }
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
    int chanceChoice{1};     // the least chance shown
    int windowChoice{0};     // how far ahead
    MapHost * hostPointer{nullptr};
};

#endif  // TROPICALLAYERS_H
