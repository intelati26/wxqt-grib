// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/ChartAlias.h"
#include "gfs/GfsChart.h"
#include <algorithm>
#include <map>
#include <string>

namespace ChartAlias {

namespace {

std::string keyOf(const Entry& e) {
    std::string k;
    for (const char * part : {e.family, e.field, e.level, e.mode, e.chance, e.detail, e.values, e.contours}) {
        k += part;
        k += '|';
    }
    k += e.barbs ? 'b' : '-';
    k += e.streamlines ? 's' : '-';
    k += e.bands ? 't' : '-';
    return k;
}

struct Index {
    std::map<std::string, const Entry *> byId;
    std::map<std::string, std::vector<const Entry *>> byChecks;
    Index() {
        for (const auto& e : table()) {
            byId[e.id] = &e;
            byChecks[keyOf(e)].push_back(&e);
        }
    }
};

const Index& index() {
    static const Index i;
    return i;
}

}  // namespace

const Entry * find(const std::string& id) {
    const auto& ids = index().byId;
    const auto it = ids.find(id);
    return it == ids.end() ? nullptr : it->second;
}

std::string idFor(const Entry& like) {
    const auto& by = index().byChecks;
    const auto it = by.find(keyOf(like));
    return it != by.end() && it->second.size() == 1 ? std::string{it->second.front()->id} : std::string{};
}

bool sameChecks(const Entry& a, const Entry& b) {
    return keyOf(a) == keyOf(b);
}

std::string idFor(const Entry& like, const std::string& model) {
    if (GfsChart::product(like.id, model) != nullptr) {
        return like.id;
    }
    const auto key = keyOf(like);
    for (const auto& product : GfsChart::products()) {   // (the model's list order: the first of two ids for one chart)
        if (product.source != model) {
            continue;
        }
        if (const auto * have = find(product.id); have != nullptr && keyOf(*have) == key) {
            return product.id;
        }
    }
    return {};
}

int ambiguousIn(const std::string& model) {
    std::map<std::string, int> count;
    for (const auto& product : GfsChart::products()) {
        if (const auto * have = find(product.id); product.source == model && have != nullptr) {
            count[keyOf(*have)]++;
        }
    }
    int n = 0;
    for (const auto& [key, c] : count) {
        n += c > 1 ? c : 0;
    }
    return n;
}

int ambiguous() {
    int n = 0;
    for (const auto& [key, list] : index().byChecks) {
        if (list.size() > 1) {
            n += static_cast<int>(list.size());
        }
    }
    return n;
}

bool oneOff(const Entry& e) {
    return std::string{e.family} == "One-off";
}

}  // namespace ChartAlias
