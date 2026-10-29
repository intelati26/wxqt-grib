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

// The CFSv2's expectation of the next stretch (a week, two weeks, 30, 60 or 90 days), anywhere on the map: the temperature as colors and the precipitation as lines (green wetter, brown drier)
// against its own normal, or any of the temperature, the precipitation and the sea level pressure alone, as a departure from the normal or as the values themselves (the pressure with isobars).
// The mean of the member 1 of the four newest runs, from NOAA's open bucket (no quota); the first stretch takes some 25 seconds, the later ones less. Only the fields shown are read.
class CfsPeriodLayer : public MapLayer {
public:
    CfsPeriodLayer();
    ~CfsPeriodLayer() override { *alive = false; }
    string id() const override { return "climate/cfs-period"; }
    string path() const override { return "Climate/CFSv2 the next days (temperature, precipitation, pressure)"; }
    string source() const override { return "NOAA/NCEP CFSv2, the mean of its four newest runs against the CFS normal"; }
    string tip() const override { return "NOAA's CFSv2 for the next week to 90 days: the temperature, precipitation or sea level pressure, against its own normal or as values (the first view: temperature as colors, precipitation as lines, green wetter, brown drier)"; }
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
        std::map<string, GfsGrid::Grid> grids;   // by key: "t2m", "precip", "mslp" (a departure from the normal) and "t2m.v", "precip.v", "mslp.v" (the mean itself), in the CFS's units (deg C, mm/day, hPa)
        QImage image;                            // of the field on show
        std::vector<BandLines::Contour> wet, dry, isobars;
    };
    void load(MapHost&);
    void install(std::map<string, GfsGrid::Grid> grids, int length);
    void drawFrame(Frame&) const;
    QImage colored(const GfsGrid::Grid&) const;
    std::vector<string> needed() const;                 // the keys the view on show is made of
    bool hasNeeded(const Frame&) const;
    int days{30};
    // 0 the temperature in colors with the precipitation as lines, 1 the temperature, 2 the precipitation, 3 the mean temperature, 4 the mean precipitation rate, 5 the sea level pressure against the normal,
    // 6 the mean sea level pressure with isobars
    int show{0};
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
