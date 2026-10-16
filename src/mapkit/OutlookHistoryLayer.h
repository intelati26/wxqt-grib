// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OUTLOOKHISTORYLAYER_H
#define OUTLOOKHISTORYLAYER_H

#include <memory>
#include "hurricane/UtilityNhcArchive.h"
#include "mapkit/MapLayer.h"

// How the National Hurricane Center's Tropical Weather Outlook changed from issuance to issuance (gis/gtwo/archive): the areas of possible development, coloured by their chance of
// forming in seven days, the earlier issuances faded behind. The time bar steps through the issuances around the time asked for (the last days when live); with the cone history
// layer on the same map, an area can be followed until it becomes a storm.
class OutlookHistoryLayer : public MapLayer {
public:
    OutlookHistoryLayer();
    ~OutlookHistoryLayer() override { *alive = false; }
    string id() const override { return "tropical/outlook-history"; }
    string path() const override { return "Tropical/Development areas history: each Tropical Weather Outlook issuance (NHC archive)"; }
    string source() const override { return "NHC Graphical Tropical Weather Outlook archive"; }
    string tip() const override { return "The areas the National Hurricane Center watched for development, issuance by issuance (about every 6 hours): the chance of forming in 2 and 7 days. Earlier issuances stay behind as faint outlines, so an area can be followed as it grows, moves, firms up or ends; turn on the cone history too to see it become a storm"; }
    int order() const override { return 43; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;
    bool timeAware() const override { return true; }
    vector<long> times() const override;
    void showTime(long seconds, MapHost&) override;
    string timeText() const override;
    void setAsOf(const QDateTime& utc, MapHost&) override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    int current() const;                   // the issuance on show: the newest at or before the time on the bar
    QDateTime when() const;
    vector<UtilityNhcArchive::Outlook> issuances;
    int windowDays{6};
    bool ghosts{true};
    long shown{0};
    QDateTime asOf;
    bool loading{false};
    bool relist{false};
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // OUTLOOKHISTORYLAYER_H
