// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYTEXTREFLOW_H
#define UTILITYTEXTREFLOW_H

#include <QString>

class QPlainTextEdit;

// NWS text products (forecast discussions, advisories, outlooks) are broken into lines of about 66 characters. On a screen wider or narrower than that the hard breaks leave ragged
// lines, so the breaks inside a paragraph are taken out and the text wraps to the window; the blank lines between paragraphs, headings, lists and tables stay as they were.
namespace UtilityTextReflow {
    QString reflow(const QString& text);
    bool enabled();   // the TEXT_REFLOW setting (on by default)
    // The same for a text box (QPlainTextEdit) that holds a product: the text reflowed (when the setting is on), wrapped to the box, and the box no wider than 12 inches (a sheet of paper: a longer line is
    // hard to follow), as the text screens (Text::fillWidth) are. The font is the caller's.
    void setProduct(QPlainTextEdit * box, const QString& text);
    void style(QPlainTextEdit * box);   // the wrapping and the width alone, for a box that is filled later with setProduct
}

#endif  // UTILITYTEXTREFLOW_H
