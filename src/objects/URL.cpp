// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/URL.h"
#include <deque>
#include <map>
#include <mutex>
#include <QDateTime>
#include <QEventLoop>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QUrl>
#include "common/GlobalVariables.h"
#include "util/Utility.h"
#include "util/UtilityLog.h"

namespace {
    // Self-paces bursts of requests to the same host (e.g. NOMADS): found
    // live 2026-09-12 that a tight, unpaced run of 11 sequential byte-range
    // fetches (UtilitySevereIndices, computing SHIP's inputs) intermittently
    // returned truncated data or outright failed - a rate limit, not a code
    // bug. Rather than have every call site invent its own sleep (this file
    // already had two different ad-hoc ones - UtilitySpcPost's own
    // 1000-1500ms pre-download pause, and a new 400-700ms one just added
    // here for the SHIP fetch loop), this is one shared, host-scoped gate
    // every URL:: entry point goes through automatically. 300ms is
    // imperceptible for the ordinary one-request-at-a-time case (most of
    // the app) and only actually delays back-to-back bursts to one host -
    // unrelated hosts are never held up by it. Requests to the same host
    // from parallel threads (DownloadParallelBytes, QtConcurrent-backed
    // FutureBytes/FutureVoid/FutureText) are also real in this codebase, so
    // the shared state is mutex-protected and each caller "reserves" its
    // slot under the lock before sleeping outside it - concurrent callers
    // to one host queue up correctly without stalling callers to others.
    constexpr qint64 minHostIntervalMs = 300;

    void throttleByHost(const string& url) {
        static std::mutex mutex;
        static std::map<string, qint64> lastSlotMs;

        const auto host = QUrl{QString::fromStdString(url)}.host().toStdString();
        qint64 waitMs = 0;
        {
            std::lock_guard<std::mutex> lock{mutex};
            const auto nowMs = QDateTime::currentMSecsSinceEpoch();
            const auto it = lastSlotMs.find(host);
            const auto earliestSlot = (it != lastSlotMs.end()) ? std::max(nowMs, it->second + minHostIntervalMs) : nowMs;
            waitMs = earliestSlot - nowMs;
            lastSlotMs[host] = earliestSlot;
        }
        if (waitMs > 0) {
            QThread::msleep(static_cast<unsigned long>(waitMs));
        }
    }

    // NWS/NOAA API usage guidelines ask for a contactable User-Agent, but
    // that contact address is the user's own choice, not something to bake
    // into the source - empty (just the app name) until they opt in via
    // Settings > General > "Contact email".
    QString userAgent() {
        const auto email = Utility::readPref("CONTACT_EMAIL", "");
        auto agent = QString::fromStdString(GlobalVariables::appName);
        if (!email.empty()) {
            agent += " " + QString::fromStdString(email);
        }
        return agent;
    }

    // NOAA's operational-RRFS S3 bucket mirrors NOMADS's RRFS / RRFS Ensemble /
    // REFS trees with identical relative paths (verified 2026-09-30:
    // rrfs.YYYYMMDD/CC/..., rrfsens.YYYYMMDD/CC/mNNN/..., refs.YYYYMMDD/CC/
    // ensprod/...), so when NOMADS rate-limits (429/403) or is down the same
    // file can be had from AWS. Returns "" for any URL that is not one of
    // those NOMADS trees.
    string mirrorUrl(const string& url) {
        const string nomads = "https://nomads.ncep.noaa.gov/pub/data/nccf/com/";
        if (url.compare(0, nomads.size(), nomads) != 0) {
            return "";
        }
        for (const string tree : {"rrfs/", "refs/"}) {
            const auto prefix = nomads + tree;
            if (url.compare(0, prefix.size(), prefix) != 0) {
                continue;
            }
            // skip the stream directory ("para" / "prod")
            const auto streamEnd = url.find('/', prefix.size());
            if (streamEnd == string::npos) {
                return "";
            }
            return "https://noaa-rrfs-ops-pds.s3.amazonaws.com/" + url.substr(streamEnd + 1);
        }
        return "";
    }

    struct Fetched {
        QByteArray bytes;
        int status{0};   // HTTP status, 0 if the request never got a response
        QDateTime lastModified;   // the Last-Modified header, if any
    };

    Fetched fetchOnce(const string& url, const QByteArray& range, const QByteArray& accept = QByteArray{}) {
        throttleByHost(url);
        QNetworkAccessManager manager;
        QNetworkRequest request{QUrl{QString::fromStdString(url)}};
        request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
        if (!range.isEmpty()) {
            request.setRawHeader(QByteArray{"Range"}, range);
        }
        if (!accept.isEmpty()) {
            request.setRawHeader(QByteArray{"Accept"}, accept);
        }
        QNetworkReply * response = manager.get(request);
        QEventLoop event;
        QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
        event.exec();
        Fetched result;
        result.bytes = response->readAll();
        result.status = response->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.lastModified = response->header(QNetworkRequest::LastModifiedHeader).toDateTime();
        delete response;
        return result;
    }

    // fetchOnce, then the AWS mirror if NOMADS failed (no response, or a 4xx/5xx)
    Fetched fetchWithMirror(const string& url, const QByteArray& range) {
        auto result = fetchOnce(url, range);
        if (result.status == 0 || result.status >= 400) {
            const auto mirror = mirrorUrl(url);
            if (!mirror.empty()) {
                UtilityLog::d("mirror fallback (NOMADS status " + std::to_string(result.status) + ") " + mirror);
                auto alt = fetchOnce(mirror, range);
                if (alt.status > 0 && alt.status < 400) {
                    return alt;
                }
            }
        }
        return result;
    }
}

namespace {
    // hash of the image bytes -> where/when it came from; bounded, newest kept
    using MetaKey = std::pair<qsizetype, size_t>;
    std::mutex metaMutex;
    std::map<MetaKey, URL::Meta> metaTable;
    std::deque<MetaKey> metaOrder;
    constexpr size_t maxMetaEntries = 600;

    MetaKey metaKeyFor(const QByteArray& bytes) {
        return {bytes.size(), static_cast<size_t>(qHash(bytes, 0x5eed))};
    }

    void rememberMeta(const QByteArray& bytes, const string& url, const QDateTime& lastModified) {
        if (bytes.isEmpty()) {
            return;
        }
        const auto key = metaKeyFor(bytes);
        std::lock_guard<std::mutex> lock{metaMutex};
        if (metaTable.find(key) == metaTable.end()) {
            metaOrder.push_back(key);
            while (metaOrder.size() > maxMetaEntries) {
                metaTable.erase(metaOrder.front());
                metaOrder.pop_front();
            }
        }
        metaTable[key] = URL::Meta{url, lastModified, QDateTime::currentDateTimeUtc()};
    }
}

bool URL::metaFor(const QByteArray& bytes, Meta& out) {
    if (bytes.isEmpty()) {
        return false;
    }
    std::lock_guard<std::mutex> lock{metaMutex};
    const auto found = metaTable.find(metaKeyFor(bytes));
    if (found == metaTable.end()) {
        return false;
    }
    out = found->second;
    return true;
}

string URL::getText(const string& url) {
    UtilityLog::d("getHtml " + url);
    const auto fetched = fetchWithMirror(url, QByteArray{});
    return QString{fetched.bytes}.toStdString();
}

string URL::getTextXmlAcceptHeader(const string& url) {
    UtilityLog::d("getHtml XML " + url);
    throttleByHost(url);
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    request.setRawHeader(QByteArray{"Accept"}, QByteArray{"application/atom+xml"});
    QNetworkReply * response = manager.get(request);
    QEventLoop event;
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    QString html{response->readAll()};
    delete response;
    return html.toStdString();
}

QByteArray URL::getBytes(const string& url) {
    UtilityLog::d("getByte " + url);
    const auto fetched = fetchWithMirror(url, QByteArray{});
    if (fetched.status >= 200 && fetched.status < 300) {
        rememberMeta(fetched.bytes, url, fetched.lastModified);
    }
    return fetched.bytes;
}

// HTTP range request - byte range is inclusive; pass end < 0 for "to end of file"
QByteArray URL::getBytesRange(const string& url, long long start, long long end) {
    UtilityLog::d("getByteRange " + url + " " + std::to_string(start) + "-" + std::to_string(end));
    auto range = QByteArray{"bytes="} + QByteArray::number(start) + "-";
    if (end >= 0) {
        range += QByteArray::number(end);
    }
    return fetchWithMirror(url, range).bytes;
}
