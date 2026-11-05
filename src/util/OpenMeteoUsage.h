// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OPENMETEOUSAGE_H
#define OPENMETEOUSAGE_H

#include <string>

// How much of Open-Meteo's free allowance (10,000 calls a day, 600 a minute, per IP address, counted from midnight UTC) this program has used today.
// An estimate: only the requests this program makes are seen (another program on the same address counts against the same allowance), and Open-Meteo
// weighs a call by its size, about (variables / 10) x (days / 14) with 1 as the least, which is worked out here from the request's own parameters.
// Counted where a request leaves the program (every try, a retry too); kept in the settings so that a restart carries on the day's count.
namespace OpenMeteoUsage {
    struct Today {
        std::string day;      // yyyy-MM-dd UTC
        long calls{0};        // requests sent
        double weight{0.0};   // their estimated cost against the allowance
        long lastMinute{0};   // requests in the last 60 seconds
    };
    constexpr double dailyLimit = 10000.0;
    constexpr long minuteLimit = 600;

    bool isOpenMeteo(const std::string& url);
    double weightOf(const std::string& url);   // the estimated cost of one request
    void record(const std::string& url);       // no effect for another host
    Today today();
}

#endif  // OPENMETEOUSAGE_H
