// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/CollapsibleSection.h"
#include "util/Utility.h"

CollapsibleSection::CollapsibleSection(const QString& title, const std::string& prefKey, bool openByDefault, QWidget * parent)
    : QWidget{parent}
    , title{title}
    , prefKey{prefKey}
{
    auto * column = new QVBoxLayout{this};
    column->setContentsMargins(0, 2, 0, 2);
    column->setSpacing(2);
    bar = new QToolButton{this};
    bar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    bar->setAutoRaise(true);
    bar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bar->setStyleSheet("QToolButton { text-align: left; font-weight: bold; padding: 3px 4px; border: none; border-bottom: 1px solid #aaa; }");
    bar->setCursor(Qt::PointingHandCursor);
    holder = new QWidget{this};
    content = new QVBoxLayout{holder};
    content->setContentsMargins(6, 2, 0, 4);
    content->setSpacing(2);
    column->addWidget(bar);
    column->addWidget(holder);
    QObject::connect(bar, &QToolButton::clicked, [this] { setOpen(!holder->isVisible()); });
    setOpen(Utility::readPref(prefKey, openByDefault ? "true" : "false") == "true");
}

void CollapsibleSection::setOpen(bool open) {
    holder->setVisible(open);
    bar->setText(QString::fromUtf8(open ? "\xE2\x96\xBE  " : "\xE2\x96\xB8  ") + title);
    Utility::writePref(prefKey, open ? "true" : "false");
}
