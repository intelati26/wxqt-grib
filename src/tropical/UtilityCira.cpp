// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/UtilityCira.h"
#include <algorithm>
#include <set>
#include <QRegularExpression>
#include <QString>
#include "objects/URL.h"

namespace UtilityCira {
    namespace {
        const string site{"https://rammb-data.cira.colostate.edu"};
        const string base{site + "/tc_realtime/"};

        string clean(QString text) {
            text.replace("&nbsp;", " ");
            text.replace("&amp;", "&");
            return text.simplified().toStdString();
        }

        // one HTML table as text, a row per line with the cells separated by two spaces
        string tableText(const QString& table) {
            string out;
            static const QRegularExpression row{R"(<tr[^>]*>(.*?)</tr>)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption};
            static const QRegularExpression cell{R"(<t[dh][^>]*>(.*?)</t[dh]>)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption};
            static const QRegularExpression tag{"<[^>]+>"};
            for (auto r = row.globalMatch(table); r.hasNext();) {
                const auto cells = r.next().captured(1);
                QStringList parts;
                for (auto c = cell.globalMatch(cells); c.hasNext();) {
                    auto text = c.next().captured(1);
                    text.remove(tag);
                    parts << QString::fromStdString(clean(text));
                }
                if (!parts.isEmpty()) {
                    out += parts.join("   ").toStdString() + "\n";
                }
            }
            return out;
        }
    }

    const vector<Product>& products() {
        static const vector<Product> list{
            {"4kmirimg", "4 km infrared satellite", true},
            {"mw_89ghz", "89 GHz microwave", true},
            {"mpsatwnd", "Multi-platform satellite winds", false},
            {"diagplot", "SHIPS environment (diagnostic plot)", false},
            {"ohcnfcst", "Ocean heat content", false},
            {"ripafcst", "Rapid intensification probability", false},
        };
        return list;
    }

    const vector<string>& basinOrder() {
        static const vector<string> order{"Atlantic", "Eastern Pacific", "Central Pacific", "Western Pacific", "North Indian Ocean", "Southern Hemisphere"};
        return order;
    }

    string basinOf(const string& id) {
        const auto prefix = id.substr(0, 2);
        if (prefix == "al") return "Atlantic";
        if (prefix == "ep") return "Eastern Pacific";
        if (prefix == "cp") return "Central Pacific";
        if (prefix == "wp") return "Western Pacific";
        if (prefix == "io") return "North Indian Ocean";
        if (prefix == "sh") return "Southern Hemisphere";
        return "Other";
    }

    bool activeStorms(vector<Storm>& out, string& error) {
        out.clear();
        const auto html = QString::fromStdString(URL::getText(base));
        if (html.isEmpty()) {
            error = "the CIRA / RAMMB tropical cyclone page could not be loaded";
            return false;
        }
        static const QRegularExpression link{R"re(href="storm\.asp\?storm_identifier=([a-z0-9]+)"[^>]*>\s*([^<]+))re", QRegularExpression::CaseInsensitiveOption};
        std::set<string> seen;
        for (auto m = link.globalMatch(html); m.hasNext();) {
            const auto match = m.next();
            Storm storm;
            storm.id = match.captured(1).toLower().toStdString();
            if (!seen.insert(storm.id).second) {
                continue;
            }
            storm.title = clean(match.captured(2));
            storm.basin = basinOf(storm.id);
            const auto dash = storm.title.find(" - ");
            if (dash != string::npos) {
                const auto after = QString::fromStdString(storm.title.substr(dash + 3)).trimmed();
                const auto words = after.split(' ', Qt::SkipEmptyParts);
                // "Major Hurricane RACHEL": the name is the trailing upper-case word(s); an invest has none
                QStringList name;
                for (int i = words.size() - 1; i >= 0 && words[i] == words[i].toUpper() && words[i] != "INVEST"; i -= 1) {
                    name.prepend(words[i]);
                }
                storm.name = name.join(' ').toStdString();
            }
            out.push_back(storm);
        }
        const auto& order = basinOrder();
        std::stable_sort(out.begin(), out.end(), [&order] (const Storm& a, const Storm& b) {
            return (std::find(order.begin(), order.end(), a.basin) - order.begin()) < (std::find(order.begin(), order.end(), b.basin) - order.begin());
        });
        return true;
    }

    bool stormPage(const string& id, StormPage& out, string& error) {
        out = StormPage{};
        const auto html = QString::fromStdString(URL::getText(base + "storm.asp?storm_identifier=" + id));
        if (html.isEmpty()) {
            error = "no CIRA / RAMMB page for " + id;
            return false;
        }
        static const QRegularExpression file{R"re(/tc_realtime/products/storms/[0-9a-z]+/([a-z0-9_]+)/([A-Za-z0-9_.]+\.(?:gif|png|txt)))re"};
        for (auto m = file.globalMatch(html); m.hasNext();) {
            const auto match = m.next();
            const auto key = match.captured(1).toStdString();
            const auto path = site + match.captured(0).toStdString();
            if (key == "ripastbl") {
                if (out.rapidIntensificationTableUrl.empty()) out.rapidIntensificationTableUrl = path;
            } else if (!out.imageUrl.count(key) && match.captured(2).endsWith("gif", Qt::CaseInsensitive) + match.captured(2).endsWith("png", Qt::CaseInsensitive) > 0) {
                out.imageUrl[key] = path;
            }
        }
        static const QRegularExpression issued{R"(Time of Latest Forecast:\s*([0-9\-]+ [0-9:]+))"};
        const auto issuedMatch = issued.match(html);
        if (issuedMatch.hasMatch()) {
            out.forecastTime = issuedMatch.captured(1).toStdString();
        }
        // the first table is the forecast track, the next the track history
        static const QRegularExpression table{R"(<table[^>]*>(.*?)</table>)", QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption};
        vector<string> tables;
        for (auto m = table.globalMatch(html); m.hasNext();) {
            const auto text = tableText(m.next().captured(1));
            if (text.find("Latitude") != string::npos) {
                tables.push_back(text);
            }
        }
        if (!tables.empty()) out.forecastTrack = tables[0];
        if (tables.size() > 1) out.trackHistory = tables[1];
        return true;
    }

    vector<string> frameUrls(const string& id, const string& product, int count) {
        const auto html = QString::fromStdString(URL::getText(base + "archive.asp?product=" + product + "&storm_identifier=" + id));
        const QRegularExpression file{QString{R"re(/tc_realtime/products/storms/[0-9a-z]+/%1/[A-Za-z0-9_.]+\.(?:gif|png))re"}.arg(QString::fromStdString(product))};
        std::set<string> unique;   // file names carry the time, so the sorted set is oldest first
        for (auto m = file.globalMatch(html); m.hasNext();) {
            unique.insert(m.next().captured(0).toStdString());
        }
        vector<string> all{unique.begin(), unique.end()};
        const auto n = static_cast<size_t>(std::max(count, 1));
        if (all.size() > n) {
            all.erase(all.begin(), all.end() - static_cast<long>(n));
        }
        for (auto& path : all) {
            path = site + path;
        }
        return all;
    }
}
