// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFSARCHIVEINDEX_H
#define CFSARCHIVEINDEX_H

#include <string>
#include <QByteArray>

// NCEI's monthly files of the CFS analyses (the CFSR of 1979-2010 and the operational analysis since 2011: a file for each variable and month, a message for each 6 hourly analysis) have no
// index. This makes one: the file is read once, every message's header is looked at (when it is for, which field, which level) and the lines are kept for good in the same form as NOAA's
// ("12:5483620:d=1994010100:HGT:500 mb:anl:"), so that the field of any analysis is then one read of its own bytes. A month file is 10 to 60 MB at the low resolution.
namespace CfsArchiveIndex {
    bool handles(const std::string& url);          // the index of one of those files ("...gdas.199401.grb2.idx")
    QByteArray index(const std::string& url);      // its text, empty when the file could not be had
    // the lines of a month file's bytes (no network): the part that can be tested
    std::string scan(const QByteArray& file);
    // the same for a file out on the network, from the head of each message only (the RTMA and URMA analyses of the open data buckets, which have no index before 2017)
    std::string scanRemote(const std::string& fileUrl);
}

#endif  // CFSARCHIVEINDEX_H
