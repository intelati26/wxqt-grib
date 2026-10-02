// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/UtilityClimate.h"
#include <cstdlib>
#include <map>
#include <sstream>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include "objects/URL.h"

namespace UtilityClimate {
    namespace {
        const string cpc{"https://www.cpc.ncep.noaa.gov"};
        const string cpcData{cpc + "/data/indices/"};
        const string cpcProducts{cpc + "/products/"};
        const string ospo{"https://www.ospo.noaa.gov/data/"};
        const string nhc{"https://www.nhc.noaa.gov/tafb/sst_loop/"};
        const string ensoFigures{cpcProducts + "analysis_monitoring/enso_advisory/"};
        const string intraseasonal{cpcProducts + "intraseasonal/"};

        string contour(const string& region) {
            return ospo + "sst/contour/" + region + ".cf.gif";
        }

        // the middle month of a three-month season (DJF is January, NDJ is December)
        int seasonMonth(const string& seas) {
            static const std::map<string, int> months{{"DJF", 1}, {"JFM", 2}, {"FMA", 3}, {"MAM", 4}, {"AMJ", 5}, {"MJJ", 6},
                                                      {"JJA", 7}, {"JAS", 8}, {"ASO", 9}, {"SON", 10}, {"OND", 11}, {"NDJ", 12}};
            const auto found = months.find(seas);
            return found == months.end() ? 0 : found->second;
        }

        vector<QString> words(const string& line) {
            vector<QString> out;
            for (const auto& part : QString::fromStdString(line).split(QRegularExpression{"\\s+"}, Qt::SkipEmptyParts)) {
                out.push_back(part);
            }
            return out;
        }

        // "SEAS YR [TOTAL] ANOM" (ONI / RONI)
        void parseSeasonal(const string& text, Series& out) {
            std::istringstream in{text};
            string line;
            while (std::getline(in, line)) {
                const auto parts = words(line);
                if (parts.size() < 3) {
                    continue;
                }
                const auto month = seasonMonth(parts[0].toStdString());
                bool okYear = false;
                bool okValue = false;
                const auto year = parts[1].toInt(&okYear);
                const auto value = parts.back().toDouble(&okValue);
                if (month > 0 && okYear && okValue) {
                    out.push_back({year, month, value});
                }
            }
        }

        // "YR MON NINO1+2 ANOM NINO3 ANOM NINO4 ANOM NINO3.4 ANOM": the anomaly is at an odd column
        void parseNino(const string& text, size_t column, Series& out) {
            std::istringstream in{text};
            string line;
            while (std::getline(in, line)) {
                const auto parts = words(line);
                if (parts.size() < 10) {
                    continue;
                }
                bool okYear = false;
                bool okMonth = false;
                bool okValue = false;
                const auto year = parts[0].toInt(&okYear);
                const auto month = parts[1].toInt(&okMonth);
                const auto value = parts[column].toDouble(&okValue);
                if (okYear && okMonth && okValue && month >= 1 && month <= 12) {
                    out.push_back({year, month, value});
                }
            }
        }

        // "YYYY m m m m ..." twelve monthly values to a row (PDO); a value of 99 or more is a missing one
        void parseYearRows(const string& text, Series& out) {
            std::istringstream in{text};
            string line;
            while (std::getline(in, line)) {
                const auto parts = words(line);
                if (parts.size() < 2) {
                    continue;
                }
                bool okYear = false;
                const auto year = parts[0].toInt(&okYear);
                if (!okYear || year < 1800 || year > 2200) {
                    continue;
                }
                for (size_t m = 1; m < parts.size() && m <= 12; m += 1) {
                    bool ok = false;
                    const auto value = parts[m].toDouble(&ok);
                    if (ok && value < 99.0 && value > -99.0) {
                        out.push_back({year, static_cast<int>(m), value});
                    }
                }
            }
        }

        // the SOI file: an anomaly table, then a standardized one, fixed width (the missing months run into each other as -999.9)
        void parseSoi(const string& text, Series& out) {
            const auto start = text.find("STANDARDIZED");
            if (start == string::npos) {
                return;
            }
            std::istringstream in{text.substr(start)};
            string line;
            while (std::getline(in, line)) {
                if (line.size() < 10 || !std::isdigit(static_cast<unsigned char>(line[0]))) {
                    continue;
                }
                const auto year = std::atoi(line.substr(0, 4).c_str());
                for (int m = 0; m < 12; m += 1) {
                    const size_t from = 4 + static_cast<size_t>(m) * 6;
                    if (from + 6 > line.size()) {
                        break;
                    }
                    const auto value = std::atof(line.substr(from, 6).c_str());
                    if (value > -99.0 && value < 99.0) {
                        out.push_back({year, m + 1, value});
                    }
                }
            }
        }

        // "YYYY M value" (NAO, AO, PNA, AAO)
        void parseMonthlyList(const string& text, Series& out) {
            std::istringstream in{text};
            string line;
            while (std::getline(in, line)) {
                const auto parts = words(line);
                if (parts.size() < 3) {
                    continue;
                }
                bool okYear = false;
                bool okMonth = false;
                bool okValue = false;
                const auto year = parts[0].toInt(&okYear);
                const auto month = parts[1].toInt(&okMonth);
                const auto value = parts[2].toDouble(&okValue);
                if (okYear && okMonth && okValue && month >= 1 && month <= 12 && value > -90.0 && value < 90.0) {
                    out.push_back({year, month, value});
                }
            }
        }

        QString decode(QString text) {
            static const std::vector<std::pair<QString, QString>> named{
                {"&ntilde;", QString::fromUtf8("ñ")}, {"&Ntilde;", QString::fromUtf8("Ñ")}, {"&deg;", QString::fromUtf8("°")},
                {"&nbsp;", " "}, {"&amp;", "&"}, {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&ndash;", "-"}, {"&mdash;", "-"}};
            for (const auto& entity : named) {
                text.replace(entity.first, entity.second);
            }
            static const QRegularExpression numeric{"&#(\\d+);"};
            QString out;
            qsizetype last = 0;
            for (auto it = numeric.globalMatch(text); it.hasNext();) {
                const auto match = it.next();
                out += text.mid(last, match.capturedStart() - last);
                out += QChar{static_cast<char16_t>(match.captured(1).toInt())};
                last = match.capturedEnd();
            }
            out += text.mid(last);
            return out;
        }

        string plain(const QString& html) {
            static const QRegularExpression tag{"<[^>]*>"};
            auto text = html;
            text.replace(QRegularExpression{"<\\s*(br|/p)\\s*/?>", QRegularExpression::CaseInsensitiveOption}, "\n");
            text.remove(tag);
            text = decode(text);
            // collapse each line's spaces, keep the paragraph breaks
            QStringList lines;
            for (const auto& line : text.split('\n')) {
                lines << line.simplified();
            }
            auto joined = lines.join("\n");
            joined.replace(QRegularExpression{"\n{3,}"}, "\n\n");
            return joined.trimmed().toStdString();
        }
    }

    const vector<Tile>& tiles() {
        static const vector<Tile> all{
            // sea surface temperature, as it is
            {"Sea surface temperature", "Global (NOAA/NESDIS 5 km blend)", ospo + "cb/sst/sst.daily.current.png"},
            {"Sea surface temperature", "Global, continuous colours (click for the full size)", ospo + "sst/contour/global_small.cf.gif", contour("global")},
            {"Sea surface temperature", "Equatorial Pacific", contour("equatpac")},
            {"Sea surface temperature", "US Atlantic, Gulf and Caribbean", contour("usatlant")},
            {"Sea surface temperature", "Gulf of America", contour("GulfwGulfofAmer")},
            {"Sea surface temperature", "North Atlantic", contour("natlanti")},
            {"Sea surface temperature", "Florida, Bahamas and Cuba", contour("satlanti")},
            {"Sea surface temperature", "Tropical Atlantic (NHC TAFB)", nhc + "14_atl.png"},
            {"Sea surface temperature", "Eastern Pacific (NHC TAFB)", nhc + "14_pac.png"},
            {"Sea surface temperature", "US Pacific", contour("uspacifi")},
            {"Sea surface temperature", "North America", contour("namerica")},
            {"Sea surface temperature", "California", contour("californ")},
            {"Sea surface temperature", "Washington and Oregon", contour("washngtn")},
            {"Sea surface temperature", "Hawaii", contour("hawaii")},
            {"Sea surface temperature", "Gulf of Alaska", contour("alaska")},
            {"Sea surface temperature", "Bering Sea", contour("beringst")},
            {"Sea surface temperature", "Alaska to Hawaii", contour("alashawa")},

            // the departure from normal
            {"Sea surface temperature anomaly", "Global anomaly (NOAA/NESDIS)", ospo + "cb/ssta/ssta.daily.current.png"},
            {"Sea surface temperature anomaly", "Tropical Pacific, weekly temperature and anomaly (CPC)", cpcProducts + "analysis_monitoring/enso_update/sstweek_c.gif"},
            {"Sea surface temperature anomaly", "Niño regions, relative anomalies over the year (CPC)", cpcProducts + "analysis_monitoring/enso_update/ssta_c.gif"},
            {"Sea surface temperature anomaly", "Tropical Atlantic anomaly (NHC TAFB)", nhc + "14_atl_anom.png"},
            {"Sea surface temperature anomaly", "Eastern Pacific anomaly (NHC TAFB)", nhc + "14_pac_anom.png"},

            // marine heat waves and the coral
            {"Marine heat and coral", "HotSpots (SST above the bleaching threshold)", ospo + "cb/hs/hs.daily.current.png"},
            {"Marine heat and coral", "Degree Heating Weeks", ospo + "cb/dhw/dhw.daily.current.png"},
            {"Marine heat and coral", "Bleaching alert area", ospo + "cb/baa/baa.daily.current.png"},

            // the monthly ENSO discussion's figures
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 1: monthly SST anomalies, equatorial Pacific", ensoFigures + "figure01.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 2: Niño 4, 3.4, 3 and 1+2 indices", ensoFigures + "figure02.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 3: equatorial upper-ocean heat (0-300 m)", ensoFigures + "figure03.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 4: subsurface temperature anomalies by depth and longitude", ensoFigures + "figure04.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 5: outgoing longwave radiation (tropical convection)", ensoFigures + "figure05.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 6: official ENSO outlook (RONI)", ensoFigures + "figure06.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 7: ENSO probabilities", ensoFigures + "figure07.gif"},
            {"El Niño / La Niña (CPC diagnostic discussion)", "Fig. 8: ENSO strength probabilities", ensoFigures + "figure08.gif"},

            // the tropical atmosphere that goes with it
            {"Tropical convection (MJO)", "OLR anomalies, last 30 days", cpcProducts + "precip/CWlink/MJO/olra_last30days-3plots.gif"},
            {"Tropical convection (MJO)", "OLR anomalies by longitude, 180 days", intraseasonal + "olr_hov_last180days_2.gif"},
            {"Tropical convection (MJO)", "200 hPa velocity potential anomaly", intraseasonal + "tlon_vpot_web_2.gif"},
            {"Tropical convection (MJO)", "200 hPa heights and wind speed, 5-day", intraseasonal + "200wind_5dintvl_global_2.gif"},
            {"Tropical convection (MJO)", "200 hPa velocity potential, 15-day GFS forecast", cpcProducts + "people/wd52qz/mjo/chi/gfs.gif"},
        };
        return all;
    }

    const vector<IndexInfo>& indices() {
        static const vector<IndexInfo> all{
            {"roni", "RONI: Relative Oceanic Niño Index (3-month)", "°C", cpcData + "RONI.ascii.txt", 0.5},
            {"oni", "ONI: Oceanic Niño Index (3-month)", "°C", cpcData + "oni.ascii.txt", 0.5},
            {"nino34", "Niño 3.4 SST anomaly (monthly)", "°C", cpcData + "sstoi.indices", 0.5},
            {"nino12", "Niño 1+2 SST anomaly (monthly)", "°C", cpcData + "sstoi.indices", 0.0},
            {"nino3", "Niño 3 SST anomaly (monthly)", "°C", cpcData + "sstoi.indices", 0.0},
            {"nino4", "Niño 4 SST anomaly (monthly)", "°C", cpcData + "sstoi.indices", 0.0},
            {"soi", "SOI: Southern Oscillation Index (standardized)", "", cpcData + "soi", 0.0},
            {"pdo", "PDO: Pacific Decadal Oscillation", "", "https://ncei.noaa.gov/pub/data/cmb/ersst/v5/index/ersst.v5.pdo.dat", 0.0},
            {"nao", "NAO: North Atlantic Oscillation (monthly)", "", cpcProducts + "precip/CWlink/pna/norm.nao.monthly.b5001.current.ascii", 0.0},
            {"ao", "AO: Arctic Oscillation (monthly)", "", cpcProducts + "precip/CWlink/daily_ao_index/monthly.ao.index.b50.current.ascii", 0.0},
            {"pna", "PNA: Pacific/North American pattern (monthly)", "", cpcProducts + "precip/CWlink/pna/norm.pna.monthly.b5001.current.ascii", 0.0},
            {"aao", "AAO: Antarctic Oscillation (monthly)", "", cpcProducts + "precip/CWlink/daily_ao_index/aao/monthly.aao.index.b79.current.ascii", 0.0},
        };
        return all;
    }

    bool loadSeries(const IndexInfo& info, Series& out, string& error) {
        out.clear();
        const auto text = URL::getText(info.url);
        if (text.empty() || text.find("<html") != string::npos || text.find("<HTML") != string::npos) {
            error = info.label + " is not available right now.";
            return false;
        }
        if (info.key == "roni" || info.key == "oni") {
            parseSeasonal(text, out);
        } else if (info.key == "nino12") {
            parseNino(text, 3, out);
        } else if (info.key == "nino3") {
            parseNino(text, 5, out);
        } else if (info.key == "nino4") {
            parseNino(text, 7, out);
        } else if (info.key == "nino34") {
            parseNino(text, 9, out);
        } else if (info.key == "soi") {
            parseSoi(text, out);
        } else if (info.key == "pdo") {
            parseYearRows(text, out);
        } else {
            parseMonthlyList(text, out);
        }
        if (out.empty()) {
            error = info.label + " could not be read.";
            return false;
        }
        return true;
    }

    const vector<TextProduct>& textProducts() {
        static const vector<TextProduct> all{
            {"RONI table", cpcData + "RONI.ascii.txt", 60},
            {"ONI table", cpcData + "oni.ascii.txt", 60},
            {"Niño indices", cpcData + "sstoi.indices", 48},
            {"SOI table", cpcData + "soi", 0},
            {"PDO table", "https://ncei.noaa.gov/pub/data/cmb/ersst/v5/index/ersst.v5.pdo.dat", 30},
            {"NAO", cpcProducts + "precip/CWlink/pna/norm.nao.monthly.b5001.current.ascii", 48},
            {"AO", cpcProducts + "precip/CWlink/daily_ao_index/monthly.ao.index.b50.current.ascii", 48},
            {"MJO index (daily)", cpcProducts + "precip/CWlink/daily_mjo_index/proj_norm_order.ascii", 60},
            {"Ocean heat content index", cpcProducts + "analysis_monitoring/ocean/index/heat_content_index.txt", 60},
        };
        return all;
    }

    string tailOf(const string& text, int lines) {
        if (lines <= 0) {
            return text;
        }
        auto end = text.size();
        while (end > 0 && (text[end - 1] == '\n' || text[end - 1] == '\r')) {
            end -= 1;
        }
        int seen = 0;
        size_t pos = end;
        while (pos > 0) {
            pos -= 1;
            if (text[pos] == '\n') {
                seen += 1;
                if (seen == lines) {
                    return text.substr(pos + 1, end - pos - 1);
                }
            }
        }
        return text.substr(0, end);
    }

    bool ensoStatus(EnsoStatus& out, string& error) {
        const auto html = QString::fromStdString(URL::getText(cpcProducts + "analysis_monitoring/enso_advisory/ensodisc.shtml"));
        if (html.isEmpty()) {
            error = "The ENSO diagnostic discussion is not available right now.";
            return false;
        }
        const auto flags = QRegularExpression::DotMatchesEverythingOption | QRegularExpression::CaseInsensitiveOption;
        const auto statusMatch = QRegularExpression{"Alert System Status:(.*?)</a>", flags}.match(html);
        if (statusMatch.hasMatch()) {
            out.status = plain(statusMatch.captured(1));
        }
        const auto issuedMatch = QRegularExpression{
            "(\\d{1,2}\\s+(?:January|February|March|April|May|June|July|August|September|October|November|December)\\s+20\\d\\d)"}.match(html);
        if (issuedMatch.hasMatch()) {
            out.issued = issuedMatch.captured(1).toStdString();
        }
        const auto synopsisMatch = QRegularExpression{"Synopsis:\\s*</u>(?:&nbsp;|\\s)*<strong>(.*?)</strong>", flags}.match(html);
        if (synopsisMatch.hasMatch()) {
            out.synopsis = plain(synopsisMatch.captured(1));
        }
        const auto nextMatch = QRegularExpression{"next ENSO Diagnostics? Discussion is scheduled for (.*?)\\.", flags}.match(html);
        if (nextMatch.hasMatch()) {
            out.next = plain(nextMatch.captured(1));
        }
        const auto from = html.indexOf("Synopsis:");
        const auto to = html.indexOf("This discussion is a consolidated effort");
        if (from >= 0 && to > from) {
            out.discussion = plain(html.mid(from, to - from));
        }
        if (out.status.empty() && out.synopsis.empty()) {
            error = "The ENSO diagnostic discussion could not be read.";
            return false;
        }
        return true;
    }
}
