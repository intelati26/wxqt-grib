// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityWpcRainfallOutlook.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

const vector<string> UtilityWpcRainfallOutlook::codes{
    "QPF94E",
    "QPF98E",
    "QPF99E",
    "PMDEPD",
    "PMDEPD"
};

const vector<string> UtilityWpcRainfallOutlook::urls{
    "https://www.wpc.ncep.noaa.gov/qpf/94ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/98ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/99ewbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/ero_d45/images/d4wbg.gif",
    "https://www.wpc.ncep.noaa.gov/qpf/ero_d45/images/d5wbg.gif"
};

string UtilityWpcRainfallOutlook::layerUrl(int day) {
    return "https://mapservices.weather.noaa.gov/vector/rest/services/hazards/wpc_precip_hazards/MapServer/" + std::to_string(day - 1) + "/query?where=1%3D1&outFields=outlook,issue_time,valid_time&returnGeometry=false&f=json";
}

UtilityWpcRainfallOutlook::Day UtilityWpcRainfallOutlook::parseLayer(const string& json) {
    static const vector<string> order{"Marginal", "Slight", "Moderate", "High"};
    Day day;
    int best = -1;
    const auto features = QJsonDocument::fromJson(QByteArray::fromStdString(json)).object().value("features").toArray();
    for (const auto& f : features) {
        const auto attributes = f.toObject().value("attributes").toObject();
        const auto outlook = attributes.value("outlook").toString().toStdString();
        for (size_t i = 0; i < order.size(); i++) {
            if (outlook.rfind(order[i], 0) == 0 && static_cast<int>(i) > best) {
                best = static_cast<int>(i);
                day.risk = order[i];
            }
        }
        if (day.issued.empty()) {
            const auto issue = attributes.value("issue_time").toString();   // "2026-11-06 15:53:00"
            if (issue.size() >= 16) {
                day.issued = issue.mid(11, 5).toStdString() + "Z";
            }
            day.valid = attributes.value("valid_time").toString().toStdString();
        }
    }
    return day;
}

string UtilityWpcRainfallOutlook::riskColor(const string& risk) {
    if (risk == "Marginal") {
        return "#7FD97F";
    }
    if (risk == "Slight") {
        return "#FFFF66";
    }
    if (risk == "Moderate") {
        return "#FF6666";
    }
    if (risk == "High") {
        return "#FF66FF";
    }
    return "";
}
