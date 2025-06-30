// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WEBVIEWER_H
#define WEBVIEWER_H

#include <functional>
#include <string>
#include <QPushButton>
#include <QWebEngineView>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class WebViewer : public Widget2 {
public:
    WebViewer(Window *, const string&);
    QWebEngineView * getView();

private:
    QWebEngineView * webEngine;
    string url;
};

#endif  // WEBVIEWER_H
