// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDOWNLOAD_H
#define NEXRADDOWNLOAD_H

#include <string>
#include "objects/FileStorage.h"

using std::string;

class NexradDownload {
public:
    static string getRadarFileUrl(const string&, const string&);
    static void getRadarFilesForAnimation(int, const string&, const string&, FileStorage *);

private:
    static string getRadarDirectoryUrl(const string&, const string&);
    static const string pattern1;
    static const string pattern2;
};

#endif  // NEXRADDOWNLOAD_H
