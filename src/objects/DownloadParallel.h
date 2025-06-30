// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef DOWNLOADPARALLEL_H
#define DOWNLOADPARALLEL_H

#include <string>
#include <thread>
#include <vector>
#include "objects/FileStorage.h"

using std::string;
using std::thread;
using std::vector;

class DownloadParallel {
public:
    DownloadParallel(FileStorage *, const vector<string>&);

private:
    void download(int);
    FileStorage * fileStorage;
    vector<string> urls;
    vector<thread> threads;
};

#endif  // DOWNLOADPARALLEL_H
