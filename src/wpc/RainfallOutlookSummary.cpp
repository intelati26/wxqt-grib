// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "RainfallOutlookSummary.h"
#include "objects/FutureBytes.h"
#include "misc/OutlookNav.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include "util/To.h"
#include "util/UtilityList.h"
#include "wpc/RainfallOutlook.h"
#include "wpc/UtilityWpcRainfallOutlook.h"

RainfallOutlookSummary::RainfallOutlookSummary(Window * parent)
    : Window{parent}
    , notes{this}
    , urls{UtilityWpcRainfallOutlook::urls}
{
    setTitle("Excessive Rainfall Outlook");
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
    }
    auto * grid = new QGridLayout;
    grid->setSpacing(4);
    for (auto index : range(urls.size())) {
        auto * header = new QLabel{QString::fromStdString("Day " + To::string(index + 1) + ": ..."), this};
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        headers.push_back(header);
        auto * cell = new QVBoxLayout;
        cell->setSpacing(0);
        cell->addWidget(header);
        cell->addWidget(images[index].getView());
        grid->addLayout(cell, static_cast<int>(index) / numberAcross, static_cast<int>(index) % numberAcross);
    }
    box.addLayoutReal(grid);
    notes.setWordWrap(true);
    box.addWidget(notes);
    box.addStretch();
    box.getAndShow(this);
    // what WPC's map service says of each day: its highest category (on WPC's colour for it), when it was issued, and the period it is for
    dayText.assign(urls.size(), "");
    for (auto index : range(urls.size())) {
        new FutureBytes{this, UtilityWpcRainfallOutlook::layerUrl(static_cast<int>(index) + 1), [this, index] (const auto& json) {
            const auto day = UtilityWpcRainfallOutlook::parseLayer(json.toStdString());
            const auto color = UtilityWpcRainfallOutlook::riskColor(day.risk);
            headers[index]->setText(QString::fromStdString("Day " + To::string(index + 1) + ": <b>" + (day.risk.empty() ? string{"No risk"} : day.risk) + "</b>"));
            headers[index]->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + (color.empty() ? string{"#F0F0F0"} : color) + ";"));
            dayText[index] = day.issued.empty() ? "" : "Day " + To::string(index + 1) + ": Issued " + day.issued + "   Valid " + day.valid + "   Risk: " + (day.risk.empty() ? string{"No risk"} : day.risk);
            string text;
            for (const auto& line : dayText) {
                if (!line.empty()) {
                    text += line + "\n";
                }
            }
            notes.setText(text);
        }};
    }
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { OutlookNav::open(this, OutlookNav::RainfallDay, static_cast<int>(index)); });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); images[index].resizeToWidth(static_cast<float>(width()) / numberAcross - 8); }};   // sized when it arrives: the page is made at a child widget's default width, and the resize before the download did nothing
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { OutlookNav::open(this, OutlookNav::RainfallDay, static_cast<int>(index)); });
    }
}

void RainfallOutlookSummary::resizeEventCustom() {
    if (images.size() != urls.size() || urls.empty()) {
        return;   // the window is shown while the cells are still being added
    }
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width()) / numberAcross - 8);
    }
}
