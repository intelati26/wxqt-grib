// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ACTIVITY_H
#define ACTIVITY_H

#include <atomic>
#include <mutex>
#include <string>

// What the app is busy with, for the status indicators on the screens (ui/ActivityLabel): the background tasks (FutureVoid, FutureBytes) that are running and the network requests
// in flight. Counted where the work happens, so every screen is covered without each one reporting.
namespace Activity {
    inline std::atomic<int> tasks{0};            // background jobs running now
    inline std::atomic<int> downloads{0};        // requests in flight now
    inline std::atomic<long> downloadsDone{0};   // requests finished since the app started
    inline std::atomic<int> indexes{0};          // of those in flight, the model files' index (.idx) requests
    inline std::atomic<long> indexesDone{0};     // of those finished, the index requests

    inline bool isIndex(const std::string& url) {
        const auto query = url.find('?');
        const std::string path = url.substr(0, query);
        return path.size() > 4 && path.compare(path.size() - 4, 4, ".idx") == 0;
    }

    // A line a long job puts up for the status indicator while it works ("Reading NCEI's z500 file for 1994-01: 24 MB"): empty when there is none
    inline std::mutex noteMutex;
    inline std::string noteText;
    inline void setNote(const std::string& text) {
        std::lock_guard<std::mutex> guard{noteMutex};
        noteText = text;
    }
    inline std::string note() {
        std::lock_guard<std::mutex> guard{noteMutex};
        return noteText;
    }

    struct Task {
        Task() { tasks++; }
        ~Task() { tasks--; }
        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;
    };
    struct Download {
        const bool index;
        explicit Download(const std::string& url = std::string{}) : index{isIndex(url)} {
            downloads++;
            if (index) {
                indexes++;
            }
        }
        ~Download() {
            downloads--;
            downloadsDone++;
            if (index) {
                indexes--;
                indexesDone++;
            }
        }
        Download(const Download&) = delete;
        Download& operator=(const Download&) = delete;
    };
}

#endif  // ACTIVITY_H
