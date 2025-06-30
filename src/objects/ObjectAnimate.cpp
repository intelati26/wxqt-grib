// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ObjectAnimate.h"
#include "objects/DownloadParallelBytes.h"
#include "util/To.h"
#include "util/UtilityLog.h"

ObjectAnimate::ObjectAnimate(
    Window * parent,
    Photo * photo,
    const function<vector<string>(string, string, int)>& getFunction,
    const function<void()>& downloadFunction
)
    : ObjectAnimateParent(parent)
    , getFunction{getFunction}
    , photo{photo}
    , downloadFunction{downloadFunction}
    , animationSpeed{500}
    , timeLine{parent, animationSpeed * 20, 12, [this] (int i) { loadAnimationFrame(i); }}
{
    frameCount = 12;
}

void ObjectAnimate::stopAnimate() {
    timeLine.stop();
    button.setText("");
}

void ObjectAnimate::stopAnimateNoDownload() {
    timeLine.stop();
    button.setText("");
}

void ObjectAnimate::animateClicked() {
    if (timeLine.isRunning()) {
        timeLine.stop();
        button.setActive(false);
        button.setText("");
        downloadFunction();
    } else {
        button.setActive(true);
        button.setText("Downloading...");
        urls = getFunction(product, sector, frameCount);
        downloadFrames();
        timeLine.setCount(static_cast<int>(urls.size()));
        timeLine.start();
        button.setText("Stop Animation");
    }
}

void ObjectAnimate::loadAnimationFrame(int animationIndex) {
    UtilityLog::d(To::string(animationIndex));
    button.setText(To::string(animationIndex + 1) + " / " + To::string(timeLine.getCount()));
    photo->setBytes(animationFrames[animationIndex % frameCount]);
}

void ObjectAnimate::downloadFrames() {
    animationFrames = DownloadParallelBytes{urls}.byteList;
}
