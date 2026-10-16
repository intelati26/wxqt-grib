// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CONEHISTORYLAYER_H
#define CONEHISTORYLAYER_H

#include <QComboBox>
#include <QPointer>
#include <memory>
#include "hurricane/UtilityNhcArchive.h"
#include "mapkit/MapLayer.h"

// How NHC's forecast of a storm changed, advisory by advisory (2008 on), against the track the storm then took. The time bar steps through the advisories: the cone, the forecast
// track and its points of the one on show, the earlier cones faded behind it, and the best track over all of it, with the miss at each forecast hour in the hover. Either one chosen
// storm, or every storm that was active at the time on the bar (the history page opens it that way).
class ConeHistoryLayer : public MapLayer {
public:
    ConeHistoryLayer();
    string id() const override { return "tropical/cone-history"; }
    string path() const override { return "Tropical/Forecast cone history: each advisory against the track the storm took (NHC archive)"; }
    string source() const override { return "NHC advisory cones (gis/forecast/archive) and ATCF best tracks"; }
    string tip() const override { return "How the National Hurricane Center's forecast cone and track changed from advisory to advisory, against where the storm actually went (best track). The time bar steps through the advisories; hover a forecast point for the miss"; }
    int order() const override { return 44; }
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
    void onEnable(MapHost& host) override { hostPointer = &host; refresh(host); }

private:
    struct Shown {
        const UtilityNhcArchive::Storm * storm;
        size_t index;   // the advisory on show
    };
    vector<Shown> shownNow() const;
    QDateTime when() const;                 // the time on the bar, else as of, else now
    void listYear(MapHost&);
    int yearChoice{0};                      // 0: the year of the time on the bar
    string stormChoice;                     // "" = every storm active at the time
    bool ghosts{true};
    bool observed{true};
    bool points{true};
    long shown{0};
    QDateTime asOf;
    vector<std::shared_ptr<UtilityNhcArchive::Storm>> storms;
    vector<UtilityNhcArchive::StormRef> yearList;
    int yearListed{0};
    bool loading{false};
    bool relist{false};
    string error;
    MapHost * hostPointer{nullptr};
    QPointer<QComboBox> stormBox;
};

#endif  // CONEHISTORYLAYER_H
