// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/NetManager.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <QCoreApplication>
#include <QMetaObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include "common/GlobalVariables.h"
#include "objects/KnownIntermediates.h"
#include "util/Activity.h"
#include "util/AppState.h"
#include "util/Utility.h"
#include "util/UtilityLog.h"

namespace {
    using Clock = std::chrono::steady_clock;

    // A caller waiting for an answer
    struct Waiter {
        std::mutex mutex;
        std::condition_variable done;
        bool finished{false};
        NetManager::Result result;
    };

    struct Entry {
        std::string key, url;
        QByteArray range;
        NetManager::Priority priority;
        Clock::time_point asked;
        bool started{false};
        long long received{0}, total{-1};
        std::vector<std::shared_ptr<Waiter>> waiters;
        QNetworkReply * reply{nullptr};
        std::string host;
    };

    // how a host is treated: the gap between starting two requests, and how many may be going at once
    struct Policy {
        long long gapMs;
        int active;
    };
    Policy policyFor(const std::string& host) {
        if (host == "nomads.ncep.noaa.gov") {   // asks for a gap between requests (it answered a tight burst with truncated or failed ones)
            return {300, 4};
        }
        return {0, 8};   // the cloud buckets and ECMWF's server: the manager's own six connections per host are the limit
    }

    std::mutex tableMutex;
    std::map<std::string, std::shared_ptr<Entry>> table;               // everything queued or in flight, by url + range
    std::vector<std::shared_ptr<Entry>> pending;                       // not started
    std::map<std::string, std::pair<int, Clock::time_point>> hosts;    // active requests and the earliest next start
    NetManager::Totals counts;
    std::atomic<bool> stopped{false};
    thread_local NetManager::Priority callerPriority = NetManager::Priority::Visible;

    void finish(const std::shared_ptr<Entry>& entry, NetManager::Result result) {
        for (const auto& w : entry->waiters) {
            const std::lock_guard lock{w->mutex};
            w->result = result;
            w->finished = true;
            w->done.notify_all();
        }
        entry->waiters.clear();
    }

    QString userAgent() {
        const auto email = Utility::readPref("CONTACT_EMAIL", "");
        auto agent = QString::fromStdString(GlobalVariables::appName);
        if (!email.empty()) {
            agent += " " + QString::fromStdString(email);
        }
        return agent;
    }

    // lives on the network thread
    class Pump : public QObject {
    public:
        void kick() {
            if (!manager) {
                manager = new QNetworkAccessManager{this};
            }
            std::vector<std::shared_ptr<Entry>> toStart;
            long long wakeMs = -1;
            {
                const std::lock_guard lock{tableMutex};
                std::stable_sort(pending.begin(), pending.end(), [] (const auto& a, const auto& b) {
                    return a->priority != b->priority ? a->priority < b->priority : a->asked < b->asked;
                });
                const auto now = Clock::now();
                for (auto it = pending.begin(); it != pending.end();) {
                    const auto& entry = *it;
                    auto& host = hosts[entry->host];
                    const auto policy = policyFor(entry->host);
                    if (host.first >= policy.active) {
                        ++it;
                        continue;
                    }
                    if (now < host.second) {   // inside the host's gap: come back when it ends
                        const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(host.second - now).count() + 1;
                        wakeMs = wakeMs < 0 ? wait : std::min<long long>(wakeMs, wait);
                        ++it;
                        continue;
                    }
                    host.first++;
                    host.second = now + std::chrono::milliseconds(policy.gapMs);
                    entry->started = true;
                    toStart.push_back(entry);
                    it = pending.erase(it);
                }
            }
            for (const auto& entry : toStart) {
                start(entry);
            }
            if (wakeMs >= 0) {
                QTimer::singleShot(static_cast<int>(wakeMs), this, [this] { kick(); });
            }
        }
        void abortAll() {
            std::vector<std::shared_ptr<Entry>> live;
            {
                const std::lock_guard lock{tableMutex};
                for (const auto& [key, entry] : table) {
                    live.push_back(entry);
                }
                pending.clear();
            }
            for (const auto& entry : live) {
                if (entry->reply) {
                    entry->reply->abort();   // finished() follows and completes the waiters
                } else {
                    {
                        const std::lock_guard lock{tableMutex};
                        table.erase(entry->key);
                    }
                    finish(entry, {});
                }
            }
        }

    private:
        void start(const std::shared_ptr<Entry>& entry) {
            QNetworkRequest request{QUrl{QString::fromStdString(entry->url)}};
            request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
            request.setTransferTimeout(30000);   // a transfer that makes no progress for 30 s is given up (a large download that keeps coming is fine)
            request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            if (KnownIntermediates::needed(request.url())) {
                request.setSslConfiguration(KnownIntermediates::configuration());
            }
            if (!entry->range.isEmpty()) {
                request.setRawHeader(QByteArray{"Range"}, entry->range);
            }
            auto * reply = manager->get(request);
            entry->reply = reply;
            QObject::connect(reply, &QNetworkReply::downloadProgress, this, [entry] (qint64 got, qint64 total) {
                const std::lock_guard lock{tableMutex};
                entry->received = got;
                entry->total = total;
            });
            QObject::connect(reply, &QNetworkReply::finished, this, [this, entry, reply] {
                NetManager::Result result;
                result.bytes = reply->readAll();
                result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                result.lastModified = reply->header(QNetworkRequest::LastModifiedHeader).toDateTime();
                reply->deleteLater();
                {
                    const std::lock_guard lock{tableMutex};
                    table.erase(entry->key);
                    auto& host = hosts[entry->host];
                    host.first = std::max(0, host.first - 1);
                    counts.requests++;
                    counts.bytes += result.bytes.size();
                }
                entry->reply = nullptr;
                finish(entry, std::move(result));
                kick();   // a slot is free
            });
        }
        QNetworkAccessManager * manager{nullptr};
    };

    QThread * netThread{nullptr};
    Pump * pump{nullptr};
    std::once_flag started;

    void ensureThread() {
        std::call_once(started, [] {
            netThread = new QThread;
            netThread->setObjectName("wxqt-network");
            pump = new Pump;
            pump->moveToThread(netThread);
            netThread->start();
            if (QCoreApplication::instance()) {
                QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [] { NetManager::shutdown(); });
            }
        });
    }

    void kickLater() {
        QMetaObject::invokeMethod(pump, [] { pump->kick(); }, Qt::QueuedConnection);
    }
}

bool NetManager::enabled() {
    if (const auto env = qgetenv("WXQT_NETMANAGER"); !env.isEmpty()) {   // for comparing: WXQT_NETMANAGER=0 is the old path
        return env != "0";
    }
    return Utility::readPref("NETMANAGER", "true").compare(0, 1, "t") == 0;
}

NetManager::Scope::Scope(Priority priority) : before{callerPriority} {
    callerPriority = priority;
}

NetManager::Scope::~Scope() {
    callerPriority = before;
}

NetManager::Result NetManager::get(const std::string& url, const QByteArray& range, Priority priority) {
    if (AppState::quitting || stopped) {
        return {};
    }
    if (priority == Priority::Visible) {
        priority = callerPriority;   // the thread's own say, when it has one
    }
    const Activity::Download counted;   // shows on the screens' activity indicator while the request is waiting or in flight
    ensureThread();
    auto waiter = std::make_shared<Waiter>();
    const std::string key = url + "|" + range.toStdString();
    {
        const std::lock_guard lock{tableMutex};
        const auto found = table.find(key);
        if (found != table.end()) {   // the same request is already queued or going: wait on it (and let it go sooner if this caller needs it sooner)
            found->second->waiters.push_back(waiter);
            if (priority < found->second->priority) {
                found->second->priority = priority;
            }
            counts.reused++;
        } else {
            auto entry = std::make_shared<Entry>();
            entry->key = key;
            entry->url = url;
            entry->range = range;
            entry->priority = priority;
            entry->asked = Clock::now();
            entry->host = QUrl{QString::fromStdString(url)}.host().toStdString();
            entry->waiters.push_back(waiter);
            table[key] = entry;
            pending.push_back(entry);
        }
    }
    kickLater();
    std::unique_lock lock{waiter->mutex};
    while (!waiter->finished) {
        waiter->done.wait_for(lock, std::chrono::milliseconds(500));
        if (AppState::quitting) {   // the program is closing: do not hold it up
            return {};
        }
    }
    return waiter->result;
}

int NetManager::cancelQueued(Priority fromPriority) {
    std::vector<std::shared_ptr<Entry>> dropped;
    {
        const std::lock_guard lock{tableMutex};
        for (auto it = pending.begin(); it != pending.end();) {
            if ((*it)->priority >= fromPriority) {
                dropped.push_back(*it);
                table.erase((*it)->key);
                it = pending.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (const auto& entry : dropped) {
        finish(entry, {});
    }
    return static_cast<int>(dropped.size());
}

std::vector<NetManager::Row> NetManager::snapshot() {
    std::vector<Row> rows;
    const std::lock_guard lock{tableMutex};
    const auto now = Clock::now();
    for (const auto& [key, entry] : table) {
        rows.push_back({entry->url, entry->range.toStdString(), entry->priority, entry->started, entry->received, entry->total,
                        std::chrono::duration<double>(now - entry->asked).count(), static_cast<int>(entry->waiters.size())});
    }
    return rows;
}

NetManager::Totals NetManager::totals() {
    const std::lock_guard lock{tableMutex};
    return counts;
}

void NetManager::shutdown() {
    if (stopped.exchange(true) || !netThread) {
        return;
    }
    QMetaObject::invokeMethod(pump, [] { pump->abortAll(); }, Qt::BlockingQueuedConnection);
    netThread->quit();
    netThread->wait(3000);
}
