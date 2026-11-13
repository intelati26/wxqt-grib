// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/ChartPreset.h"
#include <algorithm>
#include "gfs/ChartAlias.h"
#include "gfs/GfsChart.h"
#include "util/Utility.h"

namespace ChartPreset {

namespace {

const std::string pref{"MODEL_PRESETS"};

void saveAll(const std::vector<Preset>& presets) {
    std::string text;
    for (const auto& p : presets) {
        text += (text.empty() ? "" : "\n") + line(p);
    }
    Utility::writePref(pref, text);
}

}  // namespace

std::string cleanName(const std::string& name) {
    std::string out;
    for (const char c : name) {
        if (c != '|' && c != '\n' && c != '\r') {
            out += c;
        }
    }
    const auto first = out.find_first_not_of(" \t");
    if (first == std::string::npos) {
        return {};
    }
    return out.substr(first, out.find_last_not_of(" \t") - first + 1);
}

std::string line(const Preset& preset) {
    return cleanName(preset.name) + "|" + preset.chart + "|" + ChartSelection::joinList(preset.overlays);
}

bool fromLine(const std::string& text, Preset& out) {
    const auto first = text.find('|');
    const auto second = first == std::string::npos ? std::string::npos : text.find('|', first + 1);
    if (second == std::string::npos || text.find('|', second + 1) != std::string::npos) {
        return false;
    }
    const auto name = text.substr(0, first);
    const auto chart = text.substr(first + 1, second - first - 1);
    if (name.empty() || name != cleanName(name) || chart.empty()) {
        return false;
    }
    out.name = name;
    out.chart = chart;
    out.overlays = ChartSelection::splitList(text.substr(second + 1));
    return true;
}

Preset fromSelection(const std::string& name, const ChartSelection& selection) {
    return {cleanName(name), selection.param, selection.overlays};
}

std::vector<Preset> load() {
    std::vector<Preset> out;
    const auto text = Utility::readPref(pref, "");
    for (size_t at = 0; at < text.size();) {
        auto end = text.find('\n', at);
        end = end == std::string::npos ? text.size() : end;
        Preset p;
        if (fromLine(text.substr(at, end - at), p)) {
            out.push_back(p);
        }
        at = end + 1;
    }
    return out;
}

void put(const Preset& preset) {
    auto all = load();
    Preset clean{cleanName(preset.name), preset.chart, preset.overlays};
    if (clean.name.empty() || clean.chart.empty()) {
        return;
    }
    const auto existing = std::find_if(all.begin(), all.end(), [&clean] (const Preset& p) { return p.name == clean.name; });
    if (existing != all.end()) {
        *existing = clean;
    } else {
        all.push_back(clean);
    }
    saveAll(all);
}

bool erase(const std::string& name) {
    auto all = load();
    const auto before = all.size();
    all.erase(std::remove_if(all.begin(), all.end(), [&name] (const Preset& p) { return p.name == name; }), all.end());
    if (all.size() == before) {
        return false;
    }
    saveAll(all);
    return true;
}

Resolved resolve(const Preset& preset, const std::string& model) {
    Resolved r;
    r.selection.model = model;
    std::string chart;
    if (GfsChart::product(preset.chart, model) != nullptr) {
        chart = preset.chart;
    } else if (const auto * want = ChartAlias::find(preset.chart); want != nullptr && !ChartAlias::oneOff(*want)) {
        for (const auto& product : GfsChart::products()) {   // the model's own id for the same checks, when there is exactly one
            if (product.source != model) {
                continue;
            }
            const auto * have = ChartAlias::find(product.id);
            if (have != nullptr && ChartAlias::sameChecks(*want, *have)) {
                if (!chart.empty()) {
                    chart.clear();   // two charts of the model read the same: no way to choose
                    break;
                }
                chart = product.id;
            }
        }
    }
    if (chart.empty()) {
        r.dropped = preset.overlays;
        return r;
    }
    r.found = true;
    r.otherId = chart != preset.chart;
    r.selection.param = chart;
    std::vector<std::string> offered;
    for (const auto& choice : GfsChart::overlayChoices(model)) {
        offered.push_back(choice.id);
    }
    for (const auto& id : preset.overlays) {
        if (std::find(offered.begin(), offered.end(), id) != offered.end()) {
            r.selection.overlays.push_back(id);
        } else {
            r.dropped.push_back(id);
        }
    }
    return r;
}

}  // namespace ChartPreset
