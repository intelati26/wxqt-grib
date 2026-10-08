// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TROPICALLAYERS_H
#define TROPICALLAYERS_H

#include <memory>
#include <vector>
#include "hurricane/HurricaneData.h"
#include "mapkit/MapLayer.h"

// The tropical screens' drawings as layers: every active storm with its track so far, NHC's forecast, cone and watches / warnings (inland zones too);
// the Tropical Weather Outlook's areas; and NHC's wind speed probability bands.
class ActiveStormsLayer : public MapLayer {
public:
    string id() const override { return "tropical/storms"; }
    string path() const override { return "Tropical/Active storms: track, forecast, cone, warnings"; }
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

#endif  // TROPICALLAYERS_H
