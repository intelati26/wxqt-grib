// Tests of the checks picker's kinds: the charts of a model, grouped by what they vary by. Every chart is in exactly one kind, the charts of a kind differ by their values (each text once), a
// kind of one chart has no value, and the order of the list is kept.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include <set>
#include "gfs/GfsChart.h"
#include "gfs/ChartAlias.h"
#include "gfs/ChartPreset.h"
#include "gfs/GfsModels.h"
#include "models/ChecksPicker.h"
#include "util/Utility.h"

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
                CHECK(k.parts.empty() ? k.charts[i].group == k.group : true);   // (a product kind has the charts of all its categories)
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
        CHECK(combos.size() == 5);   // (the chart's kind, its value, and the hour, type and limit of a product)
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
        const auto summaryOf = [&dialog] {
            for (auto * label : dialog.findChildren<QLabel *>()) {
                if (label->text().startsWith("Ticked on top of the chart")) {
                    return label->text();
                }
            }
            return QString{"?"};
        };
        CHECK(summaryOf().startsWith("Ticked on top of the chart: ") && !summaryOf().contains("nothing") && summaryOf().contains("Pressure"));
        dialog.setState("500_wnd_ht", {});   // something else changed the chart: the rows follow, nothing is called
        CHECK(picks.size() == 1 && ticks.size() == 1 && !boxes.front()->isChecked() && values->currentText() == "500mb" && summaryOf().endsWith("nothing."));
        for (int i = 0; i < kinds->count(); i++) {   // another kind: the same value when it has one (500mb stays 500mb)
            if (kinds->itemText(i).contains("Vorticity")) {
                kinds->setCurrentIndex(i);
                break;
            }
        }
        CHECK(picks.size() == 2 && picks.back() == "500_vort_ht");
        // the search narrows the Chart list (and always keeps the chart on show); clearing it brings the list back
        const int allItems = kinds->count();
        auto * search = dialog.findChild<QLineEdit *>();
        CHECK(search != nullptr);
        search->setText("precipitable");
        CHECK(kinds->count() < allItems && kinds->count() >= 2);
        bool hasPw = false;
        for (int i = 0; i < kinds->count(); i++) {
            hasPw = hasPw || kinds->itemText(i).contains("Precipitable");
        }
        CHECK(hasPw);
        search->setText("");
        CHECK(kinds->count() == allItems);
        // favorites: the star keeps the chart under MODELFAV_GFS (as the Charts... picker does), a Favorites group heads the list, and choosing the favorite picks that chart
        QPushButton * starButton = nullptr;
        for (auto * b : dialog.findChildren<QPushButton *>()) {
            if (b->maximumWidth() == 40) {
                starButton = b;
            }
        }
        CHECK(starButton != nullptr);
        starButton->click();
        CHECK(Utility::readPref("MODELFAV_GFS", "") == "500_vort_ht");
        CHECK(kinds->itemText(0) == "Favorites" && kinds->itemText(1).contains("Vorticity"));
        const size_t before = picks.size();
        kinds->setCurrentIndex(1);   // (the favorite is the chart on show: nothing is picked again)
        CHECK(picks.size() == before);
        starButton->click();
        CHECK(Utility::readPref("MODELFAV_GFS", "x").empty() && kinds->itemText(0) != "Favorites");
        ChartPreset::put({"Mine", "850_temp_ht", {boxes.front()->property("id").toString().toStdString()}});
        dialog.close();
    }
    {   // a product of hours, types and limits (SPC REFS): the three lists offer only what exists, and a choice picks the chart that has it
        std::vector<ProductPicker::Entry> entries;
        for (const auto& p : GfsChart::products()) {
            if (p.source == "SPCREFS") {
                entries.push_back({p.id, p.label, GfsChart::category(p)});
            }
        }
        MyApplication::preferences->clear();
        std::vector<std::string> picks;
        ChecksPicker dialog{nullptr, "SPCREFS", entries, "qpf_06h_acc_max", GfsChart::overlayChoices("SPCREFS"), {}};
        dialog.onPick = [&picks] (const std::string& id) { picks.push_back(id); };
        const auto combos = dialog.findChildren<QComboBox *>();
        CHECK(combos.size() == 5);
        auto * kindsBox = combos[0];
        auto * hour = combos[2];
        auto * type = combos[3];
        auto * limit = combos[4];
        CHECK(kindsBox->currentText().contains("QPF") && hour->currentData().toString() == "6 h" && type->currentData().toString() == "Ensemble max");
        const auto alias = [] (const std::string& id) { return ChartAlias::find(id); };
        for (int i = 0; i < type->count(); i++) {   // every type offered has a chart at this hour
            bool any = false;
            for (const auto& e : entries) {
                const auto * a = alias(e.id);
                any = any || (a != nullptr && std::string{a->product} == "QPF" && std::string{a->window} == "6 h" && std::string{a->kind} == type->itemData(i).toString().toStdString());
            }
            CHECK(any);
        }
        for (int i = 0; i < hour->count(); i++) {   // another hour: the type stays when that hour has it, and the chart is of that hour
            if (hour->itemData(i).toString() == "24 h") {
                hour->setCurrentIndex(i);
            }
        }
        CHECK(!picks.empty());
        const auto * a24 = alias(picks.back());
        CHECK(a24 != nullptr && std::string{a24->window} == "24 h" && std::string{a24->product} == "QPF" && std::string{a24->kind} == "Ensemble max");
        for (int i = 0; i < type->count(); i++) {   // another type
            if (type->itemData(i).toString() == "Ensemble mean") {
                type->setCurrentIndex(i);
            }
        }
        const auto * mean = alias(picks.back());
        CHECK(mean != nullptr && std::string{mean->kind} == "Ensemble mean" && std::string{mean->window} == "24 h");
        for (int i = 0; i < type->count(); i++) {   // (a 24 hour QPF has no probability: that type is not offered there)
            CHECK(type->itemData(i).toString() != "Ensemble probability");
        }
        for (int i = 0; i < hour->count(); i++) {
            if (hour->itemData(i).toString() == "1 h") {
                hour->setCurrentIndex(i);
            }
        }
        for (int i = 0; i < type->count(); i++) {
            if (type->itemData(i).toString() == "Ensemble probability") {
                type->setCurrentIndex(i);
            }
        }
        CHECK(limit->isEnabled() && limit->count() >= 2);   // a probability has limits to choose from
        const auto * prob = alias(picks.back());
        CHECK(prob != nullptr && std::string{prob->kind} == "Ensemble probability" && !std::string{prob->limit}.empty());
        dialog.close();
    }
    {   // every chart of every product kind of every model is reachable through the Hour, Type and Limit lists (each list offers what exists, and the three choose the chart)
        int variantKinds = 0, reachable = 0, unreachable = 0;
        for (const auto& def : GfsModels::all()) {
            std::vector<ProductPicker::Entry> entries;
            for (const auto& p : GfsChart::products()) {
                if (p.source == def.id) {
                    entries.push_back({p.id, p.label, GfsChart::category(p)});
                }
            }
            const auto kinds = ChecksPicker::kinds(entries);
            for (size_t k = 0; k < kinds.size(); k++) {
                if (kinds[k].parts.empty()) {
                    continue;
                }
                variantKinds++;
                std::string shown = kinds[k].charts.front().id;
                ChecksPicker dialog{nullptr, def.id, entries, shown, {}, {}};
                std::string last = shown;
                dialog.onPick = [&last] (const std::string& id) { last = id; };
                const auto combos = dialog.findChildren<QComboBox *>();
                auto * box = combos[0];
                for (size_t c = 0; c < kinds[k].charts.size(); c++) {
                    const auto id = kinds[k].charts[c].id;
                    for (int i = 0; i < box->count(); i++) {   // this product's item
                        if (box->itemData(i, Qt::UserRole).isValid() && static_cast<size_t>(box->itemData(i, Qt::UserRole).toInt()) == k && box->currentIndex() != i) {
                            box->setCurrentIndex(i);
                        }
                    }
                    for (size_t a = 0; a < 3; a++) {   // the hour, then the type, then the limit (each list is made from the ones before it)
                        auto * axis = combos[2 + a];
                        for (int i = 0; i < axis->count(); i++) {
                            if (axis->itemData(i).toString().toStdString() == kinds[k].parts[c][a] && axis->currentIndex() != i) {
                                axis->setCurrentIndex(i);
                            }
                        }
                    }
                    (last == id ? reachable : unreachable)++;
                    if (last != id) {
                        std::printf("  not reached: %s %s -> %s\n", def.id.c_str(), id.c_str(), last.c_str());
                    }
                }
                dialog.close();
            }
        }
        std::printf("product kinds %d, charts reached through their lists %d, not reached %d\n", variantKinds, reachable, unreachable);
        CHECK(unreachable == 0 && variantKinds > 10);
    }
    std::printf("kinds %d for %d charts over all models (%d of one chart)\n", kindsTotal, charts, single);
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
