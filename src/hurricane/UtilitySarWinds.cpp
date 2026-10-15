// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilitySarWinds.h"
#include <algorithm>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include "util/UtilityIO.h"

const string UtilitySarWinds::base{"https://www.star.nesdis.noaa.gov/socd/mecb/sar/"};

string UtilitySarWinds::stormUrl(int year, const string& id, const string& name) {
    string upper = id;
    std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    string label = name;
    std::transform(label.begin(), label.end(), label.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return base + "sarwinds_tropical.php?year=" + std::to_string(year) + "&storm=" + upper + "_" + label;
}

namespace {
    QString plain(const QString& html) {
        auto text = html;
        text.remove(QRegularExpression{"<script.*?</script>", QRegularExpression::DotMatchesEverythingOption});
        text.remove(QRegularExpression{"<style.*?</style>", QRegularExpression::DotMatchesEverythingOption});
        text.replace(QRegularExpression{"<[^>]*>"}, "\n");
        text.replace("&nbsp;", " ").replace("&amp;", "&");
        return text;
    }
}

UtilitySarWinds::Schedule UtilitySarWinds::parseSchedule(const string& html) {
    Schedule out;
    const auto lines = plain(QString::fromStdString(html)).split('\n', Qt::SkipEmptyParts);
    int start = -1;
    for (int i = 0; i < lines.size(); i++) {
        if (lines[i].trimmed() == "SENSOR" && start < 0) {
            start = i + 1;
        }
        if (start >= 0 && lines[i].trimmed().startsWith("Last Updated:")) {
            out.updated = lines[i].trimmed().mid(13).trimmed().toStdString();
            break;
        }
    }
    if (start < 0) {
        return out;
    }
    const QRegularExpression when{"^\\d{4}-\\d{2}-\\d{2} \\d{2}:\\d{2}"};
    for (int i = start; i + 3 < lines.size(); i += 4) {
        if (lines[i].trimmed().startsWith("Last Updated:") || !when.match(lines[i + 2].trimmed()).hasMatch()) {
            break;
        }
        out.passes.push_back({lines[i].trimmed().toStdString(), lines[i + 1].trimmed().toStdString(), lines[i + 2].trimmed().toStdString(), lines[i + 3].trimmed().toStdString()});
    }
    return out;
}

vector<UtilitySarWinds::Scene> UtilitySarWinds::parseStorm(const string& htmlText) {
    vector<Scene> out;
    const auto html = QString::fromStdString(htmlText);
    const auto blocks = html.split(QRegularExpression{"<div class\\s*=\\s*\"grid-element\">"}, Qt::SkipEmptyParts);
    const auto first = [] (const QString& block, const QString& pattern) {
        const auto match = QRegularExpression{pattern}.match(block);
        return match.hasMatch() ? match.captured(1) : QString{};
    };
    const auto url = [] (const QString& relative) { return relative.isEmpty() ? string{} : base + relative.toStdString(); };
    for (int b = 1; b < blocks.size(); b++) {   // the first piece is what is before the first card
        const auto& block = blocks[b];
        Scene scene;
        scene.sensor = first(block, "card-header\"[^>]*>([^<]*)<").trimmed().toStdString();
        scene.time = first(block, "card-subtitle[^>]*>([^<]*)<").replace("&nbsp;", " ").trimmed().toStdString();
        scene.wind = url(first(block, "href=\"([^\"]*_wind\\.png)\""));
        if (scene.sensor.empty() || scene.wind.empty()) {
            continue;
        }
        scene.nrcs = url(first(block, "href=\"([^\"]*_nrcs\\.png)\""));
        scene.footprint = url(first(block, "src=\"([^\"]*_map_level3\\.png)\""));
        scene.contours = url(first(block, "src=\"([^\"]*_WSContour_[^\"]*\\.png)\""));
        scene.radialQuads = url(first(block, "src=\"([^\"]*_RadialWinds_4Quads[^\"]*\\.png)\""));
        scene.radialAll = url(first(block, "src=\"([^\"]*_RadialWinds_All[^\"]*\\.png)\""));
        scene.info = url(first(block, "href=\"([^\"]*_info_level3\\.txt)\""));
        auto matches = QRegularExpression{"<li class=\"list-group-item\">((?:Storm Center|Incidence|Quadrant|RMax)[^<]*)"}.globalMatch(block);
        while (matches.hasNext()) {
            const auto text = matches.next().captured(1).trimmed();
            if (text.startsWith("Storm Center Longitude:")) {
                scene.lon = text.mid(23).trimmed().toDouble();
            } else if (text.startsWith("Storm Center Latitude:")) {
                scene.lat = text.mid(22).trimmed().toDouble();
            }
            const auto s = text.toStdString();
            if (std::find(scene.measurements.begin(), scene.measurements.end(), s) == scene.measurements.end()) {
                scene.measurements.push_back(s);
            }
        }
        out.push_back(scene);
    }
    return out;
}

string UtilitySarWinds::storm(const string& id, const string& name, int year, vector<Scene>& out) {
    out.clear();
    if (name.empty() || year < 2015) {
        return "STAR has SAR winds only for named storms of 2015 and later.";
    }
    const auto html = UtilityIO::getHtml(stormUrl(year, id, name));
    if (html.empty()) {
        return "no answer from STAR's SAR winds page";
    }
    out = parseStorm(html);
    return out.empty() ? "STAR has no SAR scenes of this storm" : string{};
}

string UtilitySarWinds::fetchSchedule(Schedule& out) {
    const auto html = UtilityIO::getHtml(base + "sarwinds_tropical.php");
    if (html.empty()) {
        return "no answer from STAR's SAR winds page";
    }
    out = parseSchedule(html);
    return out.passes.empty() ? "no passes are listed right now" : string{};
}
