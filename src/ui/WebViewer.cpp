// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/WebViewer.h"
#include <QFrame>
#include <QLabel>
#include <QUrl>

WebViewer::WebViewer(Window * parent, const string& url)
    : view{nullptr}
    , url{url}
{
#ifdef WXQT_WEBENGINE
    auto * webEngine = new QWebEngineView{parent};
    webEngine->load(QUrl{QString::fromStdString(url)});
    webEngine->show();
    view = webEngine;
#else
    // Deliberately not silent: this build was made without the embedded
    // browser (it is most of the download size), so say that and give the
    // link instead of showing an empty box.
    auto * note = new QLabel{parent};
    note->setWordWrap(true);
    note->setTextFormat(Qt::RichText);
    note->setOpenExternalLinks(true);
    note->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    note->setFrameStyle(QFrame::StyledPanel);
    const auto link = QString::fromStdString(url).toHtmlEscaped();
    note->setText("<p><b>Embedded web preview is not included in this build.</b></p>"
                  "<p>This version leaves out the built-in browser to keep the download small. "
                  "<a href=\"" + link + "\">Open the page in your web browser</a>.</p>");
    view = note;
#endif
}

QWidget * WebViewer::getView() {
    return view;
}
