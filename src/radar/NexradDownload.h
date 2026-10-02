// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDOWNLOAD_H
#define NEXRADDOWNLOAD_H

#include <string>
#include <vector>
#include "objects/FileStorage.h"

using std::string;
using std::vector;

class NexradDownload {
public:
    static string getRadarFileUrl(const string&, const string&);
    // The newest picture for a radar and product: from Unidata's public S3 bucket of NEXRAD Level III files (super-resolution
    // reflectivity and velocity: N0Q is served as N0B, N0U as N0G) when that source is on and has a recent file, else the NWS
    // server's `sn.last`. Does network work: call it from a worker thread.
    static string latestFileUrl(const string& radarSite, const string& product);
    // the addresses of the newest `count` S3 files of a radar and product, oldest first (empty when there are none)
    static vector<string> s3RecentUrls(const string& radarSite, const string& product, int count);
    // "KTLX" -> "TLX" (how the bucket names a radar) and the product as the bucket carries it
    static string s3Site(const string& radarSite);
    static string s3Product(const string& product);
    static void getRadarFilesForAnimation(int, const string&, const string&, FileStorage *);

private:
    static string getRadarDirectoryUrl(const string&, const string&);
    static const string pattern1;
    static const string pattern2;
};

#endif  // NEXRADDOWNLOAD_H
