// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "DownloadParallel.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

DownloadParallel::DownloadParallel(FileStorage * fileStorage, const vector<string>& urls)
    : fileStorage{fileStorage}
    , urls{urls}
{
    fileStorage->clearBuffers();
    for ([[maybe_unused]] const auto& u : urls) {
        fileStorage->animationMemoryBuffer.emplace_back();
    }
    for (auto i : range(urls.size())) {
        threads.emplace_back([this, i] { download(i); });
    }
    for (auto i : range(urls.size())) {
        threads[i].join();
    }
}

void DownloadParallel::download(int i) {
    fileStorage->setMemoryBufferForAnimation(i, UtilityIO::downloadAsByteArray(urls[i]));
}
