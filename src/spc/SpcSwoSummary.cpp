// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoSummary.h"
#include "objects/FutureBytes.h"
#include "spc/SpcSwoDay1.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoSummary::SpcSwoSummary(Window * parent)
    : Window{parent}
    , nav{this, SpcOutlookNav::SevereSummary}
    , outlookTable{this}
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
    box.addWidget(nav.combo());
    outlookTable.getView()->setTextFormat(Qt::RichText);
    outlookTable.getView()->setOpenExternalLinks(false);
    QObject::connect(outlookTable.getView(), &QLabel::linkActivated, [this] (const QString& link) { new SpcSwoDay1{this, link.toInt()}; });
    box.addWidget(outlookTable);
    showTable();
    // the highest risk of each day, from the tables of the SPC's outlook pages: days 1-3 by their day page (the files carry the issue time), days 4-8 by their own table
    for (int day = 1; day <= 8; day++) {
        const auto take = [this, day] (const QByteArray& ba) {
            const auto row = UtilitySpcSwo::highestRow(UtilitySpcSwo::parseRiskTable(ba.toStdString()));
            const auto label = row.empty() ? string{"No risk"} : row[0];
            const auto color = UtilitySpcSwo::riskColor(label);
            cells[static_cast<size_t>(day - 1)] = "<td align='center' bgcolor='" + (color.empty() ? string{"#F0F0F0"} : color) + "' style='color:#000000'><a href='" + To::string(day) +
                "' style='color:#000000; text-decoration:none'><b>" + label + "</b>" + (row.size() > 2 ? "<br>" + row[2] + " people" : string{}) + "</a></td>";
            showTable();
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
    box.addImageRows(this, urls, images, numberAcross);
    box.addStretch();
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new SpcSwoDay1{this, static_cast<int>(index) + 1}; });
        images[index].setNumberAcross(numberAcross, getWindowWidth());
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    for (auto index : range(urls.size() + 1)) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index))}, this);
        shortcuts.back().connect([this, index] { new SpcSwoDay1{this, static_cast<int>(index)}; });
    }
}

void SpcSwoSummary::showTable() {
    string html = "<table cellspacing='2' cellpadding='6'><tr>";
    for (int day = 1; day <= 8; day++) {
        html += "<th>Day " + To::string(day) + "</th>";
    }
    html += "</tr><tr>";
    for (const auto& cell : cells) {
        html += cell.empty() ? string{"<td align='center'>...</td>"} : cell;
    }
    html += "</tr></table>";
    outlookTable.setText(QString::fromStdString(html));
}
