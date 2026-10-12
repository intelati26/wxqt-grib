// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcFireSummary.h"
#include <QString>
#include "objects/FutureBytes.h"
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
            if (i < 2 && !found[i].risk.empty()) {
                title += (title.empty() ? "" : "  |  ") + string{"Day "} + std::to_string(i + 1) + ": " + found[i].risk;
            }
        }
        if (!title.empty()) {
            setTitle("SPC Fire Weather Outlooks - " + title);
        }
    }};
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    for (auto index : range(urls.size())) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
    }
}
