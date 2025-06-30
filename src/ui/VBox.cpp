// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/VBox.h"
#include "settings/UIPreferences.h"
#include "util/UtilityList.h"
#include "util/UtilityUI.h"

VBox::VBox()
    : box{new QVBoxLayout{}}
{
    box->setSpacing(UIPreferences::boxPadding);
    box->setContentsMargins(0, 0, 0, 0);
    // box->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    // QScroller::grabGesture(box, QScroller::LeftMouseButtonGesture); // TouchGesture
    // QScroller * scroller = QScroller::scroller(box);
    // scroller->grabGesture(box, QScroller::TouchGesture);
    // scroller->grabGesture(box, QScroller::LeftMouseButtonGesture);
}

void VBox::getAndShow(Window * win) {
    win->centralWidget->setLayout(getView());
    win->show();
}

void VBox::addWidgetReal(QWidget * w, int stretch, Qt::Alignment alignment) {
    box->addWidget(w, stretch, alignment);
}

void VBox::addWidget(Widget2& w, int stretch, Qt::Alignment alignment) {
    box->addWidget(w.getView(), stretch, alignment);
}

void VBox::addWidgetAndCenter(Widget2& w) {
    box->addWidget(w.getView());
    box->setAlignment(w.getView(), Qt::AlignHCenter);
}

void VBox::addLayoutReal(QLayout * w, int stretch) {
    box->addLayout(w, stretch);
}

void VBox::addLayout(Box& w, int stretch) {
    box->addLayout(w.getView(), stretch);
}

void VBox::addStretch() {
    box->addStretch();
}

void VBox::addMargins() {
    box->setContentsMargins(UIPreferences::textPadding);
}

void VBox::setSpacing(int i) {
    box->setSpacing(i);
}

// https://doc.qt.io/qt-5/qt.html#AlignmentFlag-enum
void VBox::setAlignment(QWidget * w, Qt::Alignment alignment) {
    box->setAlignment(w, alignment);
}

void VBox::setAlignment(Widget2& w, Qt::Alignment alignment) {
    box->setAlignment(w.getView(), alignment);
}

void VBox::removeChildren() {
    UtilityUI::removeChildren(getView());
}

// void VBox::addImageRows(Window * parent, const vector<string>& urls, vector<Image>& images, int numberAcross) {
//     for (auto index : range(urls.size())) {
//         images.emplace_back(parent);
//         if ((boxRows.size() <= static_cast<size_t>(index / numberAcross))) {
//             boxRows.emplace_back();
//         }
//         boxRows.back().addWidget(images.back());
//     }
//     for (auto& b : boxRows) {
//         box->addLayout(b.getView());
//     }
// }

void VBox::addImageRows(Window * parent, const vector<string>& urls, vector<Image>& images, int imagesAcross, int width) {
    setSpacing(0);
    for (auto index : range(urls.size())) {
        images.emplace_back(parent);
        // images.back().setNumberAcross(numberAcross, width);
        if (width != 0) {
            images.back().setNumberAcross(imagesAcross, width);
        }
        if ((boxRows.size() <= static_cast<size_t>(index / imagesAcross))) {
            boxRows.emplace_back();
            boxRows.back().setSpacing(0);
        }
        boxRows.back().addWidget(images.back());
    }
    for (auto& b : boxRows) {
        box->addLayout(b.getView());
    }
}

QVBoxLayout * VBox::getView() {
    return box;
}
