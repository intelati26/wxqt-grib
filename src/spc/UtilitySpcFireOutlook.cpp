// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpcFireOutlook.h"
#include <QRegularExpression>
#include <QString>

string UtilitySpcFireOutlook::Issuance::line() const {
    string out = "Forecaster " + forecaster + "   Issued " + issued + "   Valid " + valid;
    if (!risk.empty()) {
        out += "   Risk: " + risk;
    }
    return out;
}

vector<UtilitySpcFireOutlook::Issuance> UtilitySpcFireOutlook::issuances(const string& pageHtml) {
    const auto page = QString::fromUtf8(pageHtml.data(), static_cast<qsizetype>(pageHtml.size())).remove(QRegularExpression{"<[^>]*>"}).replace("&nbsp;", " ").simplified();
    static const QRegularExpression who{"Forecaster:\\s*(\\S+)\\s*Issued:\\s*(\\S+)\\s*Valid:\\s*(\\S+\\s*-\\s*\\S+|\\S+)(?:\\s*Forecast Risk of Fire Weather:\\s*(.+?))?(?=\\s*(?:Current|Day|Fire Weather|Forecaster:)|$)"};
    vector<Issuance> out;
    for (auto it = who.globalMatch(page); it.hasNext() && out.size() < 3;) {
        const auto m = it.next();
        out.push_back({m.captured(1).toStdString(), m.captured(2).toStdString(), m.captured(3).toStdString(), m.captured(4).trimmed().toStdString()});
    }
    return out;
}

const vector<string> UtilitySpcFireOutlook::products{
    "fwddy1",
    "fwddy2",
    "fwddy38",
};

const vector<string> UtilitySpcFireOutlook::urls{
    "https://www.spc.noaa.gov/products/fire_wx/day1otlk_fire.gif",
    "https://www.spc.noaa.gov/products/fire_wx/day2otlk_fire.gif",
    "https://www.spc.noaa.gov/products/exper/fire_wx/imgs/day38otlk_fire.gif"
};
