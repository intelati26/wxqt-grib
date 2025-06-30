// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "CardSevenDay.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/UtilityLocationFragment.h"

CardSevenDay::CardSevenDay(Window * parent, const string& row1, const string& row2, const string& icon)
    : text1{parent}
    , text2{parent}
    , photo{parent}
{
    boxImage.addMargins();
    // boxText.setSpacing(0);
    text1.setBold();
    text1.setWordWrap(false);
    text2.setGray();

    boxImage.addWidget(photo);
    boxText.addWidget(text1, 0, Qt::AlignTop);
    boxText.addWidget(text2, 0, Qt::AlignTop);

    addLayout(boxImage);
    addLayout(boxText, 1);

    update(row1, row2, icon);
}

void CardSevenDay::update(const string& row1, const string& row2, const string& icon) {
    photo.setNwsIcon(icon);
    text1.setText(WString::replace(row1, "\"", "") + tempAndWind(row2));
    text2.setText(row2);
}

string CardSevenDay::tempAndWind(const string& row2) {
    const auto temperature = UtilityLocationFragment::extractTemp(row2) + GlobalVariables::degreeSymbol;
    const auto windDirection = UtilityLocationFragment::extractWindDirection(row2);
    const auto windSpeed = UtilityLocationFragment::extract7DayMetrics(row2);
    const auto tempAndWind = temperature + " " + windDirection + " " + windSpeed;
    return " (" + WString::strip(tempAndWind) + ")";
}
