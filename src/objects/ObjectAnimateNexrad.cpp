// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectAnimateNexrad.h"
#include "radar/NexradDownload.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityLog.h"

ObjectAnimateNexrad::ObjectAnimateNexrad(
    Window * parent,
    vector<NexradWidget *> * nexradList,
    ComboBox * comboboxAnimCount,
    ComboBox * comboboxAnimSpeed
)
    : ObjectAnimateParent{parent}
    , nexradList{nexradList}
    , comboboxAnimCount{comboboxAnimCount}
    , comboboxAnimSpeed{comboboxAnimSpeed}
    , timeLine{parent, animationSpeed, frameCount, [this] (int i) { loadAnimationFrame(i); }}
{}

void ObjectAnimateNexrad::animateClicked() {
    if (!timeLine.isRunning()) {
//        button.setActive(true);
        frameCount = To::Int(comboboxAnimCount->getValue());
        animationSpeed = To::Int(comboboxAnimSpeed->getValue()) * 500;
        button.setText("Downloading");
        downloadFrames();
        timeLine.setSpeed(animationSpeed);
        timeLine.setCount(frameCount);
        timeLine.start();
    } else {
        stopAnimate();
    }
}

// KEEP
// bool ObjectAnimateNexrad::isAnimating() {
//    return timeLine.isRunning();
// }

void ObjectAnimateNexrad::stopAnimate() {
    button.setText("");
    for (auto nw : *nexradList) {
        nw->nexradStateAnimation.levelDataList.clear();
    }
    if (timeLine.isRunning()) {
        timeLine.stop();
        for (auto nw : *nexradList) {
            nw->downloadData();
            nw->draw();
            // new FutureVoid{this,
            //     [nw] { nw->downloadData(); },
            //     [nw] { nw->draw(); }};
        }
    }
}

void ObjectAnimateNexrad::stopAnimateNoDownload() {
    button.setText("");
    button.setActive(false);
    for (auto nw : *nexradList) {
        nw->nexradStateAnimation.levelDataList.clear();
    }
    if (timeLine.isRunning()) {
        timeLine.stop();
    }
}

void ObjectAnimateNexrad::downloadFrames() {
    for (auto nw : *nexradList) {
        NexradDownload::getRadarFilesForAnimation(frameCount, nw->nexradState.getRadarProduct(), nw->nexradState.getRadarSite(), &nw->fileStorage);
        nw->nexradStateAnimation.levelDataList.clear();
        nw->nexradStateAnimation.processAnimationFiles(frameCount, &nw->fileStorage, &nw->nexradState);
    }
}

void ObjectAnimateNexrad::loadAnimationFrame(int animationIndex) {
    UtilityLog::d(To::string(animationIndex));
    for (auto nw : *nexradList) {
        nw->downloadDataForAnimation(animationIndex % frameCount);
    }
    button.setText(To::stringPadLeftZeros(To::string(animationIndex + 1), 2) + " / " + To::string(frameCount));
    for (auto nw : *nexradList) {
        nw->draw();
    }
}

void ObjectAnimateNexrad::setAnimationCount() {
    Utility::writePrefInt("NEXRAD_ANIM_FRAME_COUNT2", comboboxAnimCount->getIndex());
}

void ObjectAnimateNexrad::setAnimationSpeed() {
    Utility::writePrefInt("ANIM_INTERVAL", comboboxAnimSpeed->getIndex());
    animationSpeed = To::Int(comboboxAnimSpeed->getValue()) * 500;
    timeLine.setSpeed(animationSpeed);
}
