// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFSPERIODLAYER_H
#define CFSPERIODLAYER_H

#include <map>
#include <memory>
#include <QImage>
#include "gfs/GfsGrid.h"
#include "mapkit/BandLines.h"
#include "mapkit/MapLayer.h"

// The CFSv2's expectation of the next stretch (a week, two weeks, 30, 60 or 90 days) against its own normal, anywhere on the map: the temperature as colors and the precipitation as lines (green
// wetter, brown drier), or either alone. The mean of the member 1 of the four newest runs, from NOAA's open bucket (no quota); the first stretch takes some 25 seconds, the later ones less.
class CfsPeriodLayer : public MapLayer {
public:
    CfsPeriodLayer();
    ~CfsPeriodLayer() override { *alive = false; }
    string id() const override { return "climate/cfs-period"; }
    string path() const override { return "Climate/CFSv2 the next days against the normal (temperature and precipitation)"; }
    string source() const override { return "NOAA/NCEP CFSv2, the mean of its four newest runs against the CFS normal"; }
    string tip() const override { return "NOAA's CFSv2 for the next week to 90 days against its own normal: the temperature as colors, the precipitation as lines (green wetter, brown drier)"; }
    bool underCoast() const override { return true; }
    int order() const override { return 12; }
    int refreshSeconds() const override { return 6 * 3600; }
    void refresh(MapHost&) override;
    void paint(QPainter&, MapHost&) override;
    MapHit pick(const QPointF& pixels, MapHost&) const override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    void optionChanged(MapHost& host) override;
    string summary() const override;
    bool working() const override { return loading; }

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    struct Frame {
        GfsGrid::Grid temperature, precipitation;   // in the program's units (degrees F or C, mm/day)
        QImage image;
        std::vector<BandLines::Contour> wet, dry;
    };
    void load(MapHost&);
    void install(GfsGrid::Grid temperature, GfsGrid::Grid precipitation, int length);
    QImage colored(const GfsGrid::Grid&) const;
    std::vector<std::pair<double, QColor>> stops() const;
    int days{30};
    int show{0};          // 0 the temperature in colors with the precipitation as lines, 1 the temperature, 2 the precipitation
    int opacity{85};
    bool lines{true};
    bool loading{false};
    bool changeRequested{false};
    qint64 loadedAt{0};   // when the frame on hand was worked out (seconds since 1970)
    std::map<int, Frame> frames;   // by the length of the stretch
    string error;
    std::shared_ptr<bool> alive{std::make_shared<bool>(true)};
};

#endif  // CFSPERIODLAYER_H
