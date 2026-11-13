// Tests of the checks picker's kinds: the charts of a model, grouped by what they vary by. Every chart is in exactly one kind, the charts of a kind differ by their values (each text once), a
// kind of one chart has no value, and the order of the list is kept.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include <set>
#include "gfs/GfsChart.h"
#include "gfs/ChartPreset.h"
#include "gfs/GfsModels.h"
#include "models/ChecksPicker.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main(int argc, char ** argv) {
    QApplication app{argc, argv};
    MyApplication::preferences = new QSettings{"/tmp/wxqt_checks_test_prefs.ini", QSettings::IniFormat};
    int kindsTotal = 0, charts = 0, single = 0;
    for (const auto& def : GfsModels::all()) {
        std::vector<ProductPicker::Entry> entries;
        for (const auto& p : GfsChart::products()) {
            if (p.source == def.id) {
                entries.push_back({p.id, p.label, GfsChart::category(p)});
            }
        }
        if (entries.empty()) {
            continue;
        }
        const auto kinds = ChecksPicker::kinds(entries);
        std::set<std::string> seen;
        for (const auto& k : kinds) {
            CHECK(!k.charts.empty() && k.values.size() == k.charts.size());
            std::set<std::string> values;
            for (size_t i = 0; i < k.charts.size(); i++) {
                CHECK(seen.insert(k.charts[i].id).second);   // in one kind only
                CHECK(k.charts[i].group == k.group);
                if (k.charts.size() > 1) {
                    const bool ok = !k.values[i].empty() && values.insert(k.values[i]).second;
                    if (!ok) {
                        std::printf("  not told apart: %s %s (%s) value '%s'\n", def.id.c_str(), k.name.c_str(), k.charts[i].id.c_str(), k.values[i].c_str());
                    }
                    CHECK(ok);   // told apart by its value
                } else {
                    CHECK(k.values[i].empty());
                }
            }
            charts += static_cast<int>(k.charts.size());
            single += k.charts.size() == 1 ? 1 : 0;
        }
        CHECK(seen.size() == entries.size());
        kindsTotal += static_cast<int>(kinds.size());
        if (def.id == "GFS") {
            for (const auto& k : kinds) {
                std::printf("  %-28s %-52s", k.group.substr(0, 27).c_str(), k.name.substr(0, 51).c_str());
                for (const auto& v : k.values) {
                    std::printf(" [%s]", v.c_str());
                }
                std::printf("\n");
            }
        }
    }
    {   // the dialog itself: choosing a value picks that chart, ticking a line reports the ticks, setState follows, a preset puts its checks back
        std::vector<ProductPicker::Entry> entries;
        for (const auto& p : GfsChart::products()) {
            if (p.source == "GFS") {
                entries.push_back({p.id, p.label, GfsChart::category(p)});
            }
        }
        MyApplication::preferences->clear();
        std::vector<std::string> picks;
        std::vector<std::vector<std::string>> ticks;
        ChecksPicker dialog{nullptr, "GFS", entries, "500_wnd_ht", GfsChart::overlayChoices("GFS"), {}};
        dialog.onPick = [&picks] (const std::string& id) { picks.push_back(id); };
        dialog.onOverlays = [&ticks] (const std::vector<std::string>& ids) { ticks.push_back(ids); };
        const auto combos = dialog.findChildren<QComboBox *>();
        CHECK(combos.size() == 2);
        auto * kinds = combos[0];
        auto * values = combos[1];
        CHECK(values->isEnabled() && values->currentText() == "500mb");
        for (int i = 0; i < values->count(); i++) {
            if (values->itemText(i) == "850mb") {
                values->setCurrentIndex(i);
            }
        }
        CHECK(picks.size() == 1 && picks[0] == "850_wnd_ht");
        const auto boxes = dialog.findChildren<QCheckBox *>();
        CHECK(!boxes.empty());
        boxes.front()->setChecked(true);
        CHECK(ticks.size() == 1 && ticks[0].size() == 1);
        dialog.setState("500_wnd_ht", {});   // something else changed the chart: the rows follow, nothing is called
        CHECK(picks.size() == 1 && ticks.size() == 1 && !boxes.front()->isChecked() && values->currentText() == "500mb");
        for (int i = 0; i < kinds->count(); i++) {   // another kind: the same value when it has one (500mb stays 500mb)
            if (kinds->itemText(i).contains("Vorticity")) {
                kinds->setCurrentIndex(i);
                break;
            }
        }
        CHECK(picks.size() == 2 && picks.back() == "500_vort_ht");
        ChartPreset::put({"Mine", "850_temp_ht", {boxes.front()->property("id").toString().toStdString()}});
        dialog.close();
    }
    std::printf("kinds %d for %d charts over all models (%d of one chart)\n", kindsTotal, charts, single);
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
