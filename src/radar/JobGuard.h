// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef JOBGUARD_H
#define JOBGUARD_H

#include <chrono>
#include <condition_variable>
#include <mutex>

// Keeps an object alive for the background jobs that use it. A job calls enter() before it touches the object (false: the object
// is going away, do nothing) and leave() when it is done; the object's destructor calls closeAndWait(), which refuses new jobs and
// waits for the running ones. Held through a shared_ptr by every job, so it outlives the object.
class JobGuard {
public:
    bool enter() {
        const std::lock_guard<std::mutex> lock{mutex};
        if (closed) {
            return false;
        }
        busy += 1;
        return true;
    }
    void leave() {
        const std::lock_guard<std::mutex> lock{mutex};
        busy -= 1;
        idle.notify_all();
    }
    void closeAndWait(int maxSeconds = 40) {
        std::unique_lock<std::mutex> lock{mutex};
        closed = true;
        idle.wait_for(lock, std::chrono::seconds{maxSeconds}, [this] { return busy == 0; });
    }

private:
    std::mutex mutex;
    std::condition_variable idle;
    int busy{0};
    bool closed{false};
};

#endif  // JOBGUARD_H
