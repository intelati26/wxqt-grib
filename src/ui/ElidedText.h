// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ELIDEDTEXT_H
#define ELIDEDTEXT_H

#include <QWidget>

// A side panel of check boxes and short labels is narrower than some of their texts. install() makes the text that does not fit end in "..." at the edge of the
// panel instead of being cut off mid-word, and gives every such widget the whole text as its tooltip, so hovering shows what is hidden. It follows the panel when
// it is resized. Rich text and wrapping labels are left alone.
namespace ElidedText {
    void install(QWidget * panel);
}

#endif  // ELIDEDTEXT_H
