// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Text.h"
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
    textView->setStyleSheet("QLabel { color : blue; }");
}

void Text::setGray() {
    textView->setStyleSheet("QLabel {color: gray;}");
}

void Text::setBold() {
    auto font = textView->font();
    font.setBold(true);
    textView->setFont(font);
}

void Text::setBlueOnWhite() {
    textView->setStyleSheet("QLabel {background-color: white; color: blue; font-size: 18px;}");
}

void Text::setVisible(bool b) {
    textView->setVisible(b);
}
