// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/RadarStatusBox.h"
#include "objects/WString.h"
#include "radar/NexradUtil.h"
#include "util/To.h"

RadarStatusBox::RadarStatusBox(Window * parent)
    : parent{parent}
    , label{new ClickableLabel{parent}}
{}

void RadarStatusBox::setBox(const NexradLevelData& levelData, const string& product, const string& radarSite) {
    const auto radarAgeString = "Radar age: " + To::string(static_cast<int>(levelData.radarAgeMilli / 60000.0)) + " min";
    const auto status = " / " + WString::split(levelData.radarInfo, " ")[0];
    const auto fullStatus = radarSite + "/" + product + " " + radarAgeString + status;
    if (NexradUtil::isRadarTimeOld(levelData.radarAgeMilli)) {
        setOld(fullStatus);
    } else {
        setCurrent(fullStatus);
    }
}

void RadarStatusBox::setCurrent(const string& s) {
    setText(s);
    setBackGroundGreen();
}

void RadarStatusBox::setOld(const string& s) {
    setText(s);
    setBackGroundRed();
}

void RadarStatusBox::setText(const string& s) {
    label->setText(QString::fromStdString(s));
}

void RadarStatusBox::setBackGroundRed() {
    label->setStyleSheet("QLabel { background-color : red ; color : white; }");
}

void RadarStatusBox::setBackGroundGreen() {
    label->setStyleSheet("QLabel { background-color : green ; color : white; }");
}

void RadarStatusBox::connect(const function<void()>& fn) {
    QObject::connect(label, &ClickableLabel::clicked, parent, fn);
}

ClickableLabel * RadarStatusBox::getView() {
    return label;
}
