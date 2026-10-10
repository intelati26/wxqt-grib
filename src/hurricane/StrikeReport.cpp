// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/StrikeReport.h"
#include <cmath>
#include "hurricane/UtilityEnsembleStats.h"
#include "settings/Location.h"

namespace {
    // a colour for how likely: grey for none, yellow, orange, red as the band rises
    QString color(const UtilityWindProbability::Chance& c) {
        if (!c.covered || c.high <= 5) {
            return "#888888";
        }
        return c.low >= 50 ? "#e03030" : c.low >= 20 ? "#e08020" : c.low >= 5 ? "#c0a000" : "#888888";
    }
}

QString StrikeReport::nhcTable(const UtilityWindProbability::Map& map) {
    if (!map.ok || Location::getNumLocations() <= 0) {
        return {};
    }
    QString html = "<table cellspacing='4'><tr><td></td><td><b>34 kt</b></td><td><b>50 kt</b></td><td><b>64 kt</b></td></tr>";
    for (int i = 0; i < Location::getNumLocations(); i++) {
        const auto where = Location::getLatLon(i);
        html += "<tr><td>" + QString::fromStdString(Location::getName(static_cast<size_t>(i))).toHtmlEscaped() + "</td>";
        for (int k = 0; k < 3; k++) {
            const auto chance = UtilityWindProbability::at(map, k, where.lat(), where.lon());
            html += "<td><span style='color:" + color(chance) + "'><b>" + QString::fromStdString(chance.text()).toHtmlEscaped() + "</b></span></td>";
        }
        html += "</tr>";
    }
    return html + "</table>";
}

QString StrikeReport::ensembleLines(const std::vector<HurricaneData::EnsembleSet>& sets, double radiusKm, double minWindKt) {
    QString html;
    for (int i = 0; i < Location::getNumLocations(); i++) {
        const auto where = Location::getLatLon(i);
        QString line;
        for (const auto& set : sets) {
            const auto s = UtilityEnsembleStats::strike(set.storm, where.lat(), where.lon(), radiusKm, minWindKt);
            if (s.members == 0) {
                continue;
            }
            line += QString::fromStdString(set.label) + " " + QString::number(std::lround(100.0 * s.share())) + "% (" + QString::number(s.hits) + "/" + QString::number(s.members) + ")";
            if (UtilityEnsembleStats::missing < s.medianHour && s.hits > 0) {
                line += ", about +" + QString::number(std::lround(s.medianHour)) + " h";
            }
            line += "; ";
        }
        if (!line.isEmpty()) {
            html += "<b>" + QString::fromStdString(Location::getName(static_cast<size_t>(i))).toHtmlEscaped() + "</b>: " + line.left(line.size() - 2) + "<br>";
        }
    }
    return html;
}
