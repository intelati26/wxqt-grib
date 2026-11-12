// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PANEGRID_H
#define PANEGRID_H

#include <array>
#include <functional>
#include <vector>
#include <QByteArray>
#include <QGridLayout>
#include <QWidget>
#include "models/CompareTile.h"

class ZoomImage;
class ChartHover;

// How the panes of a screen are laid out, one rule for them all: one, a row of two, a row of three, two by two. `widgets` are the panes in order (the Model Viewer's own main pane among them),
// `shown` how many are in use; the others are hidden.
namespace PaneLayout {
    void apply(QGridLayout * grid, const std::vector<QWidget *>& widgets, int shown);
}

// The panes of a screen as one view: the zoom and place of the picture touched go to the others, the read-out of the point under the pointer shows in each pane (and the same place is marked on
// every picture, to read the panes against each other). Members past `active()` are left alone.
class PaneLink : public QObject {
public:
    struct Member {
        ZoomImage * image;
        std::function<ChartHover *()> hover;   // may give none (a pane with no values yet)
    };
    PaneLink(QObject * parent, std::vector<Member> members, std::function<size_t()> active);
    void block(bool on) { syncing = on; }   // a view set from the program is not passed on to the other panes

private:
    std::vector<Member> members;
    std::function<size_t()> active;
    bool syncing{false};
};

// One to four chart panes side by side as the Model Viewer lays its tiles out (1, 1 x 2, 1 x 3, 2 x 2): each a CompareTile (caption, Change..., the picture with the hover read-out of its values).
// A click marks the same point in every pane, a double click opens the picture whole. The screens that show several charts at once (the 15 minute and NWS analysis windows) are built on this.
class PaneGrid : public QWidget {
public:
    static constexpr int maxPanes = 4;
    explicit PaneGrid(QWidget * parent);
    CompareTile * tile(size_t index) const { return tiles[index]; }
    void setCount(int panes);                                   // how many are shown
    int count() const { return shownCount; }
    void onChange(const std::function<void(int)>& handler);
    void onBarbs(const std::function<void(int)>& handler);       // a pane's Barbs button     // a pane's Change... button
    void setMessage(const QString& text);                       // the same words over every pane's picture
    void hoverAt(double fx, double fy);                         // the read-out of every pane at that point of the picture (a development aid)
    // the pictures side by side in one picture, laid out as the panes are (900 pixels wide each)
    static QByteArray composite(const std::vector<QByteArray>& pictures);

private:
    QGridLayout * grid{};
    std::array<CompareTile *, maxPanes> tiles{};
    int shownCount{1};
};

#endif  // PANEGRID_H
