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
#ifdef WXQT_WEBENGINE
#include <QWebEngineView>
#endif
#include <QWidget>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class WebViewer : public Widget2 {
public:
    WebViewer(Window *, const string&);
    // the embedded browser, or - in a build without QtWebEngine - a visible
    // note saying so with a link that opens the page externally, so the pane
    // never just sits there blank
    QWidget * getView() override;

private:
    QWidget * view;
    string url;
};

#endif  // WEBVIEWER_H
