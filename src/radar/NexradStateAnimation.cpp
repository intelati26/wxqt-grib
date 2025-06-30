// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradStateAnimation.h"
#include <thread>
#include "util/UtilityList.h"

using std::thread;

void NexradStateAnimation::processAnimationFiles(int frameCount, FileStorage * fileStorage, NexradState * nexradState) {
    levelDataList.clear();
    vector<thread> threads;
    // for ([[maybe_unused]] auto index : range(frameCount)) {
    //     levelDataList.emplace_back(nexradState, fileStorage);
    // }
    // for (auto index : range(frameCount)) {
    //     threads.emplace_back([index, this] { process(index); });
    // }
    // for (auto index : range(frameCount)) {
    //     threads[index].join();
    // }

    for (auto index : range(frameCount)) {
        levelDataList.emplace_back(nexradState, fileStorage);
        threads.emplace_back([index, this] { process(index); });
        threads[index].join();
    }
}

void NexradStateAnimation::process(int index) {
    levelDataList[index].radarBuffers.animationIndex = index;
    levelDataList[index].decode();
    levelDataList[index].radarBuffers.initialize();
    levelDataList[index].generateRadials();
}
