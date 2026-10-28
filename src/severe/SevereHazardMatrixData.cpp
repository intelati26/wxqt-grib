// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "severe/SevereHazardMatrixData.h"
#include <future>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "severe/SevereHazardMatrixWidget.h"
#include "util/UtilityIO.h"

namespace {
    using Row = SevereHazardMatrixWidget;

    struct Query {
        int row;
        int day;
        string service;     // under .../vector/rest/services/
        int layer;
        enum Kind { SpcCategory, SpcProbability, Ero, Winter } kind;
    };

    struct Hit {
        bool read{false};   // the service answered
        int dn{0};
        QString label2;     // the service's own wording
        QString valid;
    };

    // every area of the product, no shapes: the highest level anywhere in the country is the cell
    string layerUrl(const Query& q) {
        return "https://mapservices.weather.noaa.gov/vector/rest/services/" + q.service + "/MapServer/" + std::to_string(q.layer) +
               "/query?where=1%3D1&outFields=*&returnGeometry=false&f=json";
    }

    Hit read(const Query& q) {
        Hit hit;
        const auto doc = QJsonDocument::fromJson(UtilityIO::downloadAsByteArray(layerUrl(q)));
        if (!doc.isObject() || !doc.object().contains("features")) {
            return hit;
        }
        hit.read = true;
        for (const auto& value : doc.object().value("features").toArray()) {   // (the highest level of all the areas)
            const auto p = value.toObject().value("attributes").toObject();
            const int dn = p.value("dn").toInt();
            if (dn >= hit.dn) {
                hit.dn = dn;
                hit.label2 = p.contains("label2") && !p.value("label2").isNull() ? p.value("label2").toString() : p.value("outlook").toString();
                hit.valid = p.contains("valid") && !p.value("valid").isNull() ? p.value("valid").toString() : p.value("valid_time").toString();
            }
        }
        return hit;
    }

    QColor readable(const QColor& background) {
        return 0.299 * background.red() + 0.587 * background.green() + 0.114 * background.blue() < 130 ? QColor{Qt::white} : QColor{Qt::black};
    }

    // the fills of the National Weather Service's own outlook map layers: the tornado, the hail and wind, and the days 4-8 outlooks each have their own scale
    QColor probabilityColor(int row, int percent) {
        struct Step { int percent; QColor color; };
        static const std::vector<Step> tornado{{2, {121, 186, 122}}, {5, {189, 153, 138}}, {10, {255, 228, 129}}, {15, {255, 128, 128}}, {30, {255, 128, 255}}, {45, {200, 150, 247}}, {60, {16, 78, 139}}};
        static const std::vector<Step> hailWind{{5, {197, 163, 146}}, {15, {255, 235, 127}}, {30, {255, 127, 127}}, {45, {255, 127, 255}}, {60, {200, 149, 246}}, {75, {92, 133, 214}}, {90, {26, 255, 255}}};
        static const std::vector<Step> later{{15, {255, 255, 0}}, {30, {230, 152, 0}}};
        const auto& steps = row == Row::RowTornado ? tornado : row == Row::RowAnySevere ? later : hailWind;
        QColor out{240, 240, 240};
        for (const auto& step : steps) {
            if (percent >= step.percent) {
                out = step.color;
            }
        }
        return out;
    }

    // the part of "Slight (10-39%)" in the brackets
    QString shortWinter(const QString& outlook, int dn) {
        const int open = outlook.indexOf('(');
        const int close = outlook.indexOf(')');
        if (open >= 0 && close > open) {
            return outlook.mid(open + 1, close - open - 1);
        }
        return dn == 0 ? "<10%" : outlook;
    }
}

std::vector<SevereHazardMatrixData::Cell> SevereHazardMatrixData::fetch(QString& problem) {
    const string spc = "outlooks/SPC_wx_outlks";
    const string ero = "hazards/wpc_precip_hazards";
    const string winter = "precip/wpc_prob_winter_precip";
    std::vector<Query> queries;
    for (int day = 0; day < 2; day++) {   // days 1 and 2: the categories and the three probabilities
        const int base = day * 8;
        queries.push_back({Row::RowAnySevere, day, spc, 1 + base, Query::SpcCategory});
        queries.push_back({Row::RowTornado, day, spc, 3 + base, Query::SpcProbability});
        queries.push_back({Row::RowHail, day, spc, 5 + base, Query::SpcProbability});
        queries.push_back({Row::RowWind, day, spc, 7 + base, Query::SpcProbability});
    }
    queries.push_back({Row::RowAnySevere, 2, spc, 17, Query::SpcCategory});
    for (int day = 3; day < 8; day++) {
        queries.push_back({Row::RowAnySevere, day, spc, 21 + day - 3, Query::SpcProbability});
    }
    for (int day = 0; day < 5; day++) {
        queries.push_back({Row::RowRainfallERO, day, ero, day, Query::Ero});
    }
    for (int day = 0; day < 3; day++) {
        queries.push_back({Row::RowHeavySnow, day, winter, 1 + 5 * day, Query::Winter});   // (at least 4 inches)
        queries.push_back({Row::RowFreezingRain, day, winter, 4 + 5 * day, Query::Winter});   // (ice over a quarter inch)
    }
    std::vector<std::future<Hit>> pending;
    for (const auto& q : queries) {
        pending.push_back(std::async(std::launch::async, [q] { return read(q); }));
    }
    std::vector<Cell> cells;
    int answered = 0;
    for (size_t i = 0; i < queries.size(); i++) {
        const auto& q = queries[i];
        const auto hit = pending[i].get();
        if (!hit.read) {
            continue;
        }
        answered++;
        Cell c{q.row, q.day, QString{}, QColor{255, 255, 255}, QColor{Qt::black}, "No area of this product anywhere in the country."};
        if (hit.dn > 0 || (q.kind == Query::Winter && hit.label2.size() > 0)) {
            switch (q.kind) {
            case Query::SpcCategory: {
                static const char * names[] = {"", "", "TSTM", "MRGL", "SLGT", "ENH", "MDT", "", "HIGH"};   // (the service's values: 2 to 6, and 8 for high)
                c.label = hit.dn >= 2 && hit.dn <= 8 && names[hit.dn][0] != '\0' ? names[hit.dn] : hit.label2;
                c.background = SevereHazardMatrixWidget::getSpcCategoricalColor(c.label);
                break;
            }
            case Query::SpcProbability:
                c.label = QString::number(hit.dn) + "%";
                c.background = probabilityColor(q.row, hit.dn);
                break;
            case Query::Ero: {
                static const char * names[] = {"", "MRGL", "SLGT", "MOD", "HIGH"};
                c.label = hit.dn >= 1 && hit.dn <= 4 ? names[hit.dn] : hit.label2;
                c.background = SevereHazardMatrixWidget::getWpcEroColor(c.label);
                break;
            }
            case Query::Winter: {
                c.label = shortWinter(hit.label2, hit.dn);
                static const QColor colors[] = {QColor{240, 240, 240}, QColor{255, 230, 0}, QColor{255, 140, 0}, QColor{225, 0, 0}, QColor{255, 0, 255}};
                c.background = colors[std::min(hit.dn, 4)];
                break;
            }
            }
            c.text = readable(c.background);
            c.tooltip = "Highest anywhere in the country: " + hit.label2 + (hit.valid.isEmpty() ? QString{} : "\nValid " + hit.valid);
        }
        if (q.kind == Query::Winter && c.label.isEmpty()) {
            continue;
        }
        cells.push_back(c);
    }
    if (answered == 0) {
        problem = "The National Weather Service's map services did not answer.";
    }
    return cells;
}
