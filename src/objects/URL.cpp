// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/URL.h"
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
}

string URL::getText(const string& url) {
    UtilityLog::d("getHtml " + url);
    throttleByHost(url);
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    QNetworkReply * response = manager.get(request);
    QEventLoop event;
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    QString data{response->readAll()};
    delete response;
    return data.toStdString();
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
    throttleByHost(url);
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    QNetworkReply * response = manager.get(request);
    QEventLoop event;
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    QByteArray byteArray{response->readAll()};
    delete response;
    return byteArray;
}

// HTTP range request - byte range is inclusive; pass end < 0 for "to end of file"
QByteArray URL::getBytesRange(const string& url, long long start, long long end) {
    UtilityLog::d("getByteRange " + url + " " + std::to_string(start) + "-" + std::to_string(end));
    throttleByHost(url);
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, userAgent());
    auto range = QByteArray{"bytes="} + QByteArray::number(start) + "-";
    if (end >= 0) {
        range += QByteArray::number(end);
    }
    request.setRawHeader(QByteArray{"Range"}, range);
    QNetworkReply * response = manager.get(request);
    QEventLoop event;
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    QByteArray byteArray{response->readAll()};
    delete response;
    return byteArray;
}
