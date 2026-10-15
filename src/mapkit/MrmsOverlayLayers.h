// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef MRMSOVERLAYLAYERS_H
#define MRMSOVERLAYLAYERS_H

#include <memory>
#include <vector>
#include "mapkit/MapLayer.h"
#include "mrms/MrmsOverlays.h"

// The MRMS screen's overlays as master map layers: lightning strikes (by age), the lightning chance as contours, hail swaths and rotation tracks. They read the same data as the
// MRMS screen (MrmsOverlays) and go over any other layer.
class MrmsOverlayLayer : public MapLayer {
public:
    enum class Kind { Lightning, Chance, Hail, Rotation };
    explicit MrmsOverlayLayer(Kind kind);

    string id() const override;
    string path() const override;
    string tip() const override;
    string source() const override { return "NOAA MRMS (NSSL / NCEP)"; }
    int order() const override { return kind == Kind::Lightning ? 48 : kind == Kind::Chance ? 46 : kind == Kind::Hail ? 24 : 26; }
    int refreshSeconds() const override { return 120; }
    void refresh(MapHost&) override;
    void optionChanged(MapHost& host) override { if (reloadNeeded) { reloadNeeded = false; refresh(host); } }
    void paint(QPainter&, MapHost&) override;
    vector<MapLegendRow> legend() const override;
    QWidget * options(QWidget * parent, const std::function<void()>& changed) override;
    string summary() const override;

protected:
    void onEnable(MapHost& host) override { refresh(host); }

private:
    void paintSwath(QPainter&, MapHost&);
    Kind kind;
    int choice{2};   // the period (1 ..): lightning 1-4 (the last minute .. 30 minutes), chance 1-2, hail 1-5, rotation 1-8
    bool loading{false};
    bool reloadNeeded{false};
    bool have{false};
    int generation{0};
    QDateTime utc;
    std::vector<MrmsOverlays::Strike> strikes;
    std::vector<MrmsOverlays::ChanceLine> lines;
    MrmsOverlays::Swath swath;
};

#endif  // MRMSOVERLAYLAYERS_H
