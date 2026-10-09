// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtHistory.h"
#include <algorithm>
#include <sstream>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include "util/PermanentCache.h"

namespace DroughtHistory {
    QString folder() {
        const QString path = PermanentCache{"drought"}.folder() + "/history";
        QDir{}.mkpath(path);
        return path;
    }

    QString fileFor(const std::string& areaId) {
        QString safe = QString::fromStdString(areaId);
        safe.replace(QRegularExpression{"[^A-Za-z0-9_-]"}, "_");
        return folder() + "/" + safe + ".csv";
    }

    namespace {
        QString number(double v, int decimals) {
            return std::isnan(v) ? QString{} : QString::number(v, 'f', decimals);
        }
        double parse(const QString& s) {
            bool ok = false;
            const double v = s.toDouble(&ok);
            return ok ? v : std::nan("");
        }
    }

    std::string toCsv(const std::string& areaName, const std::vector<Row>& rows) {
        std::ostringstream out;
        out << "# wxqt drought history: " << areaName << "\n";
        out << "# metric units: millimeters, degrees C, percent. One row a month; the program only adds rows and fills in empty columns.\n";
        out << "# rain_rank and temperature_rank: where the month stands among the same month of every year on record (CPC), 100 = wettest / warmest, 0 = driest / coolest.\n";
        out << "# d0..d4: percent of the area in that drought category or worse on the U.S. Drought Monitor map of map_date (the last one of the month); dsci: the Severity and Coverage index (0-500).\n";
        out << "month,rain_mm,normal_mm,departure_mm,percent_of_normal,rain_rank,temperature_departure_c,temperature_rank,map_date,d0,d1,d2,d3,d4,dsci\n";
        for (const auto& r : rows) {
            out << r.month.toStdString() << "," << number(r.rain, 1).toStdString() << "," << number(r.normal, 1).toStdString() << "," << number(r.departure, 1).toStdString() << ","
                << number(r.percent, 0).toStdString() << "," << number(r.rainRank, 0).toStdString() << "," << number(r.temperature, 2).toStdString() << "," << number(r.temperatureRank, 0).toStdString() << ","
                << r.mapDate.toStdString();
            for (const double v : r.d) {
                out << "," << number(v, 1).toStdString();
            }
            out << "," << number(r.dsci, 0).toStdString() << "\n";
        }
        return out.str();
    }

    std::vector<Row> fromCsv(const std::string& text) {
        std::vector<Row> rows;
        for (const auto& line : QString::fromStdString(text).split('\n', Qt::SkipEmptyParts)) {
            if (line.startsWith('#') || line.startsWith("month")) {
                continue;
            }
            const auto c = line.trimmed().split(',');
            if (c.size() < 15 || !QRegularExpression{"^[0-9]{4}-[0-9]{2}$"}.match(c[0]).hasMatch()) {
                continue;
            }
            Row r;
            r.month = c[0];
            r.rain = parse(c[1]);
            r.normal = parse(c[2]);
            r.departure = parse(c[3]);
            r.percent = parse(c[4]);
            r.rainRank = parse(c[5]);
            r.temperature = parse(c[6]);
            r.temperatureRank = parse(c[7]);
            r.mapDate = c[8];
            for (int k = 0; k < 5; k++) {
                r.d[k] = parse(c[9 + k]);
            }
            r.dsci = parse(c[14]);
            rows.push_back(std::move(r));
        }
        std::sort(rows.begin(), rows.end(), [] (const Row& a, const Row& b) { return a.month < b.month; });
        return rows;
    }

    std::vector<Row> read(const std::string& areaId) {
        QFile f{fileFor(areaId)};
        return f.open(QIODevice::ReadOnly) ? fromCsv(f.readAll().toStdString()) : std::vector<Row>{};
    }

    bool write(const std::string& areaId, const std::string& areaName, const std::vector<Row>& rows) {
        QSaveFile f{fileFor(areaId)};
        if (!f.open(QIODevice::WriteOnly)) {
            return false;
        }
        const auto csv = toCsv(areaName, rows);
        f.write(csv.data(), static_cast<qint64>(csv.size()));
        return f.commit();
    }
}
