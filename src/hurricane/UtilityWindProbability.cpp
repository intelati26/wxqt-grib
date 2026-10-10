// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityWindProbability.h"
#include <algorithm>
#include <cstdlib>
#include <map>
#include "hurricane/UtilityShapefile.h"
#include "util/UtilityZip.h"

bool UtilityWindProbability::parseBand(const string& label, int& low, int& high) {
    if (label.size() < 3 || label.back() != '%') {
        return false;
    }
    const string body = label.substr(0, label.size() - 1);
    if (body.front() == '<') {
        low = 0;
        high = std::atoi(body.c_str() + 1);
        return high > 0;
    }
    if (body.front() == '>') {
        low = std::atoi(body.c_str() + 1);
        high = 100;
        return low > 0;
    }
    const auto dash = body.find('-');
    if (dash == string::npos) {
        return false;
    }
    low = std::atoi(body.c_str());
    high = std::atoi(body.c_str() + dash + 1);
    return high > low;
}

string UtilityWindProbability::Chance::text() const {
    if (!covered) {
        return "none";
    }
    if (low == 0) {
        return "<" + std::to_string(high) + "%";
    }
    return high >= 100 ? ">" + std::to_string(low) + "%" : std::to_string(low) + "-" + std::to_string(high) + "%";
}

UtilityWindProbability::Map UtilityWindProbability::parse(const string& zip) {
    Map map;
    std::map<string, string> files;
    if (!UtilityZip::read(zip, files)) {
        return map;
    }
    static const char * names[3] = {"wsp34knt", "wsp50knt", "wsp64knt"};
    for (int k = 0; k < 3; k++) {
        for (const auto& [name, data] : files) {
            if (name.find(names[k]) == string::npos || name.size() < 4 || name.compare(name.size() - 4, 4, ".shp") != 0) {
                continue;
            }
            const auto d = files.find(name.substr(0, name.size() - 3) + "dbf");
            vector<UtilityShapefile::Feature> features;
            if (d == files.end() || !UtilityShapefile::parse(data, d->second, features)) {
                continue;
            }
            if (map.cycle.empty() && name.size() >= 10) {
                map.cycle = name.substr(0, 10);
            }
            for (const auto& f : features) {
                Band band;
                const auto label = f.attributes.find("PERCENTAGE");
                if (label == f.attributes.end() || !parseBand(label->second, band.low, band.high)) {
                    continue;
                }
                band.rings = f.parts;
                map.bands[k].push_back(std::move(band));
            }
            std::sort(map.bands[k].begin(), map.bands[k].end(), [] (const Band& a, const Band& b) { return a.low < b.low; });
        }
    }
    map.ok = !map.bands[0].empty();
    return map;
}

bool UtilityWindProbability::inside(const Ring& ring, double lat, double lon) {
    bool in = false;
    for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
        const auto& [x1, y1] = ring[i];
        const auto& [x2, y2] = ring[j];
        if ((y1 > lat) != (y2 > lat) && lon < (x2 - x1) * (lat - y1) / (y2 - y1) + x1) {
            in = !in;
        }
    }
    return in;
}

UtilityWindProbability::Chance UtilityWindProbability::at(const Map& map, int knotsIndex, double lat, double lon) {
    Chance chance;
    if (knotsIndex < 0 || knotsIndex > 2) {
        return chance;
    }
    // a band's polygon may have holes (the bands are rings round the storm's track) or may hold every higher band too: count the rings the point is
    // in, an odd number meaning it is inside the polygon; the highest band that holds the point wins
    for (const auto& band : map.bands[knotsIndex]) {
        int count = 0;
        for (const auto& ring : band.rings) {
            count += ring.size() >= 3 && inside(ring, lat, lon) ? 1 : 0;
        }
        if (count % 2 == 1) {
            chance.low = band.low;
            chance.high = band.high;
            chance.covered = true;
        }
    }
    return chance;
}
