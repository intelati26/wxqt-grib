// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CAPTIONEDTILE_H
#define CAPTIONEDTILE_H

#include <QString>

class QWidget;

// A picture (or any widget) with its caption underneath, for a grid whose rows are made equally tall (FlowBox::setEqualRowHeights):
// the picture sits in the middle of the room above the caption and the caption is always at the bottom of the tile, so the captions
// of one row line up even when the pictures are different heights. An empty caption leaves the picture centred, alone. The tooltip goes
// on both. The returned widget owns the picture from here on.
namespace CaptionedTile {
    // captionWidth: the widest the caption may be. wrap: break a long caption into lines (its height is reserved, since a flow row
    // measures its tiles without regard to their width). fixedWidth: give the tile that width (0 = as wide as its picture)
    QWidget * make(QWidget * parent, QWidget * picture, const QString& caption, const QString& tip, int captionWidth, bool wrap = false, int fixedWidth = 0);
}

#endif  // CAPTIONEDTILE_H
