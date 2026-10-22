// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UISTANDARDS_H
#define UISTANDARDS_H

class QColor;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
#include <QString>
class QWidget;

// One design language for every screen and dialog. The conventions behind the picture sizes are in the wxqt-grib-docs repository (ui-conventions.md); the rest are these:
//
//  Widgets    A screen made of Window uses the wrapper widgets (Button, ComboBox ...). A dialog or a panel that is not a Window builds its own, but through button() and combo() below,
//             so they behave as the wrappers do (tooltip, size, the person's list length). Never a raw `new QPushButton` / `new QComboBox` in a screen.
//  Buttons    A short label (at most buttonChars characters, cut at a word when longer, the full text then in the tooltip; one to three words): the detail goes in the tooltip, not in the label. A label ends in "..." when the button opens another window or asks for more
//             input ("Track map...", "Export view..."); a plain verb acts where it is ("Refresh", "Reload", "Remove"). Sentence case.
//  Titles     "Name - subject": "ACE forecast - Weather Lab ensemble", "HAFS intensity - AL10", "Master map - as of ...". The subject (a storm, a place, a time) comes last.
//  Colors     From the theme: secondary text with muted(), not a hex written into a screen. The colors of data (wind, categories, map layers) belong to the map palette and the charts.
//  Pickers    A dropdown for one choice from a list; a "by parts" dialog (Level / Field / Lines, What / Period / Lines) when the choices combine. Every chart picture has the same
//             right-click menu (ChartExport::popupMenu).
namespace UiStandards {
    constexpr int thumbnailImage = 150;   // a small preview, e.g. the home screen's thumbnails
    constexpr int smallImage = 250;       // a compact picture, e.g. storm report cards
    constexpr int tileImage = 380;        // the standard picture of a grid tile (Climate, Tropical, storm pages, outlooks)
    constexpr int tileMargin = 10;        // room around a tile's picture
    constexpr int tileWidth = tileImage + tileMargin;   // a tile that holds a picture (or a placeholder panel)

    constexpr int buttonChars = 20;   // the most characters a button's label has (the "..." and the menu arrow count)
    // a label cut to buttonChars at a word: the whole words that fit, then "..." (and the menu arrow, if it had one); a label that fits is as it was. `cut` says whether it was cut: the tooltip is then the full label
    QString fitLabel(const QString& label, bool * cut = nullptr);
    // a push button as Button makes it: the tooltip (the tip, else the label), its natural size and not stretched
    QPushButton * button(QWidget * parent, const QString& label, const QString& tip = {});
    // a drop-down as ComboBox makes it: as wide as its longest item, as many rows as the person's setting says
    QComboBox * combo(QWidget * parent);
    // secondary text (credits, notes): the theme's own dimmed color, in light and dark themes
    void muted(QLabel * label);
    // the few fixed looks, in one place (a hex color written into a screen is a bug): the dark read-out that follows the pointer over a map, the amber notice over a picture, the blue
    // "working" line on a map, an error line, and the outlined card with a colored left edge
    void hoverPill(QLabel * label, int alpha = 220);
    void notice(QLabel * label);
    void working(QLabel * label);
    void error(QLabel * label);
    void card(QFrame * frame, const QColor& edge);
}

#endif  // UISTANDARDS_H
