// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/NiceWeatherCache.h"
#include <array>
#include <mutex>
#include "gfs/GfsRender.h"
#include "misc/NiceWeatherOutlook.h"

namespace {
    constexpr int days = 8;
    std::mutex guard;                                                    // the table
    std::array<std::shared_ptr<const NiceWeatherCache::Day>, days> made;
    std::array<std::mutex, days> making;                                 // a day is made by one at a time: the others wait and take it

    bool fresh(const std::shared_ptr<const NiceWeatherCache::Day>& day) {
        return day && day->made.isValid() && day->made.secsTo(QDateTime::currentDateTimeUtc()) < 60 * NiceWeatherCache::freshMinutes;
    }
}

std::shared_ptr<const NiceWeatherCache::Day> NiceWeatherCache::find(int day) {
    if (day < 0 || day >= days) {
        return nullptr;
    }
    const std::lock_guard lock{guard};
    return fresh(made[static_cast<size_t>(day)]) ? made[static_cast<size_t>(day)] : nullptr;
}

std::shared_ptr<const NiceWeatherCache::Day> NiceWeatherCache::make(int day, std::string& error) {
    if (day < 0 || day >= days) {
        error = "no such day";
        return nullptr;
    }
    const std::lock_guard one{making[static_cast<size_t>(day)]};   // (what was being made by another is there when this gets in)
    if (auto lately = find(day)) {
        return lately;
    }
    auto result = std::make_shared<Day>();
    GfsRender::Session session;
    GfsRender::Variant variant;
    variant.width = 1100;
    result->probe = std::make_shared<GfsChart::Probe>();
    result->png = GfsRender::png(session, "NBM", "nice_weather", "CONUS", "", 24 * (day + 1), {}, error, result->probe.get(), variant);
    if (result->png.isEmpty()) {
        return nullptr;   // (not kept: the next asks again)
    }
    if (result->probe->valid()) {
        const auto& levels = result->probe->fields.front().grid;
        result->summary = NiceWeatherOutlook::summaryOf(levels);
        result->headline = NiceWeatherOutlook::headlineOf(levels);
        result->valid = result->probe->validUtc;
    }
    result->made = QDateTime::currentDateTimeUtc();
    const std::lock_guard lock{guard};
    made[static_cast<size_t>(day)] = result;
    return result;
}

void NiceWeatherCache::forget() {
    const std::lock_guard lock{guard};
    for (auto& day : made) {
        day.reset();
    }
}
