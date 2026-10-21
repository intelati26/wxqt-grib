// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/CpcPrecipLayer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QSlider>
#include <QStandardPaths>
#include <QVBoxLayout>
#include "mapkit/PlateCarreeImage.h"
#include "util/Utility.h"
#include "util/UtilityIO.h"

namespace {
    const int columns = 300, rows = 120;   // 230.0 to 305.0 east (130 W to 55 W), 20 N to 50 N
    const double west = -130.0, east = -55.0, south = 20.0, north = 50.0;
    const int periods[] = {1, 2, 3, 5, 7, 10, 14, 21, 30, 45, 60};

    QString folder() { return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/cpc-precip"; }

    QString urlOf(const QDate& day) {
        const auto year = day.toString("yyyy");
        return "https://ftp.cpc.ncep.noaa.gov/precip/CPC_UNI_PRCP/GAUGE_CONUS/RT/" + year + "/PRCP_CU_GAUGE_V1.0CONUS_0.25deg.lnx." + day.toString("yyyyMMdd") + ".RT";
    }

    // one day's analysis (0.1 mm, first of the two fields): from the folder, else downloaded. A day kept before the analysis was reprocessed (the 14 days after it) is asked for again.
    bool dayField(const QDate& day, std::vector<float>& out) {
        QDir{}.mkpath(folder());
        const QString path = folder() + "/" + day.toString("yyyyMMdd") + ".bin";
        QByteArray bytes;
        const QFileInfo info{path};
        const bool settled = info.exists() && info.lastModified().date() >= day.addDays(14);
        const bool fresh = info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < 6 * 3600;
        if (settled || fresh) {
            QFile file{path};
            if (file.open(QIODevice::ReadOnly)) {
                bytes = file.readAll();
            }
        }
        if (bytes.size() < columns * rows * 4) {
            bytes = UtilityIO::downloadAsByteArray(urlOf(day).toStdString());
            if (bytes.size() >= columns * rows * 4) {
                QFile file{path};
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(bytes);
                }
            } else {
                QFile file{path};   // an older copy is better than none
                if (file.open(QIODevice::ReadOnly)) {
                    bytes = file.readAll();
                }
            }
        }
        if (bytes.size() < columns * rows * 4) {
            return false;
        }
        out.resize(static_cast<size_t>(columns) * rows);
        for (size_t i = 0; i < out.size(); i++) {
            float value;
            std::memcpy(&value, bytes.constData() + i * 4, 4);   // little endian, as the files are
            out[i] = value;
        }
        return true;
    }
}

CpcPrecipLayer::CpcPrecipLayer() {
    days = Utility::readPrefInt("MM_CPCPRECIP_DAYS", 14);
    opacity = std::clamp(Utility::readPrefInt("MM_CPCPRECIP_OPACITY", 80), 20, 100);
    millimeters = Utility::readPref("MM_CPCPRECIP_UNITS", "in") == "mm";
}

// colour bands in inches, finer for the short periods
vector<CpcPrecipLayer::Band> CpcPrecipLayer::bands() const {
    const QColor colors[] = {QColor{196, 236, 178}, QColor{138, 214, 134}, QColor{70, 184, 96}, QColor{30, 144, 66}, QColor{74, 170, 232}, QColor{36, 110, 214}, QColor{24, 50, 176},
                             QColor{122, 52, 176}, QColor{192, 48, 172}, QColor{232, 52, 110}, QColor{238, 120, 44}, QColor{250, 190, 52}, QColor{255, 244, 160}};
    vector<double> edges;
    if (loadedDays <= 3) {
        edges = {0.01, 0.1, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0};
    } else if (loadedDays <= 10) {
        edges = {0.1, 0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0, 12.0};
    } else if (loadedDays <= 21) {
        edges = {0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0};
    } else if (loadedDays <= 45) {
        edges = {0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 10.0, 12.0, 15.0, 20.0, 25.0, 30.0};
    } else {
        edges = {0.5, 1.0, 2.0, 4.0, 6.0, 8.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0, 50.0};
    }
    vector<Band> out;
    for (size_t i = 0; i < edges.size(); i++) {
        out.push_back({edges[i], colors[i]});
    }
    return out;
}

void CpcPrecipLayer::colorize() {
    {   // the edges of the bands as lines with their values (in the units shown); the quarter degree blocks are smoothed once so that the lines do not step
        GfsGrid::Grid grid;
        grid.columns = columns;
        grid.rows = rows;
        grid.lon0 = west + 0.125;
        grid.lat0 = north - 0.125;
        grid.step = 0.25;
        grid.values.assign(static_cast<size_t>(columns) * static_cast<size_t>(rows), std::nanf(""));
        for (int row = 0; row < rows; row++) {
            for (int column = 0; column < columns; column++) {
                const float mm = total[static_cast<size_t>(row) * columns + column];
                if (mm >= 0.0f) {
                    grid.values[static_cast<size_t>(rows - 1 - row) * columns + column] = inches() ? mm / 25.4f : mm;
                }
            }
        }
        std::vector<std::pair<double, QString>> levels;
        for (const auto& band : bands()) {
            if (band.from <= 0.0) {
                continue;
            }
            const double value = inches() ? band.from : band.from * 25.4;
            levels.emplace_back(value, QString::number(value, 'f', value < 1.0 && inches() ? 2 : value < 10.0 && inches() ? 1 : 0));
        }
        lines = BandLines::make(GfsGrid::smoothed(grid, 1), levels);
    }
    picture = QImage{columns, rows, QImage::Format_ARGB32_Premultiplied};
    picture.fill(Qt::transparent);
    const auto scale = bands();
    for (int row = 0; row < rows; row++) {
        auto * line = reinterpret_cast<QRgb *>(picture.scanLine(rows - 1 - row));   // the file starts at the southern edge
        for (int column = 0; column < columns; column++) {
            const float mm = total[static_cast<size_t>(row) * columns + column];
            if (mm < 0.0f) {
                continue;
            }
            const double inchesOf = mm / 25.4;
            int found = -1;
            for (size_t i = 0; i < scale.size(); i++) {
                if (inchesOf >= scale[i].from) {
                    found = static_cast<int>(i);
                }
            }
            if (found >= 0) {
                line[column] = qPremultiply(qRgba(scale[static_cast<size_t>(found)].color.red(), scale[static_cast<size_t>(found)].color.green(), scale[static_cast<size_t>(found)].color.blue(), 255));
            }
        }
    }
}

void CpcPrecipLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    const int wanted = days;
    struct Result {
        std::vector<float> sum;
        int used{0};
        QDate last;
    };
    auto result = std::make_shared<Result>();
    host.background(
        [wanted, result] {
            result->sum.assign(static_cast<size_t>(columns) * rows, -1.0f);
            for (const auto& old : QDir{folder()}.entryInfoList({"*.bin"}, QDir::Files)) {   // days past 120 are not looked at again
                if (QDate::fromString(old.baseName(), "yyyyMMdd") < QDate::currentDate().addDays(-120)) {
                    QFile::remove(old.absoluteFilePath());
                }
            }
            QDate end = QDate::currentDate().addDays(-1);   // the day ends at 12Z and the file comes the next day: yesterday's, else the day before
            std::vector<float> field;
            if (!dayField(end, field)) {
                end = end.addDays(-1);
            }
            result->last = end;
            for (int back = 0; back < wanted; back++) {
                if (!dayField(end.addDays(-back), field)) {
                    continue;
                }
                result->used++;
                for (size_t i = 0; i < field.size(); i++) {
                    if (field[i] >= 0.0f && field[i] < 1.0e4f) {
                        result->sum[i] = std::max(0.0f, result->sum[i]) + field[i] / 10.0f;   // 0.1 mm to mm
                    }
                }
            }
        },
        [this, &host, result, wanted] {
            loading = false;
            if (result->used == 0) {
                error = "the CPC precipitation files could not be read";
            } else {
                error.clear();
                total = std::move(result->sum);
                usedDays = result->used;
                loadedDays = wanted;
                lastDay = result->last;
                colorize();
            }
            if (wanted != days) {
                refresh(host);   // the period was changed while it loaded
                return;
            }
            host.redraw();
        });
}

void CpcPrecipLayer::paint(QPainter& painter, MapHost& host) {
    PlateCarreeImage::paint(painter, host, picture, west, south, east, north, opacity / 100.0);   // blocks: it is a quarter degree analysis, not a smoothed field
    if (numbers) {
        BandLines::paint(painter, host, lines);
    }
}

MapHit CpcPrecipLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    if (total.empty()) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const int column = static_cast<int>(std::floor((lon - west) / 0.25));
    const int row = static_cast<int>(std::floor((lat - south) / 0.25));
    if (column < 0 || column >= columns || row < 0 || row >= rows) {
        return hit;
    }
    const float mm = total[static_cast<size_t>(row) * columns + column];
    if (mm < 0.0f) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = QString{"CPC rain, %1 days to %2: %3"}.arg(loadedDays).arg(lastDay.toString("MMM d"))
                   .arg(millimeters ? QString::number(mm, 'f', 1) + " mm" : QString::number(mm / 25.4, 'f', 2) + " in") + "\n" +
               QString::number(lat, 'f', 2) + " N, " + QString::number(-lon, 'f', 2) + " W";
    return hit;
}

vector<MapLegendRow> CpcPrecipLayer::legend() const {
    if (picture.isNull()) {
        return {};
    }
    MapLegendRow row;
    row.title = QString{"CPC rain total, %1 days (%2):"}.arg(loadedDays).arg(millimeters ? "mm" : "in");
    for (const auto& band : bands()) {
        const double value = millimeters ? band.from * 25.4 : band.from;
        row.entries.push_back({MapLegendEntry::Square, band.color, QString::number(value, 'f', value < 1.0 ? (millimeters ? 1 : 2) : (value < 10.0 && !millimeters ? 1 : 0))});
    }
    return {row};
}

string CpcPrecipLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (picture.isNull()) {
        return "CPC rain total: reading the days...";
    }
    return "CPC rain, " + std::to_string(loadedDays) + " days to " + lastDay.toString("MMM d").toStdString() + (usedDays < loadedDays ? " (" + std::to_string(usedDays) + " days had files)" : "");
}

QWidget * CpcPrecipLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    auto * numbersBox = new QCheckBox{"Lines between the colours, with their numbers", widget};
    numbersBox->setChecked(numbers);
    QObject::connect(numbersBox, &QCheckBox::toggled, [this, changed] (bool on) { numbers = on; changed(); });
    layout->addWidget(numbersBox);
    layout->addWidget(new QLabel{"Total over the last:", widget});
    auto * period = new QComboBox{widget};
    int current = 0;
    for (size_t i = 0; i < sizeof(periods) / sizeof(periods[0]); i++) {
        period->addItem(periods[i] == 1 ? QString{"1 day"} : QString::number(periods[i]) + " days");
        if (periods[i] <= days) {
            current = static_cast<int>(i);
        }
    }
    period->setCurrentIndex(current);
    layout->addWidget(period);
    layout->addWidget(new QLabel{"Units:", widget});
    auto * units = new QComboBox{widget};
    units->addItems({"Inches", "Millimeters"});
    units->setCurrentIndex(millimeters ? 1 : 0);
    layout->addWidget(units);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    layout->addWidget(new QLabel{"Each day is downloaded once (about 0.3 MB) and kept; 60 days is about 17 MB the first time.", widget});
    QObject::connect(period, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        days = periods[std::clamp(index, 0, static_cast<int>(sizeof(periods) / sizeof(periods[0])) - 1)];
        Utility::writePrefInt("MM_CPCPRECIP_DAYS", days);
        changeRequested = true;
        changed();
    });
    QObject::connect(units, &QComboBox::currentIndexChanged, [this, changed] (int index) {
        millimeters = index == 1;
        Utility::writePref("MM_CPCPRECIP_UNITS", millimeters ? "mm" : "in");
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) {
        opacity = value;
        Utility::writePrefInt("MM_CPCPRECIP_OPACITY", value);
        changed();
    });
    return widget;
}
