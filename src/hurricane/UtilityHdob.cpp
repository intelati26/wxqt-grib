// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityHdob.h"
#include "util/UtilityDate.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace {
    long daysFromCivil(int y, int m, int d) {
        y -= m <= 2 ? 1 : 0;
        const long era = (y >= 0 ? y : y - 399) / 400;
        const long yoe = y - era * 400;
        const long doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
        const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
        return era * 146097 + doe - 719468;
    }

    bool allDigits(const string& s) {
        if (s.empty()) {
            return false;
        }
        for (const char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                return false;
            }
        }
        return true;
    }

    // a field of digits; any '/' (the bulletin's "missing") gives the missing value
    double number(const string& s, double scale = 1.0) {
        if (!allDigits(s)) {
            return UtilityHdob::missing;
        }
        return std::strtol(s.c_str(), nullptr, 10) * scale;
    }

    // sTTT: sign and tenths of a degree
    double signedTenths(const string& s) {
        if (s.size() != 4 || (s[0] != '+' && s[0] != '-') || !allDigits(s.substr(1))) {
            return UtilityHdob::missing;
        }
        const double value = std::strtol(s.c_str() + 1, nullptr, 10) / 10.0;
        return s[0] == '-' ? -value : value;
    }

    // LLLLH / NNNNNH: degrees and minutes with a hemisphere letter
    bool position(const string& s, size_t degreeDigits, double& value) {
        if (s.size() != degreeDigits + 3 || !allDigits(s.substr(0, degreeDigits + 2))) {
            return false;
        }
        value = std::strtol(s.substr(0, degreeDigits).c_str(), nullptr, 10) + std::strtol(s.substr(degreeDigits, 2).c_str(), nullptr, 10) / 60.0;
        const char hemisphere = s.back();
        if (hemisphere == 'S' || hemisphere == 'W') {
            value = -value;
        } else if (hemisphere != 'N' && hemisphere != 'E') {
            return false;
        }
        return true;
    }

    // PPPP: tenths of mb, the leading 1 of a pressure of 1000 mb or more dropped (no aircraft flies above 100 mb, about 16 km)
    double pressure(const string& s) {
        const double tenths = number(s);
        if (!UtilityHdob::has(tenths)) {
            return UtilityHdob::missing;
        }
        return (tenths < 1000.0 ? tenths + 10000.0 : tenths) / 10.0;
    }
}

vector<UtilityHdob::Message> UtilityHdob::parse(const string& text) {
    vector<Message> messages;
    std::istringstream stream{text};
    string line;
    while (std::getline(stream, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.pop_back();
        }
        std::istringstream words{line};
        vector<string> w;
        string token;
        while (words >> token) {
            w.push_back(token);
        }
        // the mission line: "<mission id ...> HDOB NN YYYYMMDD"
        if (w.size() >= 4 && w[w.size() - 3] == "HDOB" && allDigits(w[w.size() - 2]) && w.back().size() == 8 && allDigits(w.back())) {
            Message message;
            for (size_t i = 0; i + 3 < w.size(); i++) {
                message.mission += (i == 0 ? "" : " ") + w[i];
            }
            message.number = std::atoi(w[w.size() - 2].c_str());
            message.date = w.back();
            messages.push_back(message);
            continue;
        }
        // a data line: 13 fields starting with hhmmss
        if (messages.empty() || w.size() != 13 || w[0].size() != 6 || !allDigits(w[0])) {
            continue;
        }
        Ob ob;
        if (!position(w[1], 2, ob.lat) || !position(w[2], 3, ob.lon)) {
            continue;
        }
        const auto& date = messages.back().date;
        ob.seconds = daysFromCivil(std::stoi(date.substr(0, 4)), std::stoi(date.substr(4, 2)), std::stoi(date.substr(6, 2))) * 86400L +
            std::stoi(w[0].substr(0, 2)) * 3600L + std::stoi(w[0].substr(2, 2)) * 60L + std::stoi(w[0].substr(4, 2));
        // a bulletin that runs past midnight: the clock wraps, the date line is that of the first data line
        if (!messages.back().obs.empty() && ob.seconds < messages.back().obs.back().seconds) {
            ob.seconds += 86400L;
        }
        ob.staticPressure = pressure(w[3]);
        ob.height = number(w[4]);
        // XXXX: the extrapolated surface pressure at 550 mb or lower altitude, otherwise the D-value in m (negative ones have 5000 added)
        if (has(ob.staticPressure) && ob.staticPressure >= 550.0) {
            ob.surfacePressure = pressure(w[5]);
        } else if (const auto d = number(w[5]); has(d)) {
            ob.dValue = d >= 5000.0 ? 5000.0 - d : d;
        }
        ob.temperature = signedTenths(w[6]);
        ob.dewPoint = signedTenths(w[7]);
        if (w[8].size() == 6) {
            const auto direction = number(w[8].substr(0, 3));
            const auto speed = number(w[8].substr(3, 3));
            ob.windDirection = direction >= 999.0 ? missing : direction;
            ob.windSpeed = speed >= 999.0 ? missing : speed;
        }
        const auto knots = [] (const string& s) { const auto v = number(s); return v >= 999.0 ? UtilityHdob::missing : v; };
        ob.peakWind = knots(w[9]);
        ob.sfmrWind = knots(w[10]);
        ob.rainRate = knots(w[11]);
        ob.flags = w[12].size() == 2 && allDigits(w[12]) ? std::atoi(w[12].c_str()) : 0;
        messages.back().obs.push_back(ob);
    }
    return messages;
}

string UtilityHdob::timeText(long seconds) {
    return UtilityDate::isoMinute(seconds);   // "2026-10-07 01:22Z"
}
