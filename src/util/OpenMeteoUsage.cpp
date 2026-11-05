// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/OpenMeteoUsage.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <QDate>
#include <QDateTime>
#include <QUrl>
#include <QUrlQuery>
#include "util/MyApplication.h"
#include "util/Utility.h"

namespace {
    std::mutex usageMutex;
    OpenMeteoUsage::Today usage;
    bool loaded{false};
    std::deque<std::chrono::steady_clock::time_point> recent;

    std::string utcDay() {
        return QDateTime::currentDateTimeUtc().date().toString("yyyy-MM-dd").toStdString();
    }

    void save() {
        if (MyApplication::preferences == nullptr) {   // (a test program has none)
            return;
        }
        Utility::writePref("OPENMETEO_DAY", usage.day);
        Utility::writePref("OPENMETEO_CALLS", std::to_string(usage.calls));
        Utility::writePref("OPENMETEO_WEIGHT", std::to_string(usage.weight));
    }

    // the day's count from the settings (once), and a new day starts from nothing
    void roll() {
        const auto day = utcDay();
        if (!loaded) {
            loaded = true;
            if (MyApplication::preferences != nullptr && Utility::readPref("OPENMETEO_DAY", "") == day) {
                usage.day = day;
                usage.calls = std::stol(Utility::readPref("OPENMETEO_CALLS", "0"));
                usage.weight = std::stod(Utility::readPref("OPENMETEO_WEIGHT", "0"));
            }
        }
        if (usage.day != day) {
            usage = OpenMeteoUsage::Today{};
            usage.day = day;
        }
    }
}

bool OpenMeteoUsage::isOpenMeteo(const std::string& url) {
    const auto host = QUrl{QString::fromStdString(url)}.host();
    return host == "open-meteo.com" || host.endsWith(".open-meteo.com");
}

double OpenMeteoUsage::weightOf(const std::string& url) {
    const QUrlQuery query{QUrl{QString::fromStdString(url)}};
    int variables = 0;
    for (const auto& name : {"hourly", "daily", "weekly", "monthly", "current", "minutely_15"}) {
        const auto list = query.queryItemValue(name);
        if (!list.isEmpty()) {
            variables += static_cast<int>(list.count(',')) + 1;
        }
    }
    const auto startDate = QDate::fromString(query.queryItemValue("start_date"), Qt::ISODate);
    const auto endDate = QDate::fromString(query.queryItemValue("end_date"), Qt::ISODate);
    double days = 7.0;   // the forecast's default
    if (startDate.isValid() && endDate.isValid()) {
        days = static_cast<double>(startDate.daysTo(endDate) + 1);
    } else if (!query.queryItemValue("forecast_days").isEmpty()) {
        days = query.queryItemValue("forecast_days").toDouble();
    } else if (QUrl{QString::fromStdString(url)}.host().startsWith("seasonal-api")) {
        days = 92.0;   // the seasonal forecast's default
    }
    const auto locations = static_cast<double>(query.queryItemValue("latitude").count(',') + 1);
    return locations * std::max(1.0, variables / 10.0) * std::max(1.0, days / 14.0);
}

void OpenMeteoUsage::record(const std::string& url) {
    if (!isOpenMeteo(url)) {
        return;
    }
    const auto weight = weightOf(url);
    const std::lock_guard<std::mutex> guard{usageMutex};
    roll();
    usage.calls++;
    usage.weight += weight;
    recent.push_back(std::chrono::steady_clock::now());
    save();
}

OpenMeteoUsage::Today OpenMeteoUsage::today() {
    const std::lock_guard<std::mutex> guard{usageMutex};
    roll();
    const auto cutoff = std::chrono::steady_clock::now() - std::chrono::seconds{60};
    while (!recent.empty() && recent.front() < cutoff) {
        recent.pop_front();
    }
    auto out = usage;
    out.lastMinute = static_cast<long>(recent.size());
    return out;
}
