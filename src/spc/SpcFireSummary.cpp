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
#include "util/UtilityList.h"

SpcFireSummary::SpcFireSummary(Window * parent)
    : Window{parent}
    , urls{UtilitySpcFireOutlook::urls}
{
    setTitle("SPC Fire Weather Outlooks");
    static const vector<string> names{"Day 1 Fire Weather Outlook", "Day 2 Fire Weather Outlook", "Day 3-8 Fire Weather Outlook"};
    images.reserve(urls.size());
    for (auto index : range(urls.size())) {
        images.emplace_back(this);
        images.back().setNumberAcross(static_cast<int>(urls.size()), getWindowWidth());
        columns.push_back(std::make_unique<VBox>());
        captions.push_back(std::make_unique<Text>(this, names[index]));
        captions.back()->setWordWrap(true);
        static const vector<string> shortNames{"Day 1", "Day 2", "Days 3-8"};
        auto * header = new QLabel{QString::fromStdString(shortNames[index] + ": ..."), this};
        header->setAlignment(Qt::AlignCenter);
        header->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        headers.push_back(header);
        columns.back()->addWidgetReal(header);
        columns.back()->addWidgetReal(images.back().getView());
        columns.back()->addWidget(*captions.back());
        columns.back()->addStretch();
        box.addLayout(*columns.back());
    }
    box.getAndShow(this);
    // what the SPC's fire weather page says above each picture: who issued it, when, and the risk (day 1 and day 2 also go in the title)
    new FutureBytes{this, "https://www.spc.noaa.gov/products/fire_wx/", [this] (const auto& ba) {
        const auto found = UtilitySpcFireOutlook::issuances(QString::fromUtf8(ba).toStdString());
        string title;
        for (size_t i = 0; i < found.size() && i < captions.size(); i++) {
            captions[i]->setText(names[i] + "\n" + found[i].line());
            static const vector<string> shortNames{"Day 1", "Day 2", "Days 3-8"};
            const auto color = UtilitySpcFireOutlook::riskColor(found[i].risk);
            headers[i]->setText(QString::fromStdString(shortNames[i] + ": <b>" + (found[i].risk.empty() ? string{"No risk"} : found[i].risk) + "</b>"));
            headers[i]->setStyleSheet(QString::fromStdString("padding: 3px; color: #000000; background: " + (color.empty() ? string{"#F0F0F0"} : color) + ";"));
            if (i < 2 && !found[i].risk.empty()) {
                title += (title.empty() ? "" : "  |  ") + string{"Day "} + std::to_string(i + 1) + ": " + found[i].risk;
            }
        }
        if (!title.empty()) {
            setTitle("SPC Fire Weather Outlooks - " + title);
        }
    }};
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { OutlookNav::open(this, OutlookNav::FireDay, static_cast<int>(index)); });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); images[index].resizeToWidth(static_cast<float>(width()) / static_cast<float>(images.size()) - 8); }};
    }
    for (auto index : range(urls.size())) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { OutlookNav::open(this, OutlookNav::FireDay, static_cast<int>(index)); });
    }
}

// each picture as wide as its column
void SpcFireSummary::resizeEventCustom() {
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width()) / static_cast<float>(std::max<size_t>(images.size(), 1)) - 8);
    }
}
