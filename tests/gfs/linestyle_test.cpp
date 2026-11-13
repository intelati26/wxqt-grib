// Tests of the style of a ticked line (GfsChart::lineStyle and the preference LINESTYLE_<id>): the program's style is the standard, a kept style replaces it on the composed chart (color, weight,
// dashes, interval, numbers), the legend stops naming a color that is no longer the line's, a bad text is ignored, and what is not a line has no style.
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include "gfs/GfsChart.h"
#include "util/Utility.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

namespace {
const GfsChart::ContourSet * composedLine(const GfsChart::Product& composed, const std::string& overlay) {
    for (const auto& set : composed.contours) {
        if (set.key.compare(0, 2 + overlay.size(), "o_" + overlay) == 0) {
            return &set;
        }
    }
    return nullptr;
}
}  // namespace

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_linestyle_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    CHECK(!GfsChart::lineStyle("fill_dewpoint_2m").available && !GfsChart::lineStyle("barbs_850").available && !GfsChart::lineStyle("nothing").available);
    const auto z = GfsChart::lineStyle("z500");
    CHECK(z.available && z.value == z.standard && z.standard.interval > 0.0 && z.standard.labelled && z.title.find("500mb height") == 0);
    const auto * base = GfsChart::product("mslp_10m_wind", "GFS");
    CHECK(base != nullptr);
    {   // the program's style on the composed chart
        const auto composed = GfsChart::compose(*base, {"z500"});
        const auto * set = composedLine(composed, "z500");
        CHECK(set != nullptr && set->color == z.standard.color && set->width == z.standard.width && set->dashed == z.standard.dashed && set->interval == z.standard.interval);
        CHECK(set != nullptr && set->title.find(", ") != std::string::npos);   // ("500mb height (dam, blue)")
    }
    // a kept style
    GfsChart::LineStyle mine{QColor{200, 0, 120}, 2.5, true, 3.0, false};
    GfsChart::setLineStyle("z500", mine);
    CHECK(GfsChart::lineStyle("z500").value == mine && GfsChart::lineStyle("z500").standard == z.standard);
    {
        const auto composed = GfsChart::compose(*base, {"z500"});
        const auto * set = composedLine(composed, "z500");
        CHECK(set != nullptr && set->color == mine.color && set->width == 2.5 && set->dashed && set->interval == 3.0 && !set->labelled);
        CHECK(set != nullptr && set->title == "500mb height (dam)");   // (the color word is gone: the line is no longer that color)
    }
    // another line is not changed
    CHECK(GfsChart::lineStyle("z700").value == GfsChart::lineStyle("z700").standard);
    // the standard is not kept (nothing under the preference), and a clear takes a kept style away
    GfsChart::setLineStyle("z500", z.standard);
    CHECK(Utility::readPref("LINESTYLE_z500", "x").empty() && GfsChart::lineStyle("z500").value == z.standard);
    GfsChart::setLineStyle("z500", mine);
    GfsChart::clearLineStyle("z500");
    CHECK(GfsChart::lineStyle("z500").value == z.standard);
    // the same color as the standard keeps the title's word
    GfsChart::LineStyle thicker = z.standard;
    thicker.width = 3.0;
    GfsChart::setLineStyle("z500", thicker);
    {
        const auto composed = GfsChart::compose(*base, {"z500"});
        const auto * set = composedLine(composed, "z500");
        CHECK(set != nullptr && set->width == 3.0 && set->title.find(", ") != std::string::npos);
    }
    GfsChart::clearLineStyle("z500");
    // the text: it comes back, and a bad one is not a style
    GfsChart::LineStyle back;
    CHECK(GfsChart::lineStyleFromText(GfsChart::lineStyleToText(mine), back) && back == mine);
    for (const char * bad : {"", "red", "#ff0000|1|0|3", "#ff0000|x|0|3|1", "nocolor|1|0|3|1", "#ff0000|0|0|3|1", "#ff0000|1|0|-1|1", "#ff0000|99|0|3|1"}) {
        GfsChart::LineStyle kept = mine;
        CHECK(!GfsChart::lineStyleFromText(bad, kept) && kept == mine);
    }
    Utility::writePref("LINESTYLE_z500", "garbage");
    CHECK(GfsChart::lineStyle("z500").value == z.standard);   // a bad kept text is ignored
    MyApplication::preferences->clear();
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
