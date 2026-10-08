// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TORNADOVIEWER_H
#define TORNADOVIEWER_H

#include <memory>
#include <string>
#include <vector>
#include <QCheckBox>
#include <QLabel>
#include <QListWidget>
#include "radar/AreaSearch.h"
#include "radar/MapView.h"
#include "tornado/TornadoData.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The SPC tornado database on a map: every tornado of the years chosen (or of the last week, month, year or decade of the data) as a line from its start to its end,
// a dot where the end is not known, coloured by its (E)F rating. Filters: the rating, the state, fatal ones only, those within some distance of one of your saved
// locations, or those that passed through an area drawn on the map. The list beside the map holds the strongest of them; click one to see only it. Hover a track
// for its row when there are 50 or fewer. "Charts..." opens the counts by day of the year, week, month, year and decade.
class TornadoViewer : public Window {
public:
    explicit TornadoViewer(Window * parent);

private:
    void load();
    void applyFilters();
    void paintMap(QPainter&);
    void showHover(const QPointF& pixels);
    void showSelected();
    void closeEventCustom() override { closed = true; }
    void resizeEventCustom() override;
    static QColor colorOf(int mag);
    static QString describe(const UtilityTornado::Tornado&);
    static QString details(const UtilityTornado::Tornado&);

    VBox box;
    HBox rowTop;
    HBox rowMore;
    HBox rowMain;
    ComboBox comboSpan;
    ComboBox comboFrom;
    ComboBox comboTo;
    ComboBox comboRating;
    ComboBox comboState;
    ComboBox comboKind;
    ComboBox comboNear;
    ComboBox comboRadius;
    ComboBox comboSort;
    QCheckBox * checkLowest{};   // the order of the list turned round
    Button buttonArea;
    Button buttonCharts;
    Text textStatus;
    std::unique_ptr<MapView> view;
    std::unique_ptr<AreaSearch> area;
    QLabel * hoverLabel{};
    QListWidget * list{};
    std::shared_ptr<const TornadoData::Database> db;
    vector<const UtilityTornado::Tornado *> shown;
    vector<const UtilityTornado::Tornado *> listed;      // the rows of the list
    const UtilityTornado::Tornado * selected{nullptr};
    const UtilityTornado::Tornado * hovered{nullptr};
    static constexpr size_t hoverLimit = 50;             // the hover works with this many tornadoes or fewer
    bool filling{false};
    bool closed{false};
};

#endif  // TORNADOVIEWER_H
