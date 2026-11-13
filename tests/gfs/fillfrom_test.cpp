// Tests of where a ticked fill starts (GfsChart::fillFrom and the preference FILLFROM_<id>): the program's value is the scale's own cut-off, a kept value moves it, "all" takes it away, and the
// chart that is composed has its fill scale clear under the value and tinted from it up.
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include "gfs/GfsChart.h"
#include "util/Utility.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
// the scale of the ticked fill in the composed chart: the stops as value -> alpha
std::vector<std::pair<double, int>> composedAlphas(const std::string& chart, const std::string& overlay) {
    std::vector<std::pair<double, int>> out;
    const auto * base = GfsChart::product(chart, "GFS");
    if (base == nullptr) {
        return out;
    }
    const auto composed = GfsChart::compose(*base, {overlay});
    for (const auto& [key, ramp] : composed.overlays) {
        if (key.compare(0, 2, "o_") == 0) {
            for (const auto& stop : ramp.stops) {
                out.push_back({stop.first, stop.second.alpha()});
            }
        }
    }
    return out;
}
}  // namespace

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_fillfrom_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    CHECK(!GfsChart::fillFrom("mslp").available && !GfsChart::fillFrom("nothing").available);
    const auto dew = GfsChart::fillFrom("fill_dewpoint_2m");
    CHECK(dew.available && dew.unit == "C" && dew.value == dew.standard && dew.standard >= 10.0 && dew.low < dew.standard && dew.high > dew.standard);
    const auto wind = GfsChart::fillFrom("fill_wind_500");
    CHECK(wind.available && wind.unit == "kt" && wind.standard > 0.0);
    const auto temp = GfsChart::fillFrom("fill_temp_850");
    CHECK(temp.available && temp.standard == temp.low);   // a fill with no cut-off starts at the bottom of its scale
    // the composed chart with the program's value
    auto stops = composedAlphas("mslp_10m_wind", "fill_dewpoint_2m");
    CHECK(!stops.empty());
    int clear = 0, tinted = 0;
    for (const auto& [value, alpha] : stops) {
        (alpha == 0 ? clear : tinted)++;
        CHECK((value < dew.standard) == (alpha == 0));
    }
    CHECK(clear > 0 && tinted > 0);
    // a kept value moves the cut-off, and "all" takes it away
    Utility::writePref("FILLFROM_fill_dewpoint_2m", "20");
    CHECK(GfsChart::fillFrom("fill_dewpoint_2m").value == 20.0 && GfsChart::fillFrom("fill_dewpoint_2m").standard == dew.standard);
    for (const auto& [value, alpha] : composedAlphas("mslp_10m_wind", "fill_dewpoint_2m")) {
        CHECK((value < 20.0) == (alpha == 0));
    }
    Utility::writePref("FILLFROM_fill_dewpoint_2m", "all");
    for (const auto& [value, alpha] : composedAlphas("mslp_10m_wind", "fill_dewpoint_2m")) {
        CHECK(alpha > 0);
        (void) value;
    }
    // the user's scale on a ticked fill: its colors, the overlay's tint and start
    Utility::writePref("FILLFROM_fill_dewpoint_2m", "");
    const auto scale = GfsChart::fillScale("fill_dewpoint_2m");
    CHECK(scale.available && scale.quantity == GfsChart::Quantity::Temperature && !scale.title.empty());
    for (const auto& stop : scale.ramp.stops) {
        CHECK(stop.second.alpha() == 255);   // (opaque: the editor edits the colors)
    }
    GfsChart::Ramp user;
    user.stops = {{-40.0, QColor{255, 0, 0}}, {10.0, QColor{0, 255, 0}}, {20.0, QColor{0, 0, 255}}, {35.0, QColor{255, 255, 0}}};
    const auto * base = GfsChart::product("mslp_10m_wind", "GFS");
    CHECK(base != nullptr && GfsChart::compose(*base, {"fill_dewpoint_2m"}).overlays.size() >= 1);
    GfsChart::Ramp overlay;
    for (const auto& [key, ramp] : GfsChart::compose(*base, {"fill_dewpoint_2m"}).overlays) {
        if (key.compare(0, 2, "o_") == 0) {
            overlay = ramp;
        }
    }
    const auto shown = GfsChart::asFillOverlay(user, overlay);
    CHECK(shown.stops.size() == user.stops.size());
    const int tint = overlay.stops.back().second.alpha();
    for (size_t i = 0; i < shown.stops.size(); i++) {
        CHECK(shown.stops[i].second.red() == user.stops[i].second.red() && shown.stops[i].second.green() == user.stops[i].second.green());   // its colors
        CHECK(shown.stops[i].second.alpha() == (shown.stops[i].first < dew.standard ? 0 : tint));                                              // the overlay's start and tint
    }
    MyApplication::preferences->clear();
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
