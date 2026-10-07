// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/Coast.h"
#include <cmath>
#include <QFile>

const std::vector<std::vector<std::pair<float, float>>>& Coast::lines() {
    static const auto data = [] {
        std::vector<std::vector<std::pair<float, float>>> all;
        QFile file{":/res/nhc_basins.bin"};
        if (file.open(QIODevice::ReadOnly)) {
            const auto bytes = file.readAll();
            const auto * values = reinterpret_cast<const float *>(bytes.constData());
            std::vector<std::pair<float, float>> line;
            for (qsizetype i = 0; i + 1 < bytes.size() / 4; i += 2) {
                if (std::isnan(values[i])) {
                    all.push_back(std::move(line));
                    line.clear();
                } else {
                    line.emplace_back(values[i], values[i + 1]);
                }
            }
        }
        return all;
    }();
    return data;
}
