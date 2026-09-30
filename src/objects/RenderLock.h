// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef RENDERLOCK_H
#define RENDERLOCK_H

#include <map>
#include <memory>
#include <mutex>
#include <QString>

// Serialises concurrent renders of the SAME output image within this process.
// Every render pipeline names its scratch files after the request (run/field/
// region/hour) and deletes them when done, so two threads rendering the same
// image at once - e.g. the initial reload() still running when Play starts a
// sweep over the same hour - delete each other's inputs mid-run and one dies
// with "ERROR 4: ...tif: No such file or directory" (reproduced on Linux with
// two threads; far easier to hit on Windows, where GDAL start-up is slower).
// Hold one of these for the whole render, keyed by the final PNG path: the
// second caller waits, then finds the finished PNG in the cache and returns
// it instead of redoing the work. Different images never block each other.
class RenderLock {
public:
    explicit RenderLock(const QString& key)
        : entry{acquire(key)}
        , key{key}
    {
        entry->lock();
    }

    ~RenderLock() {
        entry->unlock();
        release(key);
    }

    RenderLock(const RenderLock&) = delete;
    RenderLock& operator=(const RenderLock&) = delete;

private:
    struct Slot {
        std::shared_ptr<std::mutex> mutex;
        int users{0};
    };

    static std::mutex& tableMutex() {
        static std::mutex instance;
        return instance;
    }

    static std::map<QString, Slot>& table() {
        static std::map<QString, Slot> instance;
        return instance;
    }

    static std::shared_ptr<std::mutex> acquire(const QString& key) {
        std::lock_guard<std::mutex> guard{tableMutex()};
        auto& slot = table()[key];
        if (!slot.mutex) {
            slot.mutex = std::make_shared<std::mutex>();
        }
        slot.users += 1;
        return slot.mutex;
    }

    static void release(const QString& key) {
        std::lock_guard<std::mutex> guard{tableMutex()};
        const auto found = table().find(key);
        if (found != table().end() && --found->second.users <= 0) {
            table().erase(found);
        }
    }

    std::shared_ptr<std::mutex> entry;
    QString key;
};

#endif  // RENDERLOCK_H
