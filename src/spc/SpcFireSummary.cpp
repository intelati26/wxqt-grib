// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcFireSummary.h"
#include <QString>
#include "objects/FutureBytes.h"
#include "misc/OutlookNav.h"
#include "spc/SpcFireWeatherOutlook.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "util/To.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include "util/UtilityList.h"

SpcFireSummary::SpcFireSummary(Window * parent)
    : Window{parent}
    , notes{this}
{
    setTitle("SPC Fire Weather Outlooks");
    const int across = 4;
    urls = {UtilitySpcFireOutlook::urls[0], UtilitySpcFireOutlook::urls[1]};
    for (int day = 3; day <= 8; day++) {
        urls.push_back("https://www.spc.noaa.gov/products/exper/fire_wx/imgs/day" + To::string(day) + "otlk_fire.gif");
    }
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
    }
    auto * grid = new QGridLayout;
    grid->setSpacing(4);
    for (auto index : range(urls.size())) {
        auto * header = new QLabel{QString::fromStdString("Day " + To::string(index + 1)), this};
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        headers.push_back(header);
        auto * cell = new QVBoxLayout;
        cell->setSpacing(0);
        cell->addWidget(header);
        cell->addWidget(images[index].getView());
        grid->addLayout(cell, static_cast<int>(index) / across, static_cast<int>(index) % across);
    }
    box.addLayoutReal(grid);
    notes.setWordWrap(true);
    box.addWidget(notes);
    box.addStretch();
    box.getAndShow(this);
    // what the SPC's fire weather page says above each picture: who issued it, when, and the risk (day 1 and day 2 also go in the title)
    new FutureBytes{this, "https://www.spc.noaa.gov/products/fire_wx/", [this] (const auto& ba) {
        const auto found = UtilitySpcFireOutlook::issuances(QString::fromUtf8(ba).toStdString());
        static const vector<string> names{"Day 1", "Day 2", "Days 3-8"};
        string title;
        string text;
        for (size_t i = 0; i < found.size() && i < names.size(); i++) {
            text += names[i] + ": " + found[i].line() + "\n";
            if (i < 2) {
                const auto color = UtilitySpcFireOutlook::riskColor(found[i].risk);
                headers[i]->setText(QString::fromStdString(names[i] + ": <b>" + (found[i].risk.empty() ? string{"No risk"} : found[i].risk) + "</b>"));
                headers[i]->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + (color.empty() ? string{"#F0F0F0"} : color) + ";"));
                if (!found[i].risk.empty()) {
                    title += (title.empty() ? "" : "  |  ") + string{"Day "} + std::to_string(i + 1) + ": " + found[i].risk;
                }
            }
        }
        notes.setText(text);
        if (!title.empty()) {
            setTitle("SPC Fire Weather Outlooks - " + title);
        }
    }};
    // the fire weather screen lists the days as: day 1, day 2, days 3-8 together, then day 3 .. day 8 (so a picture of day 3 or later is its day + 1)
    const auto pageOf = [] (size_t index) { return index < 2 ? static_cast<int>(index) : static_cast<int>(index) + 1; };
    for (auto index : range(urls.size())) {
        images[index].connect([this, index, pageOf] { OutlookNav::open(this, OutlookNav::FireDay, pageOf(index)); });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); images[index].resizeToWidth(static_cast<float>(width()) / 4 - 8); }};
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index, pageOf] { OutlookNav::open(this, OutlookNav::FireDay, pageOf(index)); });
    }
}

// each picture as wide as its cell: four across
void SpcFireSummary::resizeEventCustom() {
    if (images.size() != urls.size() || urls.empty()) {
        return;
    }
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width()) / 4 - 8);
    }
}
