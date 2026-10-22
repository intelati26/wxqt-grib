// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/UiStandards.h"
#include <algorithm>
#include <cstring>
#include <QColor>
#include <QComboBox>
#include <QFrame>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QSizePolicy>
#include "settings/UIPreferences.h"

QString UiStandards::fitLabel(const QString& label, bool * cut) {
    if (cut != nullptr) {
        *cut = false;
    }
    if (label.size() <= buttonChars) {
        return label;
    }
    QString core = label, arrow;
    const QString triangle = QString::fromUtf8("\xE2\x96\xBE");
    if (core.trimmed().endsWith(triangle)) {   // a menu: the arrow stays
        core = core.trimmed();
        core.chop(triangle.size());
        core = core.trimmed();
        arrow = " " + triangle;
    }
    while (core.endsWith('.')) {
        core.chop(1);
    }
    core = core.trimmed();
    const int room = buttonChars - 3 - static_cast<int>(arrow.size());
    QString out = core.left(std::max(4, room));
    if (core.size() > out.size() && !core.at(out.size()).isSpace()) {   // in the middle of a word: back to the last space
        const auto space = out.lastIndexOf(' ');
        if (space >= 4) {
            out = out.left(space);
        }
    }
    for (bool again = true; again;) {   // not ending on a word that needs the next one ("Recon plan of the")
        again = false;
        while (out.endsWith(',') || out.endsWith(' ') || out.endsWith(':') || out.endsWith('(')) {
            out.chop(1);
            again = true;
        }
        for (const char * word : {" of", " the", " a", " an", " and", " for", " to", " on", " in", " at", " by", " with"}) {
            if (out.endsWith(word, Qt::CaseInsensitive) && out.size() > 8) {
                out.chop(static_cast<int>(std::strlen(word)));
                again = true;
            }
        }
    }
    if (cut != nullptr) {
        *cut = true;
    }
    return out + "..." + arrow;
}

QPushButton * UiStandards::button(QWidget * parent, const QString& label, const QString& tip) {
    bool cut = false;
    const auto shown = fitLabel(label, &cut);
    auto * button = new QPushButton{shown, parent};
    button->setToolTip(tip.isEmpty() ? label : tip);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    return button;
}

QComboBox * UiStandards::combo(QWidget * parent) {
    auto * box = new QComboBox{parent};
    box->setMaxVisibleItems(UIPreferences::comboBoxSize);
    box->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    return box;
}

void UiStandards::muted(QLabel * label) {
    label->setForegroundRole(QPalette::PlaceholderText);
}

void UiStandards::hoverPill(QLabel * label, int alpha) {
    label->setStyleSheet(QString{"QLabel { background-color: rgba(15, 15, 15, %1); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }"}.arg(alpha));
}

void UiStandards::notice(QLabel * label) {
    label->setStyleSheet("QLabel { background: #fff3cd; color: #664d03; padding: 4px 8px; border-radius: 3px; }");
}

void UiStandards::working(QLabel * label) {
    label->setStyleSheet("QLabel { background-color: rgba(20, 60, 120, 225); color: #ffffff; padding: 5px 10px; border-radius: 4px; }");
}

void UiStandards::error(QLabel * label) {
    label->setStyleSheet("QLabel { color: #b00020; }");
}

void UiStandards::card(QFrame * frame, const QColor& edge) {
    frame->setFrameShape(QFrame::StyledPanel);
    frame->setObjectName("stormCard");
    frame->setStyleSheet("QFrame#stormCard { border: 1px solid #999; border-left: 6px solid " + edge.name() + "; }");
}
