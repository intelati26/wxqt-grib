// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/CaptionedTile.h"
#include <QLabel>
#include <QPalette>
#include <QVBoxLayout>

QWidget * CaptionedTile::make(QWidget * parent, QWidget * picture, const QString& caption, const QString& tip, int captionWidth, bool wrap, int fixedWidth) {
    auto * tile = new QWidget{parent};
    auto * stack = new QVBoxLayout{tile};
    stack->setContentsMargins(0, 0, 0, 0);
    stack->setSpacing(2);
    picture->setToolTip(tip);
    stack->addStretch(1);
    stack->addWidget(picture, 0, Qt::AlignHCenter);
    stack->addStretch(1);
    if (!caption.isEmpty()) {
        auto * label = new QLabel{caption, tile};
        label->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        if (wrap) {
            label->setWordWrap(true);
            label->setMinimumHeight(label->heightForWidth(captionWidth));
        }
        label->setForegroundRole(QPalette::PlaceholderText);
        label->setToolTip(tip);
        label->setMaximumWidth(captionWidth);
        if (wrap) {
            stack->addWidget(label);   // the whole width of the tile, so the lines break where the reserved height expects
        } else {
            stack->addWidget(label, 0, Qt::AlignHCenter);
        }
    }
    if (fixedWidth > 0) {
        tile->setFixedWidth(fixedWidth);
    }
    return tile;
}
