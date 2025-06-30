// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef DOWNLOADTIMER_H
#define DOWNLOADTIMER_H

#include <cstdint>
#include <string>

using std::string;

class DownloadTimer {
public:
    explicit DownloadTimer(const string&);
    bool isRefreshNeeded();

private:
    string identifier;
    bool initialized{false};
    int64_t lastRefresh{};
    int radarDataRefreshInterval{};
    int refreshDataInMinutes{};
};

#endif  // DOWNLOADTIMER_H
