// Tests of the model data cache: what the pruning removes (by age, by size, what a failed download left) and the GRIB file joined from the messages kept.
#include <cstdio>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include "gfs/GfsCache.h"
#include "gfs/GfsData.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

static void make(const QString& folder, const QString& name, int bytes, int hoursOld) {
    QFile f{folder + "/" + name};
    f.open(QIODevice::WriteOnly);
    f.write(QByteArray(bytes, 'x'));
    f.close();
    f.open(QIODevice::ReadWrite);
    f.setFileTime(QDateTime::currentDateTime().addSecs(-static_cast<qint64>(hoursOld) * 3600), QFileDevice::FileModificationTime);
}

int main(int argc, char ** argv) {
    QCoreApplication app{argc, argv};
    QTemporaryDir root;
    const auto folder = GfsCache::folderIn(root.path());
    CHECK(folder.endsWith("/gfs-v" + QString::number(GfsCache::formatVersion)) && QDir{folder}.exists());
    // by age: 48 hours kept, an older one gone, a fresh one kept, and what a download left half done always
    make(folder, "new.gz4", 1000, 1);
    make(folder, "day.gz4", 1000, 24);
    make(folder, "old.gz4", 1000, 49);
    make(folder, "older.grb2", 1000, 200);
    make(folder, "half.tmp", 10, 0);
    make(folder, "x.raw", 10, 0);
    make(folder, "joined.partial", 10, 0);
    CHECK(GfsCache::usage(folder) > 4000);
    const auto held = GfsCache::prune(48, 1000000, folder);
    CHECK(QFile::exists(folder + "/new.gz4") && QFile::exists(folder + "/day.gz4"));
    CHECK(!QFile::exists(folder + "/old.gz4") && !QFile::exists(folder + "/older.grb2"));
    CHECK(!QFile::exists(folder + "/half.tmp") && !QFile::exists(folder + "/x.raw") && !QFile::exists(folder + "/joined.partial"));
    CHECK(held == 2000 && GfsCache::usage(folder) == 2000);
    // by size: over the limit, the oldest download goes first
    make(folder, "a.gz4", 1000, 40);
    make(folder, "b.gz4", 1000, 10);
    CHECK(GfsCache::prune(48, 2500, folder) == 2000);   // 4 files of 1000 bytes, limit 2500: the two oldest (a 40 h, day 24 h) go
    CHECK(!QFile::exists(folder + "/a.gz4") && !QFile::exists(folder + "/day.gz4") && QFile::exists(folder + "/new.gz4") && QFile::exists(folder + "/b.gz4"));
    CHECK(GfsCache::prune(48, 1000000, folder) == 2000);   // within the limits: nothing more goes
    // a shorter time kept
    CHECK(GfsCache::prune(5, 1000000, folder) == 1000 && QFile::exists(folder + "/new.gz4") && !QFile::exists(folder + "/b.gz4"));
    GfsCache::clear(folder);
    CHECK(GfsCache::usage(folder) == 0);
    // the joined file: the messages kept for one run, hour and file, one after the other, and not those of another hour
    GfsData::Config config;
    config.cacheFolder = folder;
    GfsData data{config, GfsData::gfs()};
    const GfsData::Run run{"20261008", "12"};
    make(folder, "GFS_2026100812_f024__HGT_500-mb_24-hour-fcst__p.grb2", 3, 1);
    make(folder, "GFS_2026100812_f024__TMP_500-mb_24-hour-fcst__p.grb2", 5, 1);
    make(folder, "GFS_2026100812_f048__HGT_500-mb_48-hour-fcst__p.grb2", 7, 1);
    const auto joined = data.partialGrib(run, 24, "");
    CHECK(!joined.isEmpty() && QFileInfo{joined}.size() == 8 && joined.endsWith(".partial"));
    CHECK(data.partialGrib(run, 36, "").isEmpty());
    CHECK(GfsCache::prune(48, 1000000, folder) == 15);   // the joined file is not kept: the three messages are
    std::printf(failures ? "%d failures\n" : "all model data cache tests passed\n", failures);
    return failures ? 1 : 0;
}
