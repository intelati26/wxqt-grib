// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityModelSounding.h"
#include <cmath>
#include <map>
#include <vector>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include "models/UtilityGrib.h"
#include "util/To.h"

namespace {
    using std::string;
    using std::vector;

    constexpr double kelvin = 273.15;
    constexpr double msToKnots = 1.0 / 0.514444;

    const vector<int>& levels() {
        static const vector<int> list = [] {
            vector<int> out;
            for (int mb = 1000; mb >= 100; mb -= 25) {
                out.push_back(mb);
            }
            return out;
        }();
        return list;
    }

    UtilityGrib::Field field(const string& key, const string& idxMatch, const string& product) {
        UtilityGrib::Field f;
        f.label = key;
        f.key = key;
        f.idxMatch = idxMatch;
        f.product = product;
        return f;
    }

    string levelKey(const string& variable, int mb) { return "snd_" + variable + "_" + To::string(mb); }

    // GDAL's GRIB driver normally hands temperatures back in Celsius already; accept Kelvin too
    double toCelsius(double value) { return value > 150.0 ? value - kelvin : value; }

    // dewpoint-depression-safe wind conversion: from-direction degrees and speed in knots
    void windFromComponents(double u, double v, double& dir, double& speed) {
        speed = std::hypot(u, v) * msToKnots;
        dir = std::fmod(std::atan2(-u, -v) * 180.0 / M_PI + 360.0, 360.0);
    }
}

namespace UtilityModelSounding {

bool buildProfile(const string& dateStr, const string& cycle, const string& forecastHour, double lon, double lat,
                  SoundingProfile& out, string& status, double radiusKm) {
    const auto binDir = UtilityGrib::gdalBinDir();
    if (binDir.empty()) {
        status = "GDAL not found - install the 'gdal' package";
        return false;
    }

    // the records wanted, in a fixed order that the merged file's bands follow
    vector<UtilityGrib::Field> wanted;
    const vector<std::pair<string, string>> surface = {
        {"snd_psfc", ":PRES:surface:"}, {"snd_zsfc", ":HGT:surface:"}, {"snd_t2", ":TMP:2 m above ground:"},
        {"snd_td2", ":DPT:2 m above ground:"}, {"snd_u10", ":UGRD:10 m above ground:"}, {"snd_v10", ":VGRD:10 m above ground:"}};
    for (const auto& [key, match] : surface) {
        wanted.push_back(field(key, match, "2dfld"));
    }
    const vector<std::pair<string, string>> variables = {{"t", "TMP"}, {"td", "DPT"}, {"z", "HGT"}, {"u", "UGRD"}, {"v", "VGRD"}};
    for (const int mb : levels()) {
        for (const auto& [name, grib] : variables) {
            wanted.push_back(field(levelKey(name, mb), ":" + grib + ":" + To::string(mb) + " mb:", "prslev"));
        }
    }

    std::map<string, string> paths;
    UtilityGrib::fetchFieldSlices(wanted, dateStr, cycle, forecastHour, paths);
    for (const auto& [key, match] : surface) {
        if (!paths.count(key)) {
            status = "Model sounding: could not fetch the surface field " + match + " for this run/hour";
            return false;
        }
    }

    // keep only levels whose five records all arrived, then join the records into one
    // multi-band GRIB2 (concatenated messages are a valid GRIB2 file) so GDAL reads the
    // point once instead of once per record
    vector<string> order;   // keys, band order
    for (const auto& [key, match] : surface) {
        order.push_back(key);
    }
    vector<int> usableLevels;
    for (const int mb : levels()) {
        bool complete = true;
        for (const auto& [name, grib] : variables) {
            complete = complete && paths.count(levelKey(name, mb)) > 0;
        }
        if (complete) {
            usableLevels.push_back(mb);
            for (const auto& [name, grib] : variables) {
                order.push_back(levelKey(name, mb));
            }
        }
    }
    if (usableLevels.size() < 10) {
        status = "Model sounding: only " + To::string(static_cast<int>(usableLevels.size())) +
            " of " + To::string(static_cast<int>(levels().size())) + " pressure levels could be fetched - try again";
        return false;
    }

    const auto dir = QFileInfo{QString::fromStdString(paths.at(order.front()))}.absolutePath();
    const auto merged = dir + "/snd_" + QString::fromStdString(dateStr + cycle + "_" + forecastHour) + "_" +
        QString::number(order.size()) + ".grib2";
    if (!QFileInfo::exists(merged)) {
        QFile mergedFile{merged + ".part"};
        if (!mergedFile.open(QIODevice::WriteOnly)) {
            status = "Model sounding: cannot write " + merged.toStdString();
            return false;
        }
        for (const auto& key : order) {
            QFile piece{QString::fromStdString(paths.at(key))};
            if (!piece.open(QIODevice::ReadOnly)) {
                status = "Model sounding: cannot read the cached record " + key;
                return false;
            }
            mergedFile.write(piece.readAll());
        }
        mergedFile.close();
        QFile::rename(merged + ".part", merged);
    }

    // the points to read: the chosen one, or every 3 km cell within the radius
    vector<std::pair<double, double>> points;
    if (radiusKm <= 0.0) {
        points.emplace_back(lon, lat);
    } else {
        const double kmPerDegLat = 111.32;
        const double kmPerDegLon = kmPerDegLat * std::cos(lat * M_PI / 180.0);
        // about a dozen samples across the radius: the mean is the same to within noise, the read is far quicker
        const double spacing = std::max(3.0, radiusKm / 6.0);
        const int steps = static_cast<int>(std::floor(radiusKm / spacing));
        for (int iy = -steps; iy <= steps; iy += 1) {
            for (int ix = -steps; ix <= steps; ix += 1) {
                if (std::hypot(ix * spacing, iy * spacing) <= radiusKm) {
                    points.emplace_back(lon + ix * spacing / kmPerDegLon, lat + iy * spacing / kmPerDegLat);
                }
            }
        }
    }
    // gdallocationinfo reads "lon lat" lines from stdin and prints every band's value for each, in order
    QProcess process;
    process.start(QString::fromStdString(binDir) + "/gdallocationinfo", {"-valonly", "-wgs84", merged});
    QByteArray coordinates;
    for (const auto& [x, y] : points) {
        coordinates += QByteArray::number(x, 'f', 4) + " " + QByteArray::number(y, 'f', 4) + "\n";
    }
    process.write(coordinates);
    process.closeWriteChannel();
    if (!process.waitForFinished(120000) || process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        status = "Model sounding: gdallocationinfo failed: " + process.readAllStandardError().toStdString();
        return false;
    }
    vector<double> values;
    for (const auto& line : QString::fromUtf8(process.readAllStandardOutput()).split('\n', Qt::SkipEmptyParts)) {
        values.push_back(line.trimmed().toDouble());
    }
    if (values.size() != order.size() * points.size()) {
        status = "Model sounding: read " + To::string(static_cast<int>(values.size())) + " values for " +
            To::string(static_cast<int>(order.size() * points.size())) + " expected (is the point outside the model domain?)";
        return false;
    }
    // per record: the mean over the points that have a defined value
    std::map<string, double> value;
    for (size_t record = 0; record < order.size(); record += 1) {
        double sum = 0.0;
        int count = 0;
        for (size_t point = 0; point < points.size(); point += 1) {
            const double v = values[point * order.size() + record];
            if (v > -9000.0 && !std::isnan(v)) {
                sum += v;
                count += 1;
            }
        }
        value[order[record]] = count > 0 ? sum / count : -9999.0;
    }
    for (const auto& [key, v] : value) {
        if (v < -9000.0 && key.rfind("snd_t_", 0) != 0 && key.rfind("snd_td_", 0) != 0 && key.rfind("snd_z_", 0) != 0 &&
            key.rfind("snd_u_", 0) != 0 && key.rfind("snd_v_", 0) != 0) {
            status = "Model sounding: no model data at " + To::string(lat) + ", " + To::string(lon) + " (outside the domain?)";
            return false;
        }
    }

    // ---- assemble the column, surface first, dropping levels below the model ground ----
    const double psfc = value["snd_psfc"] / 100.0;   // Pa -> mb
    SoundingProfile profile;
    auto add = [&] (double p, double h, double t, double td, double u, double v) {
        double dir = -9999.0, speed = -9999.0;
        if (u > -9000.0 && v > -9000.0) {
            windFromComponents(u, v, dir, speed);
        }
        profile.pres.push_back(p);
        profile.hght.push_back(h);
        profile.tmpc.push_back(t);
        profile.dwpc.push_back(td);
        profile.wdir.push_back(dir);
        profile.wspd.push_back(speed);
    };
    add(psfc, value["snd_zsfc"], toCelsius(value["snd_t2"]), toCelsius(value["snd_td2"]), value["snd_u10"], value["snd_v10"]);
    for (const int mb : usableLevels) {
        if (mb >= psfc - 1.0) {
            continue;   // at or under the ground
        }
        const double t = value[levelKey("t", mb)], td = value[levelKey("td", mb)], z = value[levelKey("z", mb)];
        if (t < -9000.0 || td < -9000.0 || z < -9000.0) {
            continue;
        }
        add(mb, z, toCelsius(t), toCelsius(td), value[levelKey("u", mb)], value[levelKey("v", mb)]);
    }
    if (profile.pres.size() < 8) {
        status = "Model sounding: too few valid levels at this point";
        return false;
    }
    profile.station = To::string(lat) + "," + To::string(lon);
    profile.validTime = dateStr + "/" + cycle + "Z F" + forecastHour;
    profile.finalize();
    out = profile;
    status = "RRFS " + dateStr + " " + cycle + "Z F" + forecastHour + " at " + To::string(lat) + ", " + To::string(lon) +
        (radiusKm > 0.0 ? " (mean of " + To::string(static_cast<int>(points.size())) + " points within " + To::string(radiusKm) + " km)" : string{});
    return true;
}

}  // namespace UtilityModelSounding
