// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/UtilityJma.h"
#include <map>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include "objects/URL.h"

namespace UtilityJma {
    namespace {
        const string base{"https://www.jma.go.jp/bosai/typhoon/data/"};

        string translate(const QString& japanese, const std::map<QString, QString>& table) {
            const auto found = table.find(japanese);
            return found == table.end() ? japanese.toStdString() : found->second.toStdString();
        }

        // 16-point compass words ("北北西") and the four-point ones used for the wind areas
        string direction(const QString& text) {
            static const std::map<QString, QString> table{
                {QString::fromUtf8("北"), "N"}, {QString::fromUtf8("北北東"), "NNE"}, {QString::fromUtf8("北東"), "NE"},
                {QString::fromUtf8("東北東"), "ENE"}, {QString::fromUtf8("東"), "E"}, {QString::fromUtf8("東南東"), "ESE"},
                {QString::fromUtf8("南東"), "SE"}, {QString::fromUtf8("南南東"), "SSE"}, {QString::fromUtf8("南"), "S"},
                {QString::fromUtf8("南南西"), "SSW"}, {QString::fromUtf8("南西"), "SW"}, {QString::fromUtf8("西南西"), "WSW"},
                {QString::fromUtf8("西"), "W"}, {QString::fromUtf8("西北西"), "WNW"}, {QString::fromUtf8("北西"), "NW"},
                {QString::fromUtf8("北北西"), "NNW"}};
            return translate(text, table);
        }

        string intensityWord(const QString& text) {
            static const std::map<QString, QString> table{
                {QString::fromUtf8("強い"), "Strong"}, {QString::fromUtf8("非常に強い"), "Very strong"}, {QString::fromUtf8("猛烈な"), "Violent"},
                {QString::fromUtf8("-"), ""}};
            return translate(text, table);
        }

        string sizeWord(const QString& text) {
            static const std::map<QString, QString> table{
                {QString::fromUtf8("大型"), "Large"}, {QString::fromUtf8("超大型"), "Very large"}, {QString::fromUtf8("-"), ""}};
            return translate(text, table);
        }

        string place(const QString& text) {
            static const std::map<QString, QString> table{
                {QString::fromUtf8("マリアナ諸島"), "Mariana Islands"}, {QString::fromUtf8("小笠原近海"), "near the Ogasawara Islands"},
                {QString::fromUtf8("日本の東"), "east of Japan"}, {QString::fromUtf8("日本の南"), "south of Japan"},
                {QString::fromUtf8("日本の南東"), "southeast of Japan"}, {QString::fromUtf8("日本の南西"), "southwest of Japan"},
                {QString::fromUtf8("日本の北東"), "northeast of Japan"}, {QString::fromUtf8("日本の北"), "north of Japan"},
                {QString::fromUtf8("フィリピンの東"), "east of the Philippines"}, {QString::fromUtf8("フィリピンの北東"), "northeast of the Philippines"},
                {QString::fromUtf8("フィリピン近海"), "near the Philippines"}, {QString::fromUtf8("フィリピンの南東"), "southeast of the Philippines"},
                {QString::fromUtf8("南シナ海"), "South China Sea"}, {QString::fromUtf8("東シナ海"), "East China Sea"},
                {QString::fromUtf8("南シナ海北部"), "northern South China Sea"}, {QString::fromUtf8("南シナ海中部"), "central South China Sea"},
                {QString::fromUtf8("沖縄の南"), "south of Okinawa"}, {QString::fromUtf8("沖縄の東"), "east of Okinawa"},
                {QString::fromUtf8("沖縄の南東"), "southeast of Okinawa"}, {QString::fromUtf8("台湾の東"), "east of Taiwan"},
                {QString::fromUtf8("台湾の南東"), "southeast of Taiwan"}, {QString::fromUtf8("日本の遥か東"), "far east of Japan"},
                {QString::fromUtf8("南鳥島近海"), "near Minamitorishima"}, {QString::fromUtf8("グアム島近海"), "near Guam"},
                {QString::fromUtf8("カロリン諸島"), "Caroline Islands"}, {QString::fromUtf8("マーシャル諸島"), "Marshall Islands"},
                {QString::fromUtf8("ウェーク島近海"), "near Wake Island"}, {QString::fromUtf8("東シナ海南部"), "southern East China Sea"},
                {QString::fromUtf8("大東島地方"), "Daito Islands region"}, {QString::fromUtf8("先島諸島"), "Sakishima Islands"},
                {QString::fromUtf8("奄美地方"), "Amami region"}, {QString::fromUtf8("沖縄本島地方"), "Okinawa Island region"},
                {QString::fromUtf8("九州南部"), "southern Kyushu"}, {QString::fromUtf8("日本海"), "Sea of Japan"},
                {QString::fromUtf8("ベトナム"), "Vietnam"}, {QString::fromUtf8("海南島"), "Hainan Island"},
                {QString::fromUtf8("中国大陸"), "mainland China"}, {QString::fromUtf8("三陸沖"), "off Sanriku"},
                {QString::fromUtf8("関東の東"), "east of Kanto"}};
            return translate(text, table);
        }

        string areaWord(const QJsonValue& area) {
            if (area.isObject()) {   // {"jp": "全域", "en": "All"}
                return area.toObject().value("en").toString().toStdString();
            }
            return direction(area.toString());
        }

        string number(double value, int decimals) {
            return QString::number(value, 'f', decimals).toStdString();
        }

        string windRadii(const QJsonArray& areas) {
            string out;
            for (const auto& entry : areas) {
                const auto object = entry.toObject();
                const auto range = object.value("range").toObject();
                const auto area = areaWord(object.value("area"));
                out += (out.empty() ? "" : ", ") + (area == "All" ? string{"all directions"} : area) + " " + number(range.value("nm").toDouble(), 0) + " nm (" +
                       number(range.value("km").toDouble(), 0) + " km)";
            }
            return out;
        }

        string motion(const QJsonObject& part) {
            string out;
            const auto course = part.value("course").toString();
            if (!course.isEmpty()) {
                out = direction(course);
            }
            const auto speed = part.value("speed").toObject();
            if (speed.contains("kt")) {
                out += (out.empty() ? "" : " at ") + string{speed.value("kt").toString().toStdString()} + " kt (" + speed.value("km/h").toString().toStdString() + " km/h)";
            } else if (speed.contains("note")) {
                const auto english = speed.value("note").toObject().value("en").toString().toStdString();
                out += (out.empty() ? "" : ", ") + (english.empty() ? string{"slow-moving"} : QString::fromStdString(english).toLower().toStdString());
            }
            return out;
        }

        string normalised(const QString& name) {
            QString out;
            for (const auto c : name) {
                if (c.isLetter()) out += c.toUpper();
            }
            return out.toStdString();
        }

        QJsonDocument fetch(const string& url) {
            return QJsonDocument::fromJson(QString::fromStdString(URL::getText(url)).toUtf8());
        }

        string describe(const QJsonObject& part, const QString& heading) {
            string out = heading.toStdString() + "\n";
            const auto position = part.value("position").toObject().value("deg").toArray();
            if (position.size() == 2) {
                const auto lat = position[0].toDouble();
                const auto lon = position[1].toDouble();
                out += "  Position " + number(std::abs(lat), 1) + (lat >= 0 ? "N " : "S ") + number(std::abs(lon), 1) + (lon >= 0 ? "E" : "W");
                const auto where = place(part.value("location").toString());
                if (!where.empty()) out += " (" + where + ")";
                const auto moving = motion(part);
                if (!moving.empty()) out += ", moving " + moving;
                out += "\n";
            }
            const auto wind = part.value("maximumWind").toObject();
            string line = "  ";
            const auto pressure = part.value("pressure").toString();
            if (!pressure.isEmpty()) line += "Central pressure " + pressure.toStdString() + " hPa";
            const auto sustained = wind.value("sustained").toObject();
            if (!sustained.isEmpty()) {
                line += string{line.size() > 2 ? "; " : ""} + "max sustained wind " + sustained.value("kt").toString().toStdString() + " kt (" +
                        sustained.value("m/s").toString().toStdString() + " m/s)";
                const auto gust = wind.value("gust").toObject();
                if (!gust.isEmpty()) line += ", gusts " + gust.value("kt").toString().toStdString() + " kt";
            }
            if (line.size() > 2) out += line + "\n";
            const auto intensity = intensityWord(part.value("intensity").toString());
            const auto size = sizeWord(part.value("scale").toString());
            if (!intensity.empty() || !size.empty()) {
                out += "  Class: " + string{size.empty() ? "" : size + " size"} + string{!size.empty() && !intensity.empty() ? ", " : ""} +
                       string{intensity.empty() ? "" : intensity + " intensity"} + "\n";
            }
            const auto gale = windRadii(part.value("galeWarning").toArray());
            if (!gale.empty()) out += "  Winds of 30 kt or more extend: " + gale + "\n";
            const auto storm = windRadii(part.value("stormWarning").toArray());
            if (!storm.empty()) out += "  Winds of 50 kt or more extend: " + storm + "\n";
            const auto circle = part.value("probabilityCircleRadius").toObject();
            if (!circle.isEmpty()) {
                out += "  70% probability circle for the centre: radius " + number(circle.value("nm").toDouble(), 0) + " nm (" +
                       number(circle.value("km").toDouble(), 0) + " km)\n";
            }
            return out;
        }

        QString utcText(const QString& iso) {
            const auto when = QDateTime::fromString(iso, Qt::ISODate).toUTC();
            return when.isValid() ? when.toString("yyyy-MM-dd HH:mm") + " UTC" : iso;
        }
    }

    string nameFromTitle(const string& title) {
        const auto dash = title.find(" - ");
        if (dash == string::npos) {
            return {};
        }
        const auto words = QString::fromStdString(title.substr(dash + 3)).trimmed().split(' ', Qt::SkipEmptyParts);
        QStringList name;
        for (int i = words.size() - 1; i >= 0 && words[i] == words[i].toUpper() && words[i] != "INVEST"; i -= 1) {
            name.prepend(words[i]);
        }
        return name.join(' ').toStdString();
    }

    bool advisoryFor(const string& stormName, string& text, string& error) {
        text.clear();
        if (stormName.empty()) {
            error = "an invest has no JMA advisory";
            return false;
        }
        const auto active = fetch(base + "targetTc.json").array();
        if (active.isEmpty()) {
            error = "JMA's active-storm list could not be read";
            return false;
        }
        const auto wanted = normalised(QString::fromStdString(stormName));
        for (const auto& entry : active) {
            const auto id = entry.toObject().value("tropicalCyclone").toString().toStdString();
            if (id.empty()) continue;
            const auto parts = fetch(base + id + "/specifications.json").array();
            if (parts.isEmpty()) continue;
            const auto title = parts[0].toObject();
            if (normalised(title.value("name").toObject().value("en").toString()) != wanted) continue;
            text = "JMA (RSMC Tokyo) - Tropical cyclone " + title.value("typhoonNumber").toString().toStdString() + " " + stormName + " (" +
                   title.value("category").toObject().value("en").toString().toStdString() + ")\n";
            text += "Issued " + utcText(title.value("issue").toObject().value("UTC").toString()).toStdString() + "\n";
            text += "JMA classes: TD tropical depression, TS tropical storm, STS severe tropical storm, TY typhoon. Winds are 10-minute averages\n"
                    "(JTWC uses 1-minute winds, so its values are higher). English translation of JMA's data by this program.\n\n";
            for (int i = 1; i < parts.size(); i += 1) {
                const auto part = parts[i].toObject();
                const auto heading = part.value("part").toObject().value("en").toString();
                const auto valid = utcText(part.value("validtime").toObject().value("UTC").toString());
                text += describe(part, heading + " (valid " + valid + ")") + "\n";
            }
            return true;
        }
        error = "JMA has no active storm named " + stormName;
        return false;
    }
}
