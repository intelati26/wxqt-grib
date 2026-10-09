// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/GfsCache.h"
#include <algorithm>
#include <vector>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

QString GfsCache::folderIn(const QString& root) {
    const auto path = root + "/gfs-v" + QString::number(formatVersion);
    QDir{}.mkpath(path);
    return path;
}

QString GfsCache::folder() {
    return folderIn(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
}

namespace {
    struct Entry {
        QString path;
        qint64 size;
        QDateTime time;
    };

    std::vector<Entry> entries(const QString& folder) {
        std::vector<Entry> out;
        const QDir dir{folder};
        for (const auto& info : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
            out.push_back({info.absoluteFilePath(), info.size(), info.lastModified()});   // each file is written once when downloaded: its time is the download's
        }
        return out;
    }
}

qint64 GfsCache::usage(const QString& folderPath) {
    qint64 total = 0;
    for (const auto& e : entries(folderPath.isEmpty() ? folder() : folderPath)) {
        total += e.size;
    }
    return total;
}

void GfsCache::clear(const QString& folderPath) {
    for (const auto& e : entries(folderPath.isEmpty() ? folder() : folderPath)) {
        QFile::remove(e.path);
    }
}

qint64 GfsCache::prune(int hours, qint64 bytes, const QString& folderPath) {
    const auto path = folderPath.isEmpty() ? folder() : folderPath;
    if (folderPath.isEmpty()) {   // what the earlier versions left: the first cache folder, and the temporary folders of the screens that were open when the program ended badly
        QDir{QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/gfs"}.removeRecursively();
        const QDir temp{QStandardPaths::writableLocation(QStandardPaths::TempLocation)};
        for (const auto& old : temp.entryList({"wxqt_gfs_session_*"}, QDir::Dirs | QDir::NoDotAndDotDot)) {
            QDir{temp.filePath(old)}.removeRecursively();
        }
    }
    auto held = entries(path);
    const auto cutoff = QDateTime::currentDateTime().addSecs(-static_cast<qint64>(std::max(hours, 1)) * 3600);
    qint64 total = 0;
    std::vector<Entry> kept;
    for (const auto& e : held) {
        if (e.time < cutoff || e.path.endsWith(".tmp") || e.path.endsWith(".raw") || e.path.endsWith(".hdr") || e.path.endsWith(".partial")) {   // old, or left from a download that did not finish, or made to be read once
            QFile::remove(e.path);
        } else {
            kept.push_back(e);
            total += e.size;
        }
    }
    if (total > bytes) {   // over the limit: the oldest downloads go first
        std::sort(kept.begin(), kept.end(), [] (const Entry& a, const Entry& b) { return a.time < b.time; });
        for (const auto& e : kept) {
            if (total <= bytes) {
                break;
            }
            QFile::remove(e.path);
            total -= e.size;
        }
    }
    return total;
}
