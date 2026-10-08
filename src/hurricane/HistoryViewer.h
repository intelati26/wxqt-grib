// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HISTORYVIEWER_H
#define HISTORYVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include <QListWidget>
#include "hurricane/HurricaneData.h"
#include "radar/MapView.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The historical tracks of NHC's HURDAT2 database (the Atlantic from 1851, the Eastern and Central Pacific from 1949) on a map, coloured by the
// intensity along the track: pick the years, the strongest category reached, a name or id, or the storms that passed within some distance of
// one of your saved locations. The list beside the map holds the storms shown; click one to see only it, hover a track on the map for its name.
class HistoryViewer : public Window {
public:
    explicit HistoryViewer(Window * parent);

private:
    void load();
    void applyFilters();
    void paintMap(QPainter&);
    void showHover(const QPointF& pixels);
    void showSelected();
    void closeEventCustom() override { closed = true; }
    bool eventFilter(QObject *, QEvent *) override;
    void toggleArea();
    void resizeEventCustom() override;
    string basinCode() const { return comboBasin.getIndex() == 1 ? "ep" : "al"; }
    static QColor colorOf(int wind);
    bool passesArea(const UtilityHurdat::Track&) const;
    QString describe(const UtilityHurdat::Track&) const;

    VBox box;
    HBox rowTop;
    HBox rowMain;
    ComboBox comboBasin;
    ComboBox comboFrom;
    ComboBox comboTo;
    ComboBox comboCategory;
    ComboBox comboNear;
    ComboBox comboRadius;
    Entry entrySearch;
    Button buttonArea;
    Text textStatus;
    std::unique_ptr<MapView> view;
    QLabel * hoverLabel{};
    QListWidget * list{};
    std::shared_ptr<HurricaneData::TrackData> data;
    vector<size_t> shown;           // indexes into data->tracks that pass the filters
    int selected{-1};               // an index into data->tracks, or -1
    int hovered{-1};
    // the area search: drag a box on the map, or click a point for a circle of the radius chosen above
    struct Area {
        enum Kind { None, Box, Circle } kind{None};
        double minLat{0}, maxLat{0}, minLon{0}, maxLon{0};   // a box
        double lat{0}, lon{0}, radiusKm{0};                  // a circle
    } area;
    bool areaMode{false};      // the next drag or click on the map makes an area (the map does not pan)
    bool dragging{false};
    QPointF dragStart;
    QPointF dragNow;
    static constexpr size_t hoverLimit = 50;   // the hover highlight works with this many tracks or fewer
    bool filling{false};
    bool closed{false};
    int generation{0};
};

#endif  // HISTORYVIEWER_H
