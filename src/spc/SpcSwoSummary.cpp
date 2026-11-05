// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoSummary.h"
#include <QGridLayout>
#include <QLabel>
#include <QVBoxLayout>
#include "objects/FutureBytes.h"
#include "misc/OutlookNav.h"
#include "spc/SpcSwoDay1.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoSummary::SpcSwoSummary(Window * parent)
    : Window{parent}
    , sw{this, box}
{
    setTitle("SPC Convective Outlooks");
    const auto numberAcross = 4;
    for (const auto& day : day1to3List) {
        urls.push_back(UtilitySpcSwo::getImageUrls(day)[0]);
    }
    for (const auto& day : day4To8List) {
        urls.push_back(UtilitySpcSwo::getImageUrlsDays48(day));
    }
    // one cell for each day: its highest risk on the SPC's colour for it (and the population in it) above the day's picture
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
    }
    auto * grid = new QGridLayout;
    grid->setSpacing(4);
    for (auto index : range(urls.size())) {
        auto * label = new QLabel{QString::fromStdString("Day " + To::string(index + 1) + ": ..."), this};
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        labels.push_back(label);
        auto * cell = new QVBoxLayout;
        cell->setSpacing(0);
        cell->addWidget(label);
        cell->addWidget(images[index].getView());
        grid->addLayout(cell, static_cast<int>(index) / numberAcross, static_cast<int>(index) % numberAcross);
    }
    box.addLayoutReal(grid);
    box.addStretch();
    // the highest risk of each day, from the tables of the SPC's outlook pages: days 1-3 by their day page (the files carry the issue time), days 4-8 by their own table
    for (int day = 1; day <= 8; day++) {
        const auto take = [this, day] (const QByteArray& ba) {
            const auto row = UtilitySpcSwo::highestRow(UtilitySpcSwo::parseRiskTable(ba.toStdString()));
            if (row.empty() && day <= 3) {
                // the tables list the severe risks only: ask the outlook's map service whether the general thunderstorm area is there
                const string layer = To::string(day == 1 ? 1 : (day == 2 ? 9 : 17));
                new FutureBytes{this, "https://mapservices.weather.noaa.gov/vector/rest/services/outlooks/SPC_wx_outlks/MapServer/" + layer + "/query?where=1%3D1&outFields=label&returnGeometry=false&f=json", [this, day] (const auto& json) {
                    if (json.contains("\"TSTM\"")) {
                        auto * label = labels[static_cast<size_t>(day - 1)];
                        label->setText(QString::fromStdString("Day " + To::string(day) + ": <b>TSTM</b>"));
                        label->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + UtilitySpcSwo::riskColor("TSTM") + ";"));
                    }
                }};
            }
            const auto risk = row.empty() ? string{"No risk"} : row[0];
            const auto color = UtilitySpcSwo::riskColor(risk);
            auto * label = labels[static_cast<size_t>(day - 1)];
            label->setText(QString::fromStdString("Day " + To::string(day) + ": <b>" + risk + "</b>" + (row.size() > 2 ? "<br>" + row[2] + " people" : string{})));
            label->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + (color.empty() ? string{"#F0F0F0"} : color) + ";"));
        };
        if (day <= 3) {
            new FutureBytes{this, "https://www.spc.noaa.gov/products/outlook/day" + To::string(day) + "otlk.html", [this, take] (const auto& page) {
                const auto files = UtilitySpcSwo::riskTableFiles(page.toStdString());
                if (!files.empty()) {
                    new FutureBytes{this, "https://www.spc.noaa.gov/products/outlook/" + files.front(), [take] (const auto& ba) { take(ba); }};
                }
            }};
        } else {
            new FutureBytes{this, "https://www.spc.noaa.gov/products/exper/day4-8/ac" + To::string(day) + "_SItable.html", [take] (const auto& ba) { take(ba); }};
        }
    }
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { OutlookNav::open(this, OutlookNav::SevereDay, std::min(static_cast<int>(index) + 1, 4)); });
        images[index].setNumberAcross(numberAcross, getWindowWidth());
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); images[index].resizeToWidth(static_cast<float>(width()) / 4 - 8); }};
    }
    for (auto index : range(urls.size() + 1)) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index))}, this);
        shortcuts.back().connect([this, index] { OutlookNav::open(this, OutlookNav::SevereDay, std::clamp(static_cast<int>(index), 1, 4)); });
    }
}

// each picture as wide as its cell: four across
void SpcSwoSummary::resizeEventCustom() {
    if (images.size() != urls.size() || urls.empty()) {
        return;   // the window is shown while the cells are still being added
    }
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width()) / 4 - 8);
    }
}
