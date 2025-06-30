// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "DownloadParallelBytes.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"

DownloadParallelBytes::DownloadParallelBytes(const vector<string>& urls)
    : urls{urls}
{
    for ([[maybe_unused]] const auto& u : urls) {
        byteList.emplace_back();
    }
    for (auto i : range(urls.size())) {
        threads.emplace_back([this, i] { download(i); });
    }
    for (auto i : range(urls.size())) {
        threads[i].join();
    }
}

void DownloadParallelBytes::download(int i) {
    byteList[i] = UtilityIO::downloadAsByteArray(urls[i]);
}
