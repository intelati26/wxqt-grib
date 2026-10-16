// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "Text.h"
#include <algorithm>
#include <QEvent>
#include <QPalette>
#include "util/UtilityTextReflow.h"
#include "settings/UIPreferences.h"

namespace {
    // A label that, once asked to (Text::fillWidth), is as wide as its window (less a margin), no wider than 12 inches (a sheet of paper: a longer line is hard to follow), and follows
    // the window as it is resized: the layouts of the screens give a label its size hint, not the window's width. The window's width is used, not the container's, which grows with its
    // contents.
    class FillLabel : public QLabel {
    public:
        using QLabel::QLabel;
        void fill() {
            filling = true;
            window()->installEventFilter(this);
            fit();
        }
        bool eventFilter(QObject * object, QEvent * event) override {
            if (filling && object == window() && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
                fit();
            }
            return QLabel::eventFilter(object, event);
        }
    private:
        void fit() {
            const int limit = static_cast<int>(12.0 * std::max(72, logicalDpiX()));
            const int width = std::clamp(window()->width() - 48, 240, limit);
            if (width != maximumWidth() || width != minimumWidth()) {
                setFixedWidth(width);
            }
        }
        bool filling{false};
    };
}

Text::Text(Window * parent, const string& text)
        : textView{new FillLabel{parent}}
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
    this->text = UtilityTextReflow::enabled() ? UtilityTextReflow::reflow(text) : text;
    textView->setText(this->text);
    textView->adjustSize();
}

void Text::setText(const string& text) {
    this->text = QString::fromStdString(text);
    if (UtilityTextReflow::enabled()) {
        this->text = UtilityTextReflow::reflow(this->text);
    }
    textView->setText(this->text);
    textView->adjustSize();
}

void Text::fillWidth() {
    if (auto * label = dynamic_cast<FillLabel *>(textView)) {
        label->fill();
    }
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
