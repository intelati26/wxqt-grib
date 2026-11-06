// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpc.h"
#include "common/GlobalVariables.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"
#include <QRegularExpression>
#include <QString>

vector<string> UtilitySpc::getTstormOutlookUrls() {
    const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/enhtstm/");
    const auto imageNames = UtilityString::parseColumn(html, "OnClick.\"show_tab\\(.([0-9]{4}).\\)\".*?");
    vector<string> urls;
    for (const auto& data : imageNames) {
        urls.push_back(GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/enhtstm/imgs/enh_" + data + ".gif");
    }
    return urls;
}

UtilitySpc::TstormPeriod UtilitySpc::parseTstormKml(const string& kml) {
    TstormPeriod out;
    const auto text = QString::fromStdString(kml);
    static const QRegularExpression span{"<begin>\\d{4}-(\\d\\d)-(\\d\\d)T(\\d\\d):\\d\\d:\\d\\dZ</begin>\\s*<end>\\d{4}-(\\d\\d)-(\\d\\d)T(\\d\\d):"};
    if (const auto m = span.match(text); m.hasMatch()) {
        out.valid = (m.captured(2) + "/" + m.captured(3) + "Z-" + m.captured(5) + "/" + m.captured(6) + "Z").toStdString();
    }
    static const QRegularExpression placemark{"<name>\\s*(\\d+)\\s*%\\s*</name>.*?<PolyStyle>.*?<color>([0-9a-fA-F]{8})</color>", QRegularExpression::DotMatchesEverythingOption};
    for (auto it = placemark.globalMatch(text); it.hasNext();) {
        const auto m = it.next();
        if (m.captured(1).toInt() > out.percent) {
            out.percent = m.captured(1).toInt();
            const auto bgr = m.captured(2).mid(2);   // KML colours are aabbggrr
            out.color = ("#" + bgr.mid(4, 2) + bgr.mid(2, 2) + bgr.mid(0, 2)).toStdString();
        }
    }
    return out;
}
