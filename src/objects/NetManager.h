// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NETMANAGER_H
#define NETMANAGER_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
#include <QObject>

// One persistent network client for the whole program: a single long-lived QNetworkAccessManager on a thread of its own, so connections (and HTTP/2) are kept between requests instead of a
// new manager, handshake and connection for each one, plus a table of every request that is waiting or in flight.
//   - the same URL and byte range asked for twice at once is one download, with both callers waiting on it
//   - requests have a priority: what the user is looking at goes before the hours read ahead, and those before background work
//   - each host has its own pacing (NOMADS asks for a gap between requests; the cloud buckets do not) and a limit on requests at once
//   - snapshot() lists what is queued and downloading, for a status panel
// get() blocks the calling (worker) thread until the answer is in, as URL::getBytes does; never call it from the thread that runs the user interface's event loop for long.
namespace NetManager {
    enum class Priority { Visible = 0, Ahead = 1, Background = 2 };
    struct Result {
        QByteArray bytes;
        int status{0};              // HTTP status, 0 if the request never got a response
        QDateTime lastModified;
    };
    // range: "" or an HTTP Range header value ("bytes=0-99")
    // accept: "" or an Accept header value. Called on the interface thread, the wait keeps the event loop running (as the old per-request code did); on any other thread it blocks.
    Result get(const std::string& url, const QByteArray& range = {}, Priority priority = Priority::Visible, const QByteArray& accept = {});
    // The priority of the get() calls made on this thread while the object lives (a worker that reads ahead says so once, and everything it asks for goes behind what is on screen)
    class Scope {
    public:
        explicit Scope(Priority priority);
        ~Scope();
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
    private:
        Priority before;
    };
    // Requests that are queued (not yet started) at this priority or lower are dropped, their callers get an empty answer: the view changed and they are for the old one.
    int cancelQueued(Priority fromPriority);
    // A screen that closes: what it still waits for is dropped (queued) or cancelled (downloading), unless another screen or a task with no owner also waits on the same request.
    // Tasks started by the Future* classes say who they are for (util/NetPriority.h); trackOwner() makes the cancel happen when the screen is destroyed.
    int cancelOwner(const void * owner);
    void trackOwner(QObject * owner);

    struct Row {
        std::string url;
        std::string range;
        Priority priority;
        bool started;               // false: waiting for a slot
        long long received;         // bytes so far
        long long total;            // -1 unknown
        double seconds;             // since it was asked for
        int waiters;                // callers waiting on it
        int attempts;               // 0 the first time; a failed request that may pass is tried again, up to twice
    };
    std::vector<Row> snapshot();
    struct Totals {
        long long requests{0};      // finished
        long long reused{0};        // asked for while the same one was already going (saved a download)
        long long retried{0};       // tried again after a failure that may pass (no answer, 429, 502, 503, 504)
        long long cancelled{0};     // dropped because the screen that wanted them closed or the view changed
        long long bytes{0};
    };
    Totals totals();
    // Stops the network thread; what is in flight is cancelled and its callers get an empty answer. Called when the program quits.
    void shutdown();
    bool enabled();                 // the NETMANAGER preference (on by default)
}

#endif  // NETMANAGER_H
