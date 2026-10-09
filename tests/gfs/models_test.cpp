// Tests of the model registry: every model is described completely, and the chart code, the sector and label lookups and the overlay set all agree with it.
#include <algorithm>
#include <cstdio>
#include <set>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    std::set<std::string> ids;
    for (const auto& d : GfsModels::all()) {
        CHECK(!d.id.empty() && ids.insert(d.id).second);               // named, and named once
        CHECK(!d.label.empty());
        CHECK(static_cast<bool>(d.source));
        const auto source = d.source("09l");
        CHECK(!source.id.empty() && source.id.compare(0, d.id.size(), d.id) == 0);   // the data source is the model's (a storm model adds its storm)
        CHECK(static_cast<bool>(source.fileUrl));
        CHECK(!d.hours.empty());
        int last = -1;
        for (const auto& h : d.hours) {   // forecast hours: in order, each segment ordered, a step that moves
            CHECK(h.from >= last && h.to >= h.from && h.step > 0);
            last = h.to;
        }
        CHECK(GfsChart::sourceLabel(d.id) == d.label);
        const auto sectors = GfsChart::sectorIds(d.id);
        CHECK(!sectors.empty());
        CHECK((d.sectors == GfsModels::Sectors::Conus) == (sectors.size() < 20));   // the CONUS models list their regions, the others the world's
        int charts = 0;
        for (const auto& p : GfsChart::products()) {
            charts += p.source == d.id ? 1 : 0;
        }
        CHECK(charts > 0);                                              // a model with no charts is not drawn
        if (d.clone.enabled) {
            CHECK(charts >= 10);                                        // a clone rule that matched almost nothing is a mistake in it
        }
        CHECK(GfsModels::find(d.id) == &d);
        CHECK(GfsModels::draws(d.id));
        CHECK(d.storm == (d.sectors == GfsModels::Sectors::Storm));
    }
    {   // a chart id is the model screen's key for it: two charts of one model must not share one (the first would hide the other)
        std::set<std::string> seen;
        for (const auto& p : GfsChart::products()) {
            CHECK(seen.insert(p.source + "/" + p.id).second);
        }
    }
    CHECK(!GfsModels::draws("NAM") && !GfsModels::draws("") && GfsModels::find("HRRR") == nullptr);
    for (const auto& p : GfsChart::products()) {                         // every chart belongs to a registered model, and has a group
        CHECK(GfsModels::find(p.source) != nullptr);
        CHECK(!p.id.empty() && !p.label.empty() && !GfsChart::category(p).empty());
    }
    // the clone keeps the chart ids of its source, with the model's own fields
    const auto * gfsChart = GfsChart::product("500_wnd_ht", "GFS");
    const auto * rrfsChart = GfsChart::product("500_wnd_ht", "RRFS");
    CHECK(gfsChart && rrfsChart && rrfsChart->source == "RRFS");
    const auto * rrfsSea = GfsChart::product("10m_wnd_2m_temp", "RRFS");   // sea level pressure is MSLET there
    bool mslet = false;
    for (const auto& need : GfsChart::needs(*rrfsSea, 6)) {
        mslet = mslet || need.want.variable == "MSLET";
        CHECK(need.want.variable != "PRMSL");
    }
    CHECK(mslet);
    CHECK(GfsChart::product("precip_p24", "GEFS")->label.compare(0, 5, "Mean ") == 0);   // an ensemble mean says so
    // overlays: the GFS set is on the models that carry it, and the blend has its own
    const auto overlayModels = GfsModels::overlayModels();
    for (const char * id : {"GFS", "AIGFS", "GEFS", "RRFS"}) {
        CHECK(std::find(overlayModels.begin(), overlayModels.end(), id) != overlayModels.end());
        CHECK(!GfsChart::overlayChoices(id).empty());
    }
    CHECK(std::find(overlayModels.begin(), overlayModels.end(), "NBM") == overlayModels.end() && std::find(overlayModels.begin(), overlayModels.end(), "HAFSA") == overlayModels.end());
    CHECK(!GfsChart::overlayChoices("NBM").empty());
    {   // the model guidance site's color bands: ascending, flat, nothing drawn under the lowest, and every chart that names one finds it
        for (const char * name : {"precip", "isotach", "radar_rain", "rh", "spread_mslp", "spread_wind", "helicity", "uh", "prob", "vis", "ceiling", "echo_top", "snowdepth"}) {
            const auto * bands = GfsChart::magPalette(name);
            CHECK(bands != nullptr && bands->banded && bands->stops.size() >= 2);
            for (size_t i = 1; bands && i < bands->stops.size(); i++) {
                CHECK(bands->stops[i].first > bands->stops[i - 1].first);
            }
        }
        CHECK(GfsChart::magPalette("nothing like this") == nullptr);
        const auto * precip = GfsChart::magPalette("precip");   // in millimeters: the first band starts at 0.01 inch
        CHECK(std::abs(precip->stops.front().first - 0.254) < 1e-6 && precip->stops.size() == 16);
        CHECK(qAlpha(precip->at(0.0)) == 0 && qAlpha(precip->at(0.2)) == 0);                  // under 0.01 inch: nothing
        CHECK(precip->at(0.254) == precip->at(2.0) && precip->at(2.0) != precip->at(2.6));    // flat over a band, a new color at the next stop
        CHECK(precip->at(1000.0) == precip->stops.back().second.rgba());                      // the top band goes on up
        for (const auto& p : GfsChart::products()) {
            if (!p.palette.empty()) {
                CHECK(GfsChart::magPalette(p.palette) != nullptr);
            }
        }
    }
    std::printf(failures ? "%d failures\n" : "all model registry tests passed\n", failures);
    return failures ? 1 : 0;
}
