// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADDOWNLOAD_H
#define NEXRADDOWNLOAD_H

#include <string>
#include <vector>
#include <QDateTime>
#include "objects/FileStorage.h"

using std::string;
using std::vector;

class NexradDownload {
public:
    static string getRadarFileUrl(const string&, const string&);
    // The newest picture for a radar and product: from Unidata's public S3 bucket of NEXRAD Level III files (super-resolution
    // reflectivity and velocity: N0Q is served as N0B, N0U as N0G) when that source is on and has a recent file, else the NWS
    // server's `sn.last`. Does network work: call it from a worker thread.
    // With `at` set (UTC) it is history: the scan at or before that time from the bucket (no fallback - the NWS server keeps
    // only the newest), or an empty string when the bucket has none.
    static string latestFileUrl(const string& radarSite, const string& product, const QDateTime& at = QDateTime{});
    // the addresses of the newest `count` S3 files of a radar and product, oldest first (empty when there are none); with
    // `end` set, the `count` scans up to that time
    static vector<string> s3RecentUrls(const string& radarSite, const string& product, int count, const QDateTime& end = QDateTime{});
    // the scan times (UTC, oldest first) of a radar and product between two times (at most a few hours: one listing)
    static vector<QDateTime> s3ScanTimes(const string& radarSite, const string& product, const QDateTime& from, const QDateTime& to);
    // "KTLX" -> "TLX" (how the bucket names a radar) and the product as the bucket carries it
    static string s3Site(const string& radarSite);
    static string s3Product(const string& product);
    static void getRadarFilesForAnimation(int, const string&, const string&, FileStorage *, const QDateTime& end = QDateTime{});

private:
    static string getRadarDirectoryUrl(const string&, const string&);
    static const string pattern1;
    static const string pattern2;
};

#endif  // NEXRADDOWNLOAD_H
