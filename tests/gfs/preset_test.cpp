// Tests of the chart presets: a preset is kept as one line and comes back whole, names are kept safe, put / load / remove work through the preferences, and a preset put on a model gives that
// model's chart (the same id, or its own id for the same checks) with the overlays it offers and the others named as dropped.
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include <set>
#include "gfs/ChartAlias.h"
#include "gfs/ChartPreset.h"
#include "gfs/GfsChart.h"
#include "util/Utility.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_preset_test_prefs.ini", QSettings::IniFormat};
    MyApplication::preferences->clear();
    using namespace ChartPreset;
    // the line
    const Preset a{"Chase day", "500_wnd_ht", {"o_a", "o_b"}};
    Preset back;
    CHECK(fromLine(line(a), back) && back.name == a.name && back.chart == a.chart && back.overlays == a.overlays);
    CHECK(fromLine("Plain|10m_wnd|", back) && back.overlays.empty());
    CHECK(!fromLine("", back) && !fromLine("name", back) && !fromLine("name|chart", back) && !fromLine("|chart|", back) && !fromLine("name||", back) && !fromLine("a|b|c|d", back));
    CHECK(cleanName("  a|b\nc  ") == "abc" && cleanName("|\n") == "" && cleanName(" x ") == "x");
    // kept in the preferences
    CHECK(load().empty());
    put(a);
    put({"Second", "850_wnd_ht", {}});
    put({"Chase day", "500_wnd_ht", {"o_c"}});   // replaces the one of that name, in its place
    auto kept = load();
    CHECK(kept.size() == 2 && kept[0].name == "Chase day" && kept[0].overlays == std::vector<std::string>{"o_c"} && kept[1].name == "Second");
    put({"  ", "500_wnd_ht", {}});               // no name: nothing kept
    put({"No chart", "", {}});
    CHECK(load().size() == 2);
    const bool removed = erase("Second"), again = erase("Second");
    std::printf("removed %d again %d left %zu\n", removed, again, load().size());
    CHECK(removed && !again && load().size() == 1);
    // from what is on show
    ChartSelection s;
    s.model = "GFS";
    s.param = "500_wnd_ht";
    s.overlays = {"o_x"};
    const auto made = fromSelection(" Mine ", s);
    CHECK(made.name == "Mine" && made.chart == "500_wnd_ht" && made.overlays == s.overlays);
    // on models: every chart of every model, put on every model, gives a chart that model has, with overlays it offers
    std::set<std::string> models;
    for (const auto& product : GfsChart::products()) {
        models.insert(product.source);
    }
    int found = 0, sameId = 0, otherId = 0, none = 0, droppedAny = 0;
    for (const auto& from : GfsChart::products()) {
        if (from.source != "GFS" && from.source != "IFS" && from.source != "HRRR") {
            continue;   // (three sources are enough to be every kind of case, and keep the test fast)
        }
        std::vector<std::string> ticked;
        for (const auto& c : GfsChart::overlayChoices(from.source)) {
            ticked.push_back(c.id);
            if (ticked.size() == 3) {
                break;
            }
        }
        const Preset p{"t", from.id, ticked};
        for (const auto& to : models) {
            const auto r = resolve(p, to);
            CHECK(r.selection.model == to);
            if (r.found) {
                found++;
                CHECK(GfsChart::product(r.selection.param, to) != nullptr);   // a chart the model has
                (r.otherId ? otherId : sameId)++;
                CHECK(r.otherId == (r.selection.param != from.id));
                std::set<std::string> offered;
                for (const auto& c : GfsChart::overlayChoices(to)) {
                    offered.insert(c.id);
                }
                for (const auto& id : r.selection.overlays) {
                    CHECK(offered.count(id) == 1);
                }
                CHECK(r.selection.overlays.size() + r.dropped.size() == ticked.size());
            } else {
                none++;
                CHECK(r.dropped == ticked && r.selection.overlays.empty());
            }
            droppedAny += r.dropped.empty() ? 0 : 1;
        }
    }
    std::printf("resolved: found %d (same id %d, the model's own id %d), no chart %d, with dropped overlays %d\n", found, sameId, otherId, none, droppedAny);
    CHECK(found > 0 && sameId > 0 && none > 0);
    // a one-off never moves to another id: it is found only where its id exists
    for (const auto& e : ChartAlias::table()) {
        if (ChartAlias::oneOff(e)) {
            for (const auto& to : models) {
                const auto r = resolve({"t", e.id, {}}, to);
                CHECK(r.found == (GfsChart::product(e.id, to) != nullptr) && !r.otherId);
            }
            break;
        }
    }
    MyApplication::preferences->clear();
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
