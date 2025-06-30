// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/WebViewer.h"
#include <QUrl>

WebViewer::WebViewer(Window * parent, const string& url)
    : webEngine{new QWebEngineView{parent}}
{
    webEngine->load(QUrl{QString::fromStdString(url)});
    webEngine->show();
}

QWebEngineView * WebViewer::getView() {
    return webEngine;
}
