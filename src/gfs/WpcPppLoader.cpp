// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/WpcPppLoader.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include "gfs/GfsGrid.h"
#include "hurricane/UtilityShapefile.h"
#include "util/UtilityIO.h"
#include "util/UtilityZip.h"

namespace {
    const std::string base = "https://ftp-wpc.ncep.noaa.gov/prob_precip_portal/co/gis/";
    // the grid the shapes are filled onto: the portal's domain with room to spare
    constexpr double west = -130.0, east = -60.0, south = 20.0, north = 55.0, step = 0.05;
    constexpr double earth = 6378137.0, pi = 3.14159265358979323846;

    struct Parts {
        std::string duration, element, kind;
    };

    bool split(const std::string& variable, Parts& out) {
        const auto list = QString::fromStdString(variable).split('|');
        if (list.size() != 3) {
            return false;
        }
        out = {list[0].toStdString(), list[1].toStdString(), list[2].toStdString()};
        return true;
    }

    // the part of the file's name that says what it is: "expected_qpf_in", "50th_percentile_sn_in", "prob_zr_exceeding_0p25_in"
    std::string stem(const Parts& p) {
        if (p.kind == "expected") {
            return "expected_" + p.element + "_in";
        }
        if (p.kind.compare(0, 5, "prob_") == 0) {
            return "prob_" + p.element + "_exceeding_" + p.kind.substr(5) + "_in";
        }
        return p.kind.substr(1) + "th_percentile_" + p.element + "_in";   // "p05" -> "05th_percentile"
    }

    std::string pad3(int hour) {
        char text[8];
        std::snprintf(text, sizeof text, "%03d", hour);
        return text;
    }

    std::string folder(const GfsData::Run& run, const Parts& p) {
        return base + "ppp_gis_co_" + p.duration + "hr_" + run.id() + "/";
    }

    // the amounts are inches in the files and the charts' ramps millimeters (rain, ice) and centimeters (snow): the display converts to the user's units
    double toChartUnits(const Parts& p) {
        if (p.kind.compare(0, 5, "prob_") == 0) {
            return 1.0;
        }
        return p.element == "sn" ? 2.54 : 25.4;
    }

    double latitudeOf(double y) {
        return std::atan(std::sinh(y / earth)) * 180.0 / pi;
    }

    // The bands onto the grid: the lowest value first, so that a higher band lies over the lower ones; where no band lies is zero (below the lowest contour). A band is a set of rings (the even-odd
    // rule leaves holes out).
    bool fill(const std::vector<UtilityShapefile::Feature>& features, double factor, GfsGrid::Grid& out, std::string& error) {
        std::set<double> levels;
        for (const auto& f : features) {
            const auto v = f.attributes.find("value");
            if (f.shapeType == 5 && v != f.attributes.end()) {
                levels.insert(std::atof(v->second.c_str()));
            }
        }
        if (levels.size() > 250) {
            error = "the portal's file has too many levels";
            return false;
        }
        const std::vector<double> sorted(levels.begin(), levels.end());
        out.columns = static_cast<int>(std::lround((east - west) / step)) + 1;
        out.rows = static_cast<int>(std::lround((north - south) / step)) + 1;
        out.lon0 = west;
        out.lat0 = north;
        out.step = step;
        QImage image(out.columns, out.rows, QImage::Format_Grayscale8);
        image.fill(0);
        QPainter painter{&image};
        painter.setPen(Qt::NoPen);
        for (size_t i = 0; i < sorted.size(); i++) {
            const int gray = static_cast<int>(i) + 1;
            painter.setBrush(QColor{gray, gray, gray});
            for (const auto& f : features) {
                const auto v = f.attributes.find("value");
                if (f.shapeType != 5 || v == f.attributes.end() || std::atof(v->second.c_str()) != sorted[i]) {
                    continue;
                }
                QPainterPath path;
                path.setFillRule(Qt::OddEvenFill);
                for (const auto& ring : f.parts) {
                    QPolygonF polygon;
                    for (const auto& [x, y] : ring) {
                        polygon << QPointF{(x / earth * 180.0 / pi - west) / step, (north - latitudeOf(y)) / step};
                    }
                    path.addPolygon(polygon);
                    path.closeSubpath();
                }
                painter.drawPath(path);
            }
        }
        painter.end();
        out.values.assign(static_cast<size_t>(out.columns) * static_cast<size_t>(out.rows), 0.0f);
        for (int r = 0; r < out.rows; r++) {
            const auto * line = image.constScanLine(r);
            for (int c = 0; c < out.columns; c++) {
                if (line[c] > 0) {
                    out.values[static_cast<size_t>(r) * static_cast<size_t>(out.columns) + static_cast<size_t>(c)] = static_cast<float>(sorted[static_cast<size_t>(line[c]) - 1] * factor);
                }
            }
        }
        return true;
    }

    bool load(const GfsData::Run& run, int hour, const GfsData::Want& want, GfsGrid::Grid& out, std::string& error) {
        Parts p;
        if (!split(want.variable, p)) {
            error = "not a portal product: " + want.variable;
            return false;
        }
        const auto url = folder(run, p) + "ppp_co_" + p.duration + "hr_" + stem(p) + "_" + run.id() + "f" + pad3(hour) + ".zip";
        const auto zip = UtilityIO::downloadAsByteArray(url);
        if (zip.size() < 100 || !zip.startsWith("PK")) {
            error = "the Weather Prediction Center has no " + p.duration + " hour product for f" + pad3(hour) + " in the " + run.cycle + "Z run (yet)";
            return false;
        }
        std::map<std::string, std::string> files;
        UtilityZip::read(std::string{zip.constData(), static_cast<size_t>(zip.size())}, files);
        std::string shp, dbf;
        for (const auto& [name, bytes] : files) {
            if (name.size() > 4 && name.compare(name.size() - 4, 4, ".shp") == 0) {
                shp = bytes;
            } else if (name.size() > 4 && name.compare(name.size() - 4, 4, ".dbf") == 0) {
                dbf = bytes;
            }
        }
        std::vector<UtilityShapefile::Feature> features;
        if (shp.empty() || dbf.empty() || !UtilityShapefile::parse(shp, dbf, features)) {
            error = "could not read the portal's shapefile";
            return false;
        }
        return fill(features, toChartUnits(p), out, error);
    }

    std::mutex listingMutex;
    struct Listing {
        time_t when{0};
        std::set<std::string> names;   // the files of the folder, without ".zip"
    };
    std::map<std::string, Listing>& listings() {
        static std::map<std::string, Listing> list;
        return list;
    }
}

void WpcPppLoader::registerLoader() {
    GfsData::setLoader("WPC-PPP", load);
}

std::vector<int> WpcPppLoader::hours(const GfsData::Run& run, const std::string& variable) {
    Parts p;
    std::vector<int> out;
    if (!split(variable, p)) {
        return out;
    }
    const auto url = folder(run, p);
    std::set<std::string> names;
    {
        const std::lock_guard lock{listingMutex};
        const auto found = listings().find(url);
        if (found != listings().end() && std::time(nullptr) - found->second.when < 300) {
            names = found->second.names;
        }
    }
    if (names.empty()) {
        const auto page = QString::fromStdString(UtilityIO::getHtml(url));
        static const QRegularExpression pattern{"href=\"(ppp_co_[^\"]*)\\.zip\""};
        auto it = pattern.globalMatch(page);
        while (it.hasNext()) {
            names.insert(it.next().captured(1).toStdString());
        }
        if (names.empty()) {
            return out;
        }
        const std::lock_guard lock{listingMutex};
        listings()[url] = {std::time(nullptr), names};
    }
    const std::string head = "ppp_co_" + p.duration + "hr_" + stem(p) + "_" + run.id() + "f";
    for (const auto& name : names) {
        if (name.compare(0, head.size(), head) == 0) {
            out.push_back(std::atoi(name.c_str() + head.size()));
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}
