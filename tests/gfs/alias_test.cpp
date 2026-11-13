// Tests of the chart alias table: it holds exactly the charts the program has (every id, once, with the number of models that have it), each id gives its entry back and each set of checks
// gives its id back (the round trip), and it says how many ids cannot be told apart by their checks.
#include <QSettings>
#include "util/MyApplication.h"
#include <cstdio>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include "gfs/ChartAlias.h"
#include "gfs/GfsChart.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    MyApplication::preferences = new QSettings{"/tmp/wxqt_test_prefs.ini", QSettings::IniFormat};
    std::map<std::string, int> programModels;   // id -> how many models have it
    for (const auto& product : GfsChart::products()) {
        programModels[product.id]++;
    }
    std::set<std::string> ids;
    std::map<std::string, int> groupSize;
    for (const auto& e : ChartAlias::table()) {
        CHECK(*e.id != '\0' && ids.insert(e.id).second);   // named, and named once
        CHECK(ChartAlias::find(e.id) == &e);               // the id gives its entry back
        CHECK(*e.family != '\0' && *e.mode != '\0' && *e.group != '\0');
        groupSize[e.group]++;
    }
    int missing = 0, extra = 0, wrongModels = 0, wrongSingle = 0;
    for (const auto& [id, n] : programModels) {
        const auto * e = ChartAlias::find(id);
        if (!e) {
            std::printf("  not in the table: %s\n", id.c_str());
            missing++;
        } else if (e->models != n) {
            std::printf("  %s: %d models in the table, %d in the program\n", id.c_str(), e->models, n);
            wrongModels++;
        }
    }
    for (const auto& e : ChartAlias::table()) {
        if (!programModels.count(e.id)) {
            std::printf("  not a chart of the program: %s\n", e.id);
            extra++;
        }
        if (e.single != (groupSize[e.group] == 1)) {
            wrongSingle++;
        }
    }
    CHECK(missing == 0);
    CHECK(extra == 0);
    CHECK(wrongModels == 0);
    CHECK(wrongSingle == 0);
    // the round trip: checks -> id gives the id back, except where several ids have the same checks (counted, not hidden)
    int roundTrip = 0, same = 0;
    for (const auto& e : ChartAlias::table()) {
        const auto back = ChartAlias::idFor(e);
        if (back == e.id) {
            roundTrip++;
        } else if (back.empty()) {
            same++;
        } else {
            CHECK(false);   // another id came back: the index is wrong
        }
    }
    std::printf("entries %zu, programs ids %zu, round trip exact %d, ids whose checks another id shares %d (ambiguous() says %d), one-offs %d\n", ChartAlias::table().size(), programModels.size(), roundTrip, same,
                ChartAlias::ambiguous(), static_cast<int>(std::count_if(ChartAlias::table().begin(), ChartAlias::table().end(), [] (const ChartAlias::Entry& e) { return ChartAlias::oneOff(e); })));
    CHECK(same == ChartAlias::ambiguous());
    // by model: ids that share their checks within one model are charts that read the same (a model with two ids for one chart); the rest of the 'same' above are the same chart under another id on
    // another model ("2m_tmpc" and "temp_2m"), which the model-aware lookup resolves
    std::set<std::string> models;
    for (const auto& product : GfsChart::products()) {
        models.insert(product.source);
    }
    int inModel = 0, wrong = 0, otherId = 0;
    std::set<std::string> twice;
    for (const auto& model : models) {
        const int n = ChartAlias::ambiguousIn(model);
        inModel += n;
        if (n > 0) {
            twice.insert(model);
        }
    }
    for (const auto& product : GfsChart::products()) {
        const auto * e = ChartAlias::find(product.id);
        CHECK(e != nullptr);
        if (e == nullptr) {
            continue;
        }
        const auto got = ChartAlias::idFor(*e, product.source);
        wrong += got != product.id ? 1 : 0;   // a model has its own chart, whatever the checks say
        for (const auto& model : models) {      // on any model: its own id, or an id of that model, or none
            const auto there = ChartAlias::idFor(*e, model);
            CHECK(there.empty() || GfsChart::product(there, model) != nullptr);
            otherId += !there.empty() && there != product.id ? 1 : 0;
        }
    }
    CHECK(wrong == 0);
    std::printf("within a model: %d charts share their checks with another chart of the same model (%zu models); another model's id for a chart: %d\n", inModel, twice.size(), otherId);
    for (const auto& model : twice) {
        CHECK(model == "HRRR" || ChartAlias::ambiguousIn(model) <= 4);   // (the few true duplicates: 10m_wnd and mslp_10m_wind)
    }
    std::printf(failures == 0 ? "OK\n" : "FAILED: %d\n", failures);
    return failures == 0 ? 0 : 1;
}
