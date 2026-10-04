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
    QWidget * make(QWidget * parent, QWidget * picture, const QString& caption, const QString& tip, int captionWidth);
}

#endif  // CAPTIONEDTILE_H
