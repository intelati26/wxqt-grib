// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/UtilityJtwc.h"
#include <cstdlib>
#include <QRegularExpression>
#include <QString>
#include <QStringList>

namespace UtilityJtwc {
    string codeOf(const string& id) {
        if (id.size() < 8) {
            return {};
        }
        const auto basin = id.substr(0, 2);
        if (basin != "wp" && basin != "io" && basin != "sh") {
            return {};
        }
        const int number = std::atoi(id.substr(2, 2).c_str());
        return number >= 90 ? string{} : basin + id.substr(2, 2) + id.substr(6, 2);
    }

    string warningUrl(const string& code) {
        return "https://www.metoc.navy.mil/jtwc/products/" + code + "web.txt";
    }

    vector<Fix> parseWarning(const string& text) {
        vector<Fix> out;
        static const QRegularExpression position{R"((\d{6})Z\s*---\s*(?:NEAR\s*)?(\d+\.\d)([NS])\s+(\d+\.\d)([EW]))"};
        static const QRegularExpression heading{R"((\d+)\s+HRS,\s+VALID AT)"};
        static const QRegularExpression winds{R"(MAX SUSTAINED WINDS\s*-\s*(\d+)\s*KT(?:,\s*GUSTS\s*(\d+)\s*KT)?)"};
        static const QRegularExpression radius{R"(RADIUS OF (\d+) KT WINDS)"};
        static const QRegularExpression quadrant{R"((\d+)\s*NM\s+(NORTHEAST|SOUTHEAST|SOUTHWEST|NORTHWEST)\s+QUADRANT)"};
        Fix * now = nullptr;
        int pendingHour = 0;
        bool inForecasts = false;
        int level = -1;   // the radius block being read: 0, 1, 2 for 34, 50, 64
        for (const auto& raw : QString::fromStdString(text).split('\n')) {
            const auto line = raw.trimmed();
            if (line.startsWith("WARNING POSITION")) {
                pendingHour = 0;
                inForecasts = false;
                continue;
            }
            if (line.startsWith("FORECASTS")) {
                inForecasts = true;
                continue;
            }
            const auto h = heading.match(line);
            if (h.hasMatch()) {
                pendingHour = h.captured(1).toInt();
                inForecasts = true;
                now = nullptr;
                continue;
            }
            const auto p = position.match(line);
            if (p.hasMatch() && (now == nullptr)) {
                if (!inForecasts && !out.empty()) {
                    continue;   // (the repeated position of the warning)
                }
                Fix f;
                f.hour = pendingHour;
                f.validTime = p.captured(1).toStdString();
                f.lat = p.captured(2).toDouble() * (p.captured(3) == "S" ? -1.0 : 1.0);
                f.lon = p.captured(4).toDouble() * (p.captured(5) == "W" ? -1.0 : 1.0);
                out.push_back(f);
                now = &out.back();
                level = -1;
                continue;
            }
            if (now == nullptr) {
                continue;
            }
            const auto w = winds.match(line);
            if (w.hasMatch()) {
                now->knots = w.captured(1).toInt();
                now->gusts = w.captured(2).isEmpty() ? -1 : w.captured(2).toInt();
                continue;
            }
            const auto r = radius.match(line);
            if (r.hasMatch()) {
                const int kt = r.captured(1).toInt();
                level = kt == 34 ? 0 : kt == 50 ? 1 : kt == 64 ? 2 : -1;
            }
            const auto q = quadrant.match(line);
            if (q.hasMatch() && level >= 0) {
                const auto name = q.captured(2);
                const int index = name == "NORTHEAST" ? 0 : name == "SOUTHEAST" ? 1 : name == "SOUTHWEST" ? 2 : 3;
                now->radiusNm[level][index] = q.captured(1).toDouble();
            }
            if (line.startsWith("---") || line.startsWith("REPEAT POSIT") || line.startsWith("VECTOR TO")) {
                if (line.startsWith("---")) {
                    now = nullptr;
                }
            }
        }
        return out;
    }
}
