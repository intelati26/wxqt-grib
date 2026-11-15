// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSCHARTINTERNAL_H
#define GFSCHARTINTERNAL_H

#include <string>
#include <vector>
#include "gfs/GfsChart.h"

// What the files that make the chart catalogue share (GfsChart.cpp and the GfsProducts*.cpp, one for each family of models): small helpers, and the functions that add a family's charts to the
// list. Not for use outside the chart code.
namespace GfsChart {
namespace detail {

inline constexpr double pi = 3.14159265358979323846;
inline constexpr double msToKnots = 1.943844;

// a grid of the fetched ones by its key (an empty grid when it was not fetched)
inline GfsGrid::Grid pick(const Grids& g, const std::string& key) {
    const auto found = g.find(key);
    return found == g.end() ? GfsGrid::Grid{} : found->second;
}

// a record to fetch, by the key it is kept under
inline GfsData::Want want(const std::string& key, const std::string& variable, const std::string& level) {
    return {key, variable, level, ""};
}

// the charts of one family of models, added to the list
void geosProducts(std::vector<Product>& p);

}  // namespace detail
}  // namespace GfsChart

#endif
