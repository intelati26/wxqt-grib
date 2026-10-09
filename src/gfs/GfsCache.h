// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSCACHE_H
#define GFSCACHE_H

#include <QString>
#include <QtGlobal>

// The disk cache of the model fields (the decoded grids and the GRIB messages they came from), shared by every model screen and kept between runs of the program. A model run's files do not
// change once published, so a field fetched once is good until it is old: at the start of the program anything downloaded more than a number of hours ago (48 by default) is removed, and
// the oldest go first if the folder is over its size limit. The folder carries a format version, so a change in how fields are decoded cannot be served stale ones.
namespace GfsCache {
    constexpr int formatVersion = 3;   // 3: each entry carries the byte range of its record in the file, so a file posted again is noticed
    constexpr int defaultHours = 48;
    constexpr int defaultMegabytes = 2048;
    // the folder (made if it is not there): <cache folder of the app>/gfs-v<formatVersion>
    QString folder();
    // the folder of the given root: for the tests
    QString folderIn(const QString& root);
    // Remove what is older than `hours` (by its time of download), then the oldest until the folder is within `bytes`; also what the earlier versions left behind. Returns the bytes now held.
    qint64 prune(int hours, qint64 bytes, const QString& folderPath = {});
    // the bytes held now
    qint64 usage(const QString& folderPath = {});
    // remove everything
    void clear(const QString& folderPath = {});
}

#endif  // GFSCACHE_H
