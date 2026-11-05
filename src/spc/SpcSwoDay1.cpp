// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcSwoDay1.h"
#include <memory>
#include "ui/UiStandards.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "spc/SpcSwoStateGraphics.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"
#include "util/UtilityList.h"

SpcSwoDay1::SpcSwoDay1(Window * parent, int day)
    : Window{parent}
    , nav{this, SpcOutlookNav::SevereDay, day}
    , sw{this, imageVBox, boxText}
    , text{this}
    , riskTables{this}
    , button{this, Icon::None, "State graphics", "The state graphics"}
    , launchStateShortcut{QKeySequence{"S"}, this}
{
    setTitle("SPC Convective Outlook Day " + To::string(day));
    button.connect([this, day] { new SpcSwoStateGraphics{this, day}; });
    boxText.addWidget(nav.combo());
    boxText.addWidget(button);
    boxText.addWidget(riskTables);
    boxText.addWidget(text);
    boxText.addStretch();
    if (day > 3) {
        urls = UtilitySpcSwo::getImageUrls(48);
    } else {
        urls = UtilitySpcSwo::getImageUrls(day);
    }
    for ([[maybe_unused]] auto index : range(urls.size())) {
        images.emplace_back(this);
        images.back().imageSize = UiStandards::tileImage;
        imageVBox.addWidget(images.back());
    }
    imageVBox.addStretch();

    if (day >= 1 && day <= 3) {
        riskTables.setWordWrap(true);
        riskTables.getView()->setTextFormat(Qt::RichText);
        // the day's page names the table files (they carry the issue time); load each and put them above the discussion, with the highest risk in the title
        new FutureBytes{this, "https://www.spc.noaa.gov/products/outlook/day" + To::string(day) + "otlk.html", [this, day] (const auto& page) {
            const auto files = UtilitySpcSwo::riskTableFiles(page.toStdString());
            auto tables = std::make_shared<vector<SpcRiskTable>>(files.size());
            auto remaining = std::make_shared<size_t>(files.size());
            for (size_t i = 0; i < files.size(); i++) {
                new FutureBytes{this, "https://www.spc.noaa.gov/products/outlook/" + files[i], [this, day, i, tables, remaining] (const auto& ba) {
                    (*tables)[i] = UtilitySpcSwo::parseRiskTable(ba.toStdString());
                    if (--*remaining > 0) {
                        return;
                    }
                    string html;
                    for (const auto& table : *tables) {
                        if (!table.columns.empty()) {
                            html += UtilitySpcSwo::riskTableHtml(table) + "<br>";
                        }
                    }
                    riskTables.setText(QString::fromStdString(html));
                    const auto highest = tables->empty() ? string{} : UtilitySpcSwo::highestRisk(tables->front());
                    setTitle("SPC Convective Outlook Day " + To::string(day) + (highest.empty() ? "" : " - highest risk: " + highest));
                }};
            }
        }};
    }
    auto product = "SWODY" + To::string(day);
    if (day > 3) {
        product = "SWOD48";
    }
    new FutureText{this, product, [this] (const auto& s) { text.setText(s); }};
    for (auto index : range(urls.size())) {
        images[index].connect([this, parent, index] { new ImageViewer{parent, images[index].bytes}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};

        shortcuts.push_back(Shortcut{QKeySequence{QString::fromStdString(To::string(index + 1))}, this});
        shortcuts.back().connect([this, parent, index] { new ImageViewer{parent, images[index].bytes}; });
    }

    launchStateShortcut.connect([this, day] { new SpcSwoStateGraphics(this, day); });
}
