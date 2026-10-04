// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UISTANDARDS_H
#define UISTANDARDS_H

// The sizes shared by the screens that show pictures in a grid. Use these names, not numbers, so every grid looks alike.
// (The conventions behind them are written up in the wxqt-grib-docs repository, ui-conventions.md.)
namespace UiStandards {
    constexpr int thumbnailImage = 150;   // a small preview, e.g. the home screen's thumbnails
    constexpr int smallImage = 250;       // a compact picture, e.g. storm report cards
    constexpr int tileImage = 380;        // the standard picture of a grid tile (Climate, Tropical, storm pages, outlooks)
    constexpr int tileMargin = 10;        // room around a tile's picture
    constexpr int tileWidth = tileImage + tileMargin;   // a tile that holds a picture (or a placeholder panel)
}

#endif  // UISTANDARDS_H
