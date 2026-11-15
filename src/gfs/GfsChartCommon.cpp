// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// What the chart catalogue files share that is more than a line: the number written plainly, and the table of the charts made for values the user chooses.
#include "gfs/GfsChartInternal.h"
#include <QString>

namespace GfsChart {
namespace detail {

// the number without trailing zeros: 2, 0.25
std::string trimmed(double value) {
    auto text = QString::number(value, 'f', 2);
    while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
        text.chop(1);
    }
    return text.toStdString();
}

// template id -> a function making the product for the chosen values (filled when the registry is built)
std::map<std::string, std::function<Product(const std::vector<double>&)>>& generators() {
    static std::map<std::string, std::function<Product(const std::vector<double>&)>> table;
    return table;
}

std::string generatedId(const std::string& templateId, const std::vector<double>& values) {
    std::string id = "gen~" + templateId;
    for (const double v : values) {
        id += "~" + trimmed(v);
    }
    return id;
}

}  // namespace detail
}  // namespace GfsChart
