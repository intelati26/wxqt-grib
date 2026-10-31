// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/ChartHours.h"
#include "gfs/WpcPppLoader.h"
#include <algorithm>
#include <map>
#include <QDateTime>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "spcrefs/SpcRefsLoader.h"

std::vector<int> ChartHours::modelHours(const std::string& model, int cycle) {
    std::vector<int> out;
    const auto * def = GfsModels::find(model);
    if (def == nullptr) {
        return out;
    }
    const int last = def->runLength ? def->runLength(cycle) : 100000;
    for (const auto& range : def->hours) {
        for (int h = range.from; h <= range.to; h += std::max(1, range.step)) {
            if (h <= last) {
                out.push_back(h);
            }
        }
    }
    return out;
}

ChartHours::Rules ChartHours::rules(const std::string& model, const std::string& param, int cycle, const std::vector<int>& hours) {
    Rules out;
    const auto * def = GfsModels::find(model);
    std::vector<int> kept = hours;
    if (def != nullptr && def->runLength) {   // a model whose runs differ in length (the RRFS: 84 hours from 00, 06, 12 and 18Z, 18 from the others)
        const int last = def->runLength(cycle);
        kept.erase(std::remove_if(kept.begin(), kept.end(), [last] (int h) { return h > last; }), kept.end());
        if (kept.empty()) {
            kept = hours;
        }
    }
    const auto * product = GfsChart::product(param, model);
    if (product != nullptr && product->periodHours > 0) {   // a chart of a period that ends at the hour: the hours that are a whole number of periods, ending at the hour of the day it needs
        out.period = product->periodHours;
        out.endHourUtc = product->periodEndHourUtc;
        std::vector<int> fit;
        for (const int h : kept) {
            if (h >= out.period && h % out.period == 0 && (out.endHourUtc < 0 || (cycle + h) % 24 == out.endHourUtc)) {
                fit.push_back(h);
            }
        }
        if (!fit.empty() && fit.size() < kept.size()) {
            kept = fit;
            out.periodCut = true;
        }
    }
    out.hours = kept;
    return out;
}

std::vector<int> ChartHours::present(GfsRender::Session& session, const std::string& model, const std::string& param, const std::string& runId, const std::vector<int>& hours, const std::string& storm) {
    if (runId.size() != 10) {
        return {};
    }
    if (model == "SPCREFS") {   // the store's time axis of each field the chart is made of
        const auto * product = GfsChart::product(param, model);
        std::vector<std::string> variables{param};
        if (product != nullptr) {
            variables.clear();
            for (const auto& want : product->wants) {
                variables.push_back(want.variable);
            }
        }
        const auto have = SpcRefsLoader::fieldHours(GfsData::Run{runId.substr(0, 8), runId.substr(8, 2)}, variables);
        std::vector<int> out;
        for (const int h : hours) {
            if (std::find(have.begin(), have.end(), h) != have.end()) {
                out.push_back(h);
            }
        }
        return have.empty() ? std::vector<int>{} : out;
    }
    if (model == "WPC-PPP") {   // the portal's folder of the run lists the periods' ends it has for the product
        const auto * product = GfsChart::product(param, model);
        if (product == nullptr || product->wants.empty()) {
            return {};
        }
        const auto have = WpcPppLoader::hours(GfsData::Run{runId.substr(0, 8), runId.substr(8, 2)}, product->wants.front().variable);
        std::vector<int> out;
        for (const int h : hours) {
            if (std::find(have.begin(), have.end(), h) != have.end()) {
                out.push_back(h);
            }
        }
        return out;
    }
    return GfsRender::availableHours(session, model, param, runId, hours, storm);
}

std::set<std::string> ChartHours::absent(GfsRender::Session& session, const std::string& model, const std::string& runId, const std::vector<std::string>& charts) {
    std::set<std::string> out;
    const auto * def = GfsModels::find(model);
    if (def == nullptr || runId.size() != 10 || model == "SPCREFS" || GfsModels::isStormModel(model) || def->source("").custom) {
        return out;
    }
    if (std::any_of(def->hours.begin(), def->hours.end(), [] (const GfsModels::Hours& range) { return range.step <= 1 && range.to > range.from; })) {   // a model with a field every hour (the NBM, the HRRR, the RRFS) has fields of
        return out;                                                                                                                               // single hours too (a day's maximum at its 31st): two hours say nothing of them
    }
    const int cycle = std::atoi(runId.substr(8, 2).c_str());
    // two hours of the run, the first of the model's from the 24th (an hour with the accumulations in it): a chart is asked for its variables and levels, whatever their forecast kind, so a field that
    // the run has only at some hours (a maximum of the day, a 12 hour total) is not taken for one it lacks
    std::vector<int> probe;
    for (const int h : modelHours(model, cycle)) {
        if (h >= 24 && probe.size() < 2) {
            probe.push_back(h);
        }
    }
    if (probe.empty()) {
        return out;
    }
    std::map<std::string, bool> have;
    bool readable = false;
    for (const auto& id : charts) {
        bool any = false;
        for (const int h : probe) {
            any = any || GfsRender::hasFields(session, model, id, runId, h);
        }
        have[id] = any;
        readable = readable || any;
    }
    if (!readable) {   // nothing of the run was found: the index could not be read, or the run is not there yet: no word on any chart
        return out;
    }
    for (const auto& [id, there] : have) {
        const auto * product = GfsChart::product(id, model);
        if (!there && product != nullptr && !GfsChart::needs(*product, probe.front()).empty()) {
            out.insert(id);
        }
    }
    return out;
}

std::string ChartHours::Book::key(const std::string& model, const std::string& runId, const std::string& param, const std::string& storm) {
    return model + "|" + runId + "|" + param + (storm.empty() ? std::string{} : "|" + storm);
}

const ChartHours::Book::Entry * ChartHours::Book::find(const std::string& key) const {
    const auto found = entries.find(key);
    return found == entries.end() ? nullptr : &found->second;
}

bool ChartHours::Book::wantsAsking(const std::string& key) const {
    const auto found = entries.find(key);
    return found == entries.end() || (!found->second.complete && QDateTime::currentSecsSinceEpoch() - found->second.when > 300);
}

bool ChartHours::Book::begin(const std::string& key) {
    return busy.emplace(key, true).second;
}

void ChartHours::Book::put(const std::string& key, const std::vector<int>& present, size_t asked) {
    busy.erase(key);
    entries[key] = {present, QDateTime::currentSecsSinceEpoch(), present.size() == asked};
}
