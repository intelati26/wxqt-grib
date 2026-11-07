// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilitySpc.h"
#include "common/GlobalVariables.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

string UtilitySpc::categoricalLayerUrl(int day) {
    const int layer = day == 1 ? 1 : (day == 2 ? 9 : 17);
    return "https://mapservices.weather.noaa.gov/vector/rest/services/outlooks/SPC_wx_outlks/MapServer/" + std::to_string(layer) + "/query?where=1%3D1&outFields=issue,valid,expire&returnGeometry=false&f=json";
}

// "202611061556" -> "061556Z"
static string stamp(const QString& yyyymmddhhmm) {
    return yyyymmddhhmm.size() == 12 ? yyyymmddhhmm.mid(6).toStdString() + "Z" : string{};
}

UtilitySpc::Issuance UtilitySpc::parseIssuance(const string& json) {
    Issuance out;
    const auto features = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object().value("features").toArray();
    if (!features.isEmpty()) {
        const auto attributes = features.first().toObject().value("attributes").toObject();
        out.issued = stamp(attributes.value("issue").toString());
        out.valid = stamp(attributes.value("valid").toString());
        out.expire = stamp(attributes.value("expire").toString());
    }
    return out;
}

string UtilitySpc::issuedOfTstormKml(const string& kml) {
    static const QRegularExpression issue{"Issue Time\\s+\\d{8}\\s+(\\d{12})Z"};
    const auto m = issue.match(QString::fromStdString(kml));
    return m.hasMatch() ? stamp(m.captured(1)) : string{};
}
