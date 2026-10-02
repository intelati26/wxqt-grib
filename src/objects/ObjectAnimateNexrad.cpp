// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectAnimateNexrad.h"
#include <memory>
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
    if (timeLine.isRunning() || loading) {
        stopAnimate();
        return;
    }
    frameCount = To::Int(comboboxAnimCount->getValue());
    animationSpeed = To::Int(comboboxAnimSpeed->getValue()) * 500;
    button.setText("Downloading");
    // the frames are downloaded and decoded on worker threads (a loop of super-resolution scans takes seconds), then the
    // loop starts when every pane has its frames
    loading = true;
    const auto thisGeneration = ++generation;
    const auto count = static_cast<int>(frameCount);
    auto remaining = std::make_shared<size_t>(nexradList->size());
    for (auto nw : *nexradList) {
        auto frames = std::make_shared<NexradStateAnimation>();
        const auto product = nw->nexradState.getRadarProduct();
        const auto site = nw->nexradState.getRadarSite();
        const auto end = nw->historyTime();
        nw->runJob(
            [nw, frames, product, site, end, count] {
                FileStorage scratch;
                NexradDownload::getRadarFilesForAnimation(count, product, site, &scratch, end);
                frames->processAnimationFiles(count, &scratch, &nw->nexradState);
            },
            [this, nw, frames, remaining, thisGeneration] {
                if (thisGeneration != generation) {
                    return;   // stopped, or started again, meanwhile
                }
                nw->nexradStateAnimation.levelDataList = std::move(frames->levelDataList);
                *remaining -= 1;
                if (*remaining == 0) {
                    loading = false;
                    timeLine.setSpeed(animationSpeed);
                    timeLine.setCount(frameCount);
                    timeLine.start();
                }
            });
    }
}

// KEEP
// bool ObjectAnimateNexrad::isAnimating() {
//    return timeLine.isRunning();
// }

void ObjectAnimateNexrad::stopAnimate() {
    button.setText("");
    generation += 1;
    loading = false;
    for (auto nw : *nexradList) {
        nw->nexradStateAnimation.levelDataList.clear();
    }
    if (timeLine.isRunning()) {
        timeLine.stop();
        for (auto nw : *nexradList) {
            nw->runJob([nw] { nw->downloadData(); }, [nw] { nw->draw(); });   // back to the newest picture
        }
    }
}

void ObjectAnimateNexrad::stopAnimateNoDownload() {
    button.setText("");
    generation += 1;
    loading = false;
    button.setActive(false);
    for (auto nw : *nexradList) {
        nw->nexradStateAnimation.levelDataList.clear();
    }
    if (timeLine.isRunning()) {
        timeLine.stop();
    }
}

void ObjectAnimateNexrad::downloadFrames() {
    // unused: animateClicked loads the frames on worker threads
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
