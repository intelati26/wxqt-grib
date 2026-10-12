// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "spc/SpcFireWeatherOutlook.h"
#include <QRegularExpression>
#include <QString>
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "spc/UtilitySpcFireOutlook.h"
#include "util/To.h"

namespace {
    const std::vector<std::string> dayLabels{"Day 1", "Day 2", "Days 3-8 (all)", "Day 3", "Day 4", "Day 5", "Day 6", "Day 7", "Day 8"};

    const char * categoricalMeaning = "Day 3-8: the dashed (day 3-8 combined: scalloped) line is a critical threat of dry thunderstorms with dry fuels within 12 miles of a point, "
        "equivalent to a 40% probability. The solid line is a critical threat of strong winds, low relative humidities and warm temperatures together for at least 3 hours with dry fuels "
        "within 12 miles of a point, equivalent to a 70% probability. All days are valid from 12 UTC to 12 UTC.";
    const char * probabilityMeaning = "Probability of dry thunderstorms with dry fuels within 12 miles of a point (scalloped lines): critical area 40% (blue), marginal area 10% (brown). "
        "Probability of strong winds, low RH and warm temperatures together for at least 3 hours with dry fuels within 12 miles of a point (solid lines): critical area 70% (red), "
        "marginal area 40% (orange). All days are valid from 12 UTC to 12 UTC.";

    std::string imageUrl(int index, bool probability) {
        const std::string base = "https://www.spc.noaa.gov/products/exper/fire_wx/imgs/";
        if (index == 0) {
            return UtilitySpcFireOutlook::urls[0];
        }
        if (index == 1) {
            return UtilitySpcFireOutlook::urls[1];
        }
        const std::string day = index == 2 ? "38" : To::string(index);   // 3 .. 8 are list positions 3 .. 8
        return base + "day" + day + (probability ? "fireprob.gif" : "otlk_fire.gif");
    }
}

SpcFireWeatherOutlook::SpcFireWeatherOutlook(Window * parent, int day)
    : Window{parent}
    , comboDay{this, dayLabels}
    , comboGraphic{this, {"Categorical outlook", "Probability"}}
    , header{this}
    , photo{this, Scaled}
    , text{this}
    , sw{this, leftBox, text}
    , shortcutImage{QKeySequence{"1"}, this}
{
    setTitle("SPC Fire Weather Outlooks");
    leftBox.addWidget(comboDay);
    leftBox.addWidget(comboGraphic);
    leftBox.addWidget(header);
    leftBox.addWidget(photo);
    leftBox.addStretch();
    comboDay.setIndex(static_cast<size_t>(std::clamp(day, 0, 2)));
    comboDay.connect([this] { showDay(comboDay.getIndex()); });
    comboGraphic.connect([this] { showDay(comboDay.getIndex()); });
    photo.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    shortcutImage.connect([this, parent] { new ImageViewer{parent, photo.bytes}; });
    // who issued each and when comes from the SPC's fire weather page (the text products carry it too, in a form that is harder to read)
    new FutureBytes{this, "https://www.spc.noaa.gov/products/fire_wx/", [this] (const auto& ba) {
        const auto page = QString::fromUtf8(ba).remove(QRegularExpression{"<[^>]*>"}).replace("&nbsp;", " ").simplified();
        static const QRegularExpression who{"Forecaster:\\s*(\\S+)\\s*Issued:\\s*(\\S+)\\s*Valid:\\s*(\\S+\\s*-\\s*\\S+|\\S+)(?:\\s*Forecast Risk of Fire Weather:\\s*(.+?))?(?=\\s*(?:Current|Day|Fire Weather|Forecaster:)|$)"};
        int i = 0;
        for (auto it = who.globalMatch(page); it.hasNext() && i < 3; i++) {
            const auto m = it.next();
            auto line = "Forecaster " + m.captured(1) + "   Issued " + m.captured(2) + "   Valid " + m.captured(3);
            if (!m.captured(4).isEmpty()) {
                line += "   Risk: " + m.captured(4).trimmed();
            }
            issuance[i] = line.toStdString();
        }
        showDay(comboDay.getIndex());
    }};
    showDay(comboDay.getIndex());
}

void SpcFireWeatherOutlook::showDay(int index) {
    index = std::clamp(index, 0, static_cast<int>(dayLabels.size()) - 1);
    const bool outlook38 = index >= 2;   // days 3-8 have a probability graphic as well, and say what the lines mean
    const bool perDay = index >= 3;   // the combined days 3-8 graphic is one picture (the SPC's categorical and probability files for it are the same)
    comboGraphic.setVisible(perDay);
    const bool probability = perDay && comboGraphic.getIndex() == 1;
    const int mine = ++generation;
    setTitle("SPC Fire Weather Outlook " + dayLabels[static_cast<size_t>(index)] + (probability ? " (probability)" : ""));
    header.setText(issuance[std::min(index, 2)]);
    photo.setBytes(QByteArray{});
    new FutureBytes{this, imageUrl(index, probability), [this, mine] (const auto& ba) {
        if (mine == generation) {
            photo.setBytes(ba);
        }
    }};
    const std::string product = UtilitySpcFireOutlook::products[static_cast<size_t>(std::min(index, 2))];
    const std::string meaning = outlook38 ? std::string{probability ? probabilityMeaning : categoricalMeaning} + "\n\n" : std::string{};
    text.setText(meaning);
    new FutureText{this, product, [this, mine, meaning] (const auto& s) {
        if (mine == generation) {
            text.setText(meaning + s);
        }
    }};
}
