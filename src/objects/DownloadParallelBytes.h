// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef DOWNLOADPARALLELBYTES_H
#define DOWNLOADPARALLELBYTES_H

#include <string>
#include <thread>
#include <vector>
#include <QByteArray>

using std::string;
using std::thread;
using std::vector;

class DownloadParallelBytes {
public:
    explicit DownloadParallelBytes(const vector<string>&);
    vector<QByteArray> byteList;

private:
    void download(int);
    vector<string> urls;
    vector<thread> threads;
};

#endif  // DOWNLOADPARALLELBYTES_H
