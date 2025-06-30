// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SevereDashboard.h"
#include "misc/UsAlerts.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "objects/PolygonWatch.h"
#include "spc/SpcMcdWatchMpdViewer.h"
#include "spc/SpcStormReports.h"
#include "ui/DividerLine.h"
#include "util/DownloadImage.h"
#include "util/To.h"
#include "util/UtilityList.h"

SevereDashboard::SevereDashboard(Window * parent)
    : Window{parent}
    , autoUpdate{this, "AUTO_UPDATE_INTERVAL_SEVERE_DASHBOARD", 10, [this] { reload(); }}
    , sw{this, box}
    , shortcutAutoUpdate{QKeySequence{"U"}, this}
{
    setTitle("Severe Dashboard");
    severeNotices.insert({Watch, SevereNotice{Watch}});
    severeNotices.insert({Mcd, SevereNotice{Mcd}});
    severeNotices.insert({Mpd, SevereNotice{Mpd}});

    for (auto type : warningTypes) {
        warningsByType.insert({type, SevereWarning{type}});
    }

    box.addLayout(boxH);
    box.addLayout(boxImages);
    box.addLayout(boxWarningsMain);
    boxH.addWidget(autoUpdate);
    for (auto type : warningTypes) {
        boxWarnings.insert({type, VBox()});
        boxWarningsMain.addLayout(boxWarnings.at(type));
    }
    reload();

    shortcutAutoUpdate.connect([this] { autoUpdate.toggleAutoUpdate(); });
}

void SevereDashboard::reload() {
    boxWarningsMain.removeChildren();
    for (const auto type : warningTypes) {
        boxWarnings[type] = VBox();
        boxWarningsMain.addLayout(boxWarnings[type]);
        new FutureVoid{this, [this, type] { warningsByType.at(type).download(); }, [this, type] { updateWarnings(type); }};
    }
    boxImages.removeChildren();
    new FutureVoid{this, [this] { downloadWatch(); }, [this] { updateWatch(); }};
}

void SevereDashboard::downloadWatch() {
    urls.clear();
    urls.push_back(DownloadImage::byProduct("USWARN"));
    urls.push_back(DownloadImage::byProduct("STRPT"));
    for (auto t : {Mcd, Mpd, Watch}) {
        PolygonWatch::byType.at(t)->download();
        severeNotices.at(t).getBitmaps();
        addAll(urls, severeNotices.at(t).urls);
    }
}

void SevereDashboard::updateWatch() {
    mtx.lock();
    boxRows.clear();
    images.clear();
    shortcuts.clear();
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
        images.back().setNumberAcross(imagesAcross, getWindowWidth());
    }
    for (auto index : range(urls.size())) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { launch(index); });
        images[index].connect([this, index] { return launch(index); });
        if (boxRows.size() <= static_cast<size_t>(index / imagesAcross)) {
            boxRows.emplace_back();
        }
        boxRows.back().addWidget(images[index]);
    }
    for (auto& b : boxRows) {
        boxImages.addLayout(b);
    }
    boxImages.addStretch();
    updateTitle();
    for (auto index : range(urls.size())) {
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    mtx.unlock();
}

void SevereDashboard::updateWarnings(PolygonType type) {
    if (warningsByType.at(type).getCountAsInt() > 0) {
        boxWarnings[type].removeChildren();
        headerTextList.emplace_back(this, warningsByType.at(type).getCount() + " " + warningsByType.at(type).getName());
        boxWarnings.at(type).addLayout(headerTextList.back());
        auto d{DividerLine{this}};
        for (const auto& warning : warningsByType.at(type).warningList) {
            if (warning.isCurrent) {
                dashAlertItems.emplace_back(this, warning);
                boxWarnings.at(type).addLayout(dashAlertItems.back());
                boxWarnings.at(type).addWidget(d);
            }
        }
        if (type == Ffw) {
            boxWarnings[type].addStretch();
        }
        updateTitle();
    }
}

void SevereDashboard::launch(size_t indexFinal) {
    if (indexFinal == 0) {
        new UsAlerts{this};
    } else if (indexFinal == 1) {
        new SpcStormReports{this, "today"};
    } else if (indexFinal > 1 && indexFinal < urls.size()) {
        new SpcMcdWatchMpdViewer{this, urls[indexFinal]};
    }
}

void SevereDashboard::updateTitle() {
    string statusTotal;
    for (const auto type : {Mcd, Watch, Mpd}) {
        if (severeNotices.at(type).getCountAsInt() > 0) {
            statusTotal += "  " + severeNotices.at(type).getShortName() + ": " + severeNotices.at(type).getCount();
        }
    }
    for (const auto& type : warningsByType) {
        if (type.second.getCountAsInt() > 0) {
            statusTotal += "  " + type.second.getShortName() + ": " + type.second.getCount();
        }
    }
    setTitle(statusTotal + " " + autoUpdate.titleAdd);
}

void SevereDashboard::closeEventCustom() {
    autoUpdate.stopNoDownload();
}
