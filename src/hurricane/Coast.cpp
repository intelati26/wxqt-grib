// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/Coast.h"
#include <cmath>
#include <cstring>
#include <QFile>
#include "common/GlobalVariables.h"
#include "util/UtilityIO.h"

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

const std::vector<std::vector<std::pair<float, float>>>& Coast::worldLines() {
    static const auto data = [] {
        std::vector<std::vector<std::pair<float, float>>> all;
        QFile file{":/res/world_coast.bin"};
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

const std::vector<std::vector<std::pair<float, float>>>& Coast::borders() {
    static const auto lines = [] {
        auto all = worldLines();
        // the state lines of the radar screen's resources (statev2.bin: big-endian float segments of latitude, west longitude)
        const auto raw = UtilityIO::readBinaryFileFromResource(GlobalVariables::resDir + "statev2.bin");
        const auto floatAt = [&raw] (int offset) {
            const unsigned char b[4]{static_cast<unsigned char>(raw[offset + 3]), static_cast<unsigned char>(raw[offset + 2]),
                                     static_cast<unsigned char>(raw[offset + 1]), static_cast<unsigned char>(raw[offset])};
            float value = 0.0f;
            std::memcpy(&value, b, 4);
            return value;
        };
        for (int i = 0; i + 15 < static_cast<int>(raw.size()); i += 16) {
            const float lat1 = floatAt(i), lon1 = floatAt(i + 4), lat2 = floatAt(i + 8), lon2 = floatAt(i + 12);
            if ((lat1 == lat2 && lon1 == lon2) || lat1 < 5.0f || lat1 > 85.0f || lat2 < 5.0f || lat2 > 85.0f) {
                continue;
            }
            all.push_back({{-lon1, lat1}, {-lon2, lat2}});
        }
        return all;
    }();
    return lines;
}
