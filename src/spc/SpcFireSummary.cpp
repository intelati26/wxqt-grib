// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcFireSummary.h"
#include <QRegularExpression>
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
    box.addImageRow(this, urls, images, getWindowWidth());
    box.getAndShow(this);
    for (auto index : range(urls.size())) {
        images[index].connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
        new FutureBytes{this, urls[index], [this, index] (const auto& ba) { images[index].setBytes(ba); }};
    }
    // what the SPC's fire weather page says above each picture: the risk of day 1 and day 2, and when days 3-8 were issued
    new FutureBytes{this, "https://www.spc.noaa.gov/products/fire_wx/", [this] (const auto& ba) {
        const auto page = QString::fromUtf8(ba).remove(QRegularExpression{"<[^>]*>"}).replace("&nbsp;", " ").simplified();
        static const QRegularExpression risk{"Forecast Risk of Fire Weather:\\s*(.+?)(?=\\s*(?:Current|Day|Fire Weather|Forecaster:)|$)"};
        QStringList parts;
        int day = 1;
        for (auto it = risk.globalMatch(page); it.hasNext() && day <= 2; day++) {
            parts << "Day " + QString::number(day) + ": " + it.next().captured(1).trimmed();
        }
        if (!parts.isEmpty()) {
            setTitle("SPC Fire Weather Outlooks - " + parts.join("  |  ").toStdString());
        }
    }};
    for (auto index : range(urls.size())) {
        shortcuts.emplace_back(QKeySequence{QString::fromStdString(To::string(index + 1))}, this);
        shortcuts.back().connect([this, index] { new SpcFireWeatherOutlook{this, static_cast<int>(index)}; });
    }
}
