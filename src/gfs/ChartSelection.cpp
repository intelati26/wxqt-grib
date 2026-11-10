// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/ChartSelection.h"
#include <cstdlib>
#include "gfs/GfsChart.h"

std::string ChartSelection::joinList(const std::vector<std::string>& list) {
    std::string out;
    for (const auto& item : list) {
        out += (out.empty() ? "" : ",") + item;
    }
    return out;
}

std::vector<std::string> ChartSelection::splitList(const std::string& text) {
    std::vector<std::string> out;
    for (size_t at = 0; at < text.size();) {
        auto end = text.find(',', at);
        end = end == std::string::npos ? text.size() : end;
        if (end > at) {
            out.push_back(text.substr(at, end - at));
        }
        at = end + 1;
    }
    return out;
}

bool ChartSelection::usable() const {
    return GfsChart::product(param, model) != nullptr;
}

std::string ChartSelection::line() const {
    return model + "|" + param + "|" + joinList(overlays) + "|" + joinList(overlayModels) + "|" + member + "|" + (sigma ? "1" : "0") + "|" + std::to_string(changeBack) + "|" + (maxDay ? "1" : "0");
}

bool ChartSelection::fromLine(const std::string& line, ChartSelection& out) {
    std::vector<std::string> p;
    for (size_t at = 0; at <= line.size();) {
        auto end = line.find('|', at);
        end = end == std::string::npos ? line.size() : end;
        p.push_back(line.substr(at, end - at));
        at = end + 1;
    }
    if (p.size() < 8 || GfsChart::product(p[1], p[0]) == nullptr) {
        return false;
    }
    out.model = p[0];
    out.param = p[1];
    out.overlays = splitList(p[2]);
    out.overlayModels = splitList(p[3]);
    out.member = p[4];
    out.sigma = p[5] == "1";
    out.changeBack = std::atoi(p[6].c_str());
    out.maxDay = p[7] == "1";
    return true;
}

GfsRender::Variant ChartSelection::variant(const std::vector<int>& hours, int hour) const {
    GfsRender::Variant v;
    if (maxDay) {   // the largest of the chart's fill over the 24 hours ending at this hour
        for (const int h : hours) {
            if (h >= hour - 24 && h <= hour) {
                v.hours.push_back(h);
            }
        }
        if (v.hours.size() >= 2) {
            v.kind = GfsRender::Variant::Kind::Max;
        } else {
            v.hours.clear();
        }
    } else if (changeBack > 0) {
        v.kind = GfsRender::Variant::Kind::Change;
        v.hoursBack = changeBack;
    }
    v.member = member;
    v.standardDeviations = sigma;
    if (v.kind == GfsRender::Variant::Kind::None) {   // (the other models' lines go over the plain chart only)
        v.overlayModels = overlayModels;
    }
    return v;
}
