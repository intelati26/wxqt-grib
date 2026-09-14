// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Text.h"
#include <QPalette>
#include "settings/UIPreferences.h"

Text::Text(Window * parent, const string& text)
        : textView{new QLabel{parent}}
        , text{QString::fromStdString(text)}
{
    textView->setText(this->text);
    textView->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    textView->setWordWrap(true);
    textView->setTextFormat(Qt::PlainText);
    textView->setTextInteractionFlags(Qt::TextSelectableByMouse);
}

// QLabel * Text::get() {
//     return textView;
// }

QLabel * Text::getView() {
    return textView;
}

void Text::setWordWrap(bool c) {
    textView->setWordWrap(c);
}

void Text::setText(const QString& text) {
    textView->setText(text);
    textView->adjustSize();
    this->text = text;
}

void Text::setText(const string& text) {
    this->text = QString::fromStdString(text);
    textView->setText(this->text);
    textView->adjustSize();
}

void Text::setFixedWidth() {
    auto font = QFont{"Courier New"};
    font.setStyleHint(QFont::TypeWriter);
    textView->setFont(font);
}

void Text::setBlue() {
    auto pal = textView->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Link));
    textView->setPalette(pal);
}

void Text::setGray() {
    auto pal = textView->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
    textView->setPalette(pal);
}

void Text::setBold() {
    auto font = textView->font();
    font.setBold(true);
    textView->setFont(font);
}

void Text::setBlueOnWhite() {
    auto pal = textView->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::Link));
    textView->setPalette(pal);
    auto font = textView->font();
    font.setPointSize(14);
    textView->setFont(font);
}

void Text::setVisible(bool b) {
    textView->setVisible(b);
}
