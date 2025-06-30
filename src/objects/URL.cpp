// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/URL.h"
#include <QEventLoop>
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "common/GlobalVariables.h"
#include "util/UtilityLog.h"

string URL::getText(const string& url) {
    UtilityLog::d("getHtml " + url);
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromStdString(GlobalVariables::appName) + " " + QString::fromStdString(GlobalVariables::appCreatorEmail));
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
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl{QString::fromStdString(url)}};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromStdString(GlobalVariables::appName) + " " + QString::fromStdString(GlobalVariables::appCreatorEmail));
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
    QNetworkAccessManager manager;
    QNetworkReply * response = manager.get(QNetworkRequest{QUrl{QString::fromStdString(url)}});
    QEventLoop event;
    QObject::connect(response, &QNetworkReply::finished, &event, &QEventLoop::quit);
    event.exec();
    QByteArray byteArray{response->readAll()};
    delete response;
    return byteArray;
}
