// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtViewer.h"
#include <algorithm>
#include <cmath>
#include <QHeaderView>
#include <QDir>
#include <QFile>
#include <QPainter>
#include <QTemporaryFile>
#include <QProcess>
#include <QRegularExpression>
#include <QPainterPath>
#include <QSplitter>
#include <QVBoxLayout>
#include "common/GlobalVariables.h"
#include <QFileDialog>
#include <QMessageBox>
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "objects/NetManager.h"
#include "objects/URL.h"
#include "ui/ActivityLabel.h"
#include "objects/PolygonWatch.h"
#include "util/To.h"
#include "models/UtilityGrib.h"
#include "settings/UIPreferences.h"
#include "util/PermanentCache.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"
#include "util/WfoSites.h"
#include "util/DownloadText.h"

namespace {
    const QColor categoryColors[5] = {QColor{"#e8d84c"}, QColor{"#fcd37f"}, QColor{"#ffaa00"}, QColor{"#e60000"}, QColor{"#730000"}};
    const char * categoryNames[5] = {"D0 abnormally dry", "D1 moderate", "D2 severe", "D3 extreme", "D4 exceptional"};
    // the box the rasters cover: the contiguous states
    constexpr double boxWest = -125.0, boxSouth = 24.0, boxEast = -66.5, boxNorth = 50.0;

    // the cells to count in: the whole country at 0.04 degree, or a state or county's own box finely enough to follow its edge
    struct Box {
        double west{boxWest}, south{boxSouth}, east{boxEast}, north{boxNorth}, step{0.04};
    };
    Box boxFor(const std::vector<UtilityDrought::Area>& areas, bool country, double coarsest, int cells) {
        Box b;
        if (country || areas.empty()) {
            b.step = coarsest;
            return b;
        }
        b.west = b.south = 1e9;
        b.east = b.north = -1e9;
        for (const auto& a : areas) {
            b.west = std::min(b.west, a.west);
            b.east = std::max(b.east, a.east);
            b.south = std::min(b.south, a.south);
            b.north = std::max(b.north, a.north);
        }
        const double extent = std::max(b.east - b.west, b.north - b.south);
        b.step = std::clamp(extent / cells, 0.004, coarsest);
        b.west -= b.step * 3;
        b.south -= b.step * 3;
        b.east += b.step * 3;
        b.north += b.step * 3;
        return b;
    }

    // the Tuesday a week before a date (yyyymmdd)
std::string mapDateBefore(const std::string& date) {
    return QDate::fromString(QString::fromStdString(date), "yyyyMMdd").addDays(-7).toString("yyyyMMdd").toStdString();
}

// the Monitor's KMZ of a date (yyyymmdd), kept for good once had (a week's map does not change)
    bool loadMonitor(const std::string& date, UtilityDrought::Monitor& out, std::string& error) {
        const PermanentCache store{"drought"};
        const auto name = "usdm_" + date + ".kmz";
        const auto got = store.fetch(name, name, 50000, [&date] { return URL::getBytes("https://droughtmonitor.unl.edu/data/kmz/usdm_" + date + ".kmz").toStdString(); });
        if (got.text.empty()) {
            error = "the map of " + date + " is not available";
            return false;
        }
        return UtilityDrought::parseKmz(got.text, out, error);
    }

    // the Census boundary file of the states or the counties, kept for good
    std::string census(bool counties) {
        const PermanentCache store{"drought"};
        const std::string name = counties ? "cb_2023_us_county_5m.zip" : "cb_2023_us_state_5m.zip";
        return store.fetch(name, name, 100000, [&name] { return URL::getBytes("https://www2.census.gov/geo/tiger/GENZ2023/shp/" + name).toStdString(); }).text;
    }
}

namespace {
    // 1st, 2nd, 3rd, 4th ... 11th, 12th, 13th, 21st
    QString ordinal(double value) {
        const int n = static_cast<int>(std::lround(value));
        const int tens = n % 100, ones = n % 10;
        return QString::number(n) + ((tens >= 11 && tens <= 13) ? "th" : ones == 1 ? "st" : ones == 2 ? "nd" : ones == 3 ? "rd" : "th");
    }

    // One of the CPC's precipitation grids (one degree, 130.5 W to 66.5 W, 20.5 N to 52.5 N: GrADS GeoTIFFs of the USDM products), kept for good once had. Millimeters; nothing is NaN.
    UtilityDrought::Field precipField(const std::string& name, const std::string& url) {
        UtilityDrought::Field field;
        const PermanentCache store{"drought"};
        const auto got = store.fetch(name, name, 2000, [&url] { return URL::getBytes(url).toStdString(); });
        if (got.text.empty() || UtilityGrib::gdalBinDir().empty()) {
            return field;
        }
        QTemporaryFile tif{QDir::tempPath() + "/wxqt_precip_XXXXXX.tif"};
        if (!tif.open()) {
            return field;
        }
        tif.write(got.text.data(), static_cast<qint64>(got.text.size()));
        tif.flush();
        const QString raw = tif.fileName() + ".raw";
        QProcess translate;
        translate.start(QString::fromStdString(UtilityGrib::gdalBinDir()) + "/gdal_translate", {"-q", "-of", "ENVI", "-ot", "Float32", tif.fileName(), raw});
        translate.waitForFinished(60000);
        QFile data{raw}, header{raw.left(raw.size() - 4) + ".hdr"};
        const auto bytes = data.open(QIODevice::ReadOnly) ? data.readAll() : QByteArray{};
        const auto hdr = header.open(QIODevice::ReadOnly) ? QString::fromUtf8(header.readAll()) : QString{};
        QFile::remove(raw);
        QFile::remove(raw + ".aux.xml");
        QFile::remove(raw.left(raw.size() - 4) + ".hdr");
        const auto number = [&hdr] (const char * key) {
            const auto m = QRegularExpression{QString{"\\b%1\\s*=\\s*([-0-9.]+)"}.arg(key)}.match(hdr);
            return m.hasMatch() ? m.captured(1).toInt() : -1;
        };
        field.columns = number("samples");
        field.rows = number("lines");
        if (field.columns != 64 || field.rows != 32 || bytes.size() != 64 * 32 * 4) {
            return {};
        }
        field.west = -130.5;
        field.north = 52.5;
        field.step = 1.0;
        field.values.resize(64 * 32);
        std::memcpy(field.values.data(), bytes.constData(), static_cast<size_t>(bytes.size()));
        if (QRegularExpression{"byte order\\s*=\\s*1"}.match(hdr).hasMatch()) {
            for (auto& v : field.values) {
                unsigned char b[4];
                std::memcpy(b, &v, 4);
                std::swap(b[0], b[3]);
                std::swap(b[1], b[2]);
                std::memcpy(&v, b, 4);
            }
        }
        for (auto& v : field.values) {
            if (v < -1e8f || std::abs(v) > 1e6f) {
                v = std::nanf("");
            }
        }
        return field;
    }
}

PrecipBars::PrecipBars(QWidget * parent) : QWidget{parent} {
    setMinimumHeight(190);
}

void PrecipBars::setMonths(const std::vector<Month>& m, const QString& t, bool in, bool temp) {
    months = m;
    title = t;
    inches = in;
    temperature = temp;
    update();
}

void PrecipBars::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().window());
    auto font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    p.setFont(font);
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRectF{4, 2, width() - 8.0, 18}, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, width() - 8));
    if (months.empty()) {
        p.drawText(rect(), Qt::AlignCenter, "Loading the months...");
        return;
    }
    const double unit = temperature ? (inches ? 1.8 : 1.0) : (inches ? 1.0 / 25.4 : 1.0);   // degrees: C or the same difference in F
    double top = temperature ? 1.0 : 1.0;
    for (const auto& m : months) {
        if (m.ok) {
            top = std::max(top, std::abs(m.anomaly) * unit);
        }
    }
    const QRectF plot{50.0, 26.0, width() - 60.0, height() - 52.0};
    const double zero = plot.center().y();
    const double scale = plot.height() / 2.0 / (top * 1.1);
    p.setPen(QColor{128, 128, 128, 140});
    p.drawLine(QPointF{plot.left(), zero}, QPointF{plot.right(), zero});
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRectF{0, plot.top() - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(top * 1.1, 'f', inches ? 1 : 0));
    p.drawText(QRectF{0, zero - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, "0");
    p.drawText(QRectF{0, plot.bottom() - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(-top * 1.1, 'f', inches ? 1 : 0));
    const double slot = plot.width() / static_cast<double>(months.size());
    for (size_t i = 0; i < months.size(); i++) {
        const auto& m = months[i];
        const double x = plot.left() + slot * static_cast<double>(i);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{x, plot.bottom() + 4, slot, 14}, Qt::AlignHCenter, m.label);
        if (!m.ok) {
            continue;
        }
        const double value = m.anomaly * unit;
        const QRectF bar{x + slot * 0.18, value >= 0 ? zero - value * scale : zero, slot * 0.64, std::abs(value) * scale};
        p.setPen(Qt::NoPen);
        const QColor up = temperature ? QColor{"#c0392b"} : QColor{"#3b8f5a"}, down = temperature ? QColor{"#2b6cb0"} : QColor{"#b5793a"};
        p.setBrush(value >= 0 ? up : down);
        if (m.partial) {
            p.setBrush(QBrush{value >= 0 ? up : down, Qt::BDiagPattern});
            p.setPen(value >= 0 ? up : down);
        }
        p.drawRect(bar);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{x, value >= 0 ? bar.top() - 14 : bar.bottom() + 1, slot, 13}, Qt::AlignHCenter, QString{"%1%2"}.arg(value > 0 ? "+" : "").arg(value, 0, 'f', inches ? 1 : 0));
    }
}

HistoryChart::HistoryChart(QWidget * parent) : QWidget{parent} {
    setMinimumHeight(300);
}

void HistoryChart::setSeries(const std::vector<Point>& p, const QString& t, const QString& u, bool b, double ref, bool warm, double lo, double hi) {
    points = p;
    title = t;
    unit = u;
    bars = b;
    reference = ref;
    warmIsRed = warm;
    low = lo;
    high = hi;
    update();
}

void HistoryChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().window());
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(QRectF{4, 2, width() - 8.0, 20}, Qt::AlignLeft | Qt::AlignVCenter, title);
    const QRectF plot{56.0, 28.0, width() - 70.0, height() - 78.0};
    if (points.size() < 2) {
        p.drawText(plot, Qt::AlignCenter, "Building the history...");
        return;
    }
    double lo = low, hi = high;
    for (const auto& pt : points) {
        if (pt.ok) {
            lo = std::min(lo, pt.value);
            hi = std::max(hi, pt.value);
        }
    }
    if (hi - lo < 1e-9) {
        hi = lo + 1.0;
    }
    const double pad = (hi - lo) * 0.06;
    lo -= bars ? 0.0 : pad;
    hi += pad;
    const auto y = [&] (double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    const auto x = [&] (size_t i) { return plot.left() + plot.width() * (static_cast<double>(i) + 0.5) / static_cast<double>(points.size()); };
    // the scale
    const double step = std::pow(10.0, std::floor(std::log10((hi - lo) / 4.0))) * ((hi - lo) / 4.0 / std::pow(10.0, std::floor(std::log10((hi - lo) / 4.0))) > 5 ? 5 : (hi - lo) / 4.0 / std::pow(10.0, std::floor(std::log10((hi - lo) / 4.0))) > 2 ? 2 : 1);
    for (double v = std::ceil(lo / step) * step; v <= hi + 1e-9; v += step) {
        p.setPen(QColor{128, 128, 128, 90});
        p.drawLine(QPointF{plot.left(), y(v)}, QPointF{plot.right(), y(v)});
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{0, y(v) - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', step < 1 ? 1 : 0));
    }
    p.drawText(QRectF{0, plot.bottom() + 20, 60, 14}, Qt::AlignLeft, unit);
    if (!std::isnan(reference)) {
        p.setPen(QPen{QColor{90, 90, 90}, 1.2, Qt::DashLine});
        p.drawLine(QPointF{plot.left(), y(reference)}, QPointF{plot.right(), y(reference)});
    }
    // the years along the bottom
    p.setPen(palette().color(QPalette::WindowText));
    for (size_t i = 0; i < points.size(); i++) {
        if (points[i].month.endsWith("-01")) {
            p.drawText(QRectF{x(i) - 24, plot.bottom() + 4, 48, 14}, Qt::AlignHCenter, points[i].month.left(4));
            p.setPen(QColor{128, 128, 128, 70});
            p.drawLine(QPointF{x(i), plot.top()}, QPointF{x(i), plot.bottom()});
            p.setPen(palette().color(QPalette::WindowText));
        }
    }
    const double slot = plot.width() / static_cast<double>(points.size());
    if (bars) {
        const double zero = y(0.0);
        for (size_t i = 0; i < points.size(); i++) {
            if (!points[i].ok) {
                continue;
            }
            const double v = points[i].value;
            const QColor up = warmIsRed ? QColor{"#c0392b"} : QColor{"#3b8f5a"}, down = warmIsRed ? QColor{"#2b6cb0"} : QColor{"#b5793a"};
            p.setPen(Qt::NoPen);
            p.setBrush(v >= 0 ? up : down);
            p.drawRect(QRectF{x(i) - slot * 0.4, std::min(zero, y(v)), slot * 0.8, std::abs(y(v) - zero)});
        }
    } else {
        QPainterPath path;
        bool started = false;
        for (size_t i = 0; i < points.size(); i++) {
            if (!points[i].ok) {
                started = false;
                continue;
            }
            const QPointF at{x(i), y(points[i].value)};
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.setPen(QPen{QColor{"#2b6cb0"}, 2.0});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}

DroughtChart::DroughtChart(QWidget * parent) : QWidget{parent} {
    setMinimumHeight(250);
}

void DroughtChart::setWeeks(const std::vector<Week>& w, const QString& t) {
    weeks = w;
    title = t;
    update();
}

void DroughtChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().window());
    p.setPen(palette().color(QPalette::WindowText));
    auto font = p.font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    p.setFont(font);
    // the key goes along the bottom, on as many lines as it needs
    int keyLines = 1;
    {
        double at = 4.0;
        for (int c = 0; c < 5; c++) {
            const double need = 18 + p.fontMetrics().horizontalAdvance(QString{"%1 %2%"}.arg(categoryNames[c]).arg(100.0, 0, 'f', 1)) + 12;
            if (at + need > width()) {
                at = 4.0;
                keyLines++;
            }
            at += need;
        }
    }
    const QRectF plot{44.0, 24.0, width() - 56.0, height() - 24.0 - 22.0 - 15.0 * keyLines - 4.0};
    p.drawText(QRectF{4, 2, width() - 8.0, 18}, Qt::AlignLeft | Qt::AlignVCenter, p.fontMetrics().elidedText(title, Qt::ElideRight, width() - 8));
    if (weeks.size() < 2) {
        p.drawText(plot, Qt::AlignCenter, "Loading the weeks...");
        return;
    }
    double top = 20.0;
    for (const auto& w : weeks) {
        top = std::max(top, w.d[0]);
    }
    top = std::min(100.0, std::ceil(top / 10.0) * 10.0);
    for (double v = 0; v <= top + 0.1; v += top > 60 ? 20 : 10) {
        const double y = plot.bottom() - v / top * plot.height();
        p.setPen(QColor{128, 128, 128, 90});
        p.drawLine(QPointF{plot.left(), y}, QPointF{plot.right(), y});
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{0, y - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 0) + "%");
    }
    const auto x = [&] (size_t i) { return plot.left() + plot.width() * static_cast<double>(i) / static_cast<double>(weeks.size() - 1); };
    for (int c = 4; c >= 0; c--) {
        QPainterPath path;
        for (size_t i = 0; i < weeks.size(); i++) {
            const QPointF at{x(i), plot.bottom() - weeks[i].d[c] / top * plot.height()};
            i == 0 ? path.moveTo(at) : path.lineTo(at);
        }
        p.setPen(QPen{categoryColors[c].darker(c == 0 ? 130 : 100), 2.0});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
    p.setPen(palette().color(QPalette::WindowText));
    for (size_t i = 0; i < weeks.size(); i += std::max<size_t>(1, weeks.size() / 5)) {
        p.drawText(QRectF{x(i) - 30, plot.bottom() + 3, 60, 14}, Qt::AlignHCenter, weeks[i].date.toString("MMM d"));
    }
    double legendX = 4.0, legendY = height() - 6.0;
    for (int c = 0; c < 5; c++) {   // the key, with the newest week's number
        const QString text = QString{"%1 %2%"}.arg(categoryNames[c]).arg(weeks.back().d[c], 0, 'f', 1);
        const double need = 18 + p.fontMetrics().horizontalAdvance(text) + 12;
        if (legendX + need > width()) {
            legendX = 4.0;
            legendY -= 15.0;
        }
        p.setPen(QPen{categoryColors[c].darker(c == 0 ? 130 : 100), 3.0});
        p.drawLine(QPointF{legendX, legendY - 4.0}, QPointF{legendX + 14, legendY - 4.0});
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QPointF{legendX + 18, legendY}, text);
        legendX += need;
    }
}

DroughtViewer::DroughtViewer(Window * parent)
    : Window{parent}
    , comboWeek{this}
    , comboCompare{this, {"This week only", "Change since 1 week earlier", "Change since 2 weeks earlier", "Change since 4 weeks earlier", "Change since 8 weeks earlier", "Change since 12 weeks earlier",
                          "Change since 26 weeks earlier", "Change since 52 weeks earlier"}}
    , textMonitor{this, ""}
    , precipImage{this}
    , outlookImage{this}
    , comboKind{this, {"Total precipitation", "Departure from normal", "Percent of normal"}}
    , comboPeriod{this, {"1 and 2 weeks, 1 and 2 months", "3, 4, 5 and 6 months", "9, 12, 18 and 24 months", "2, 3, 4 and 5 years (percent of normal)"}}
    , comboOutlook{this}
    , comboHistory{this, {"Rain: departure from normal", "Rain: percent of normal", "Rain: rank among the years", "Temperature: departure from normal", "Temperature: rank among the years", "Drought: share of the area in D1 or worse", "Drought: severity and coverage index"}}
    , textHistory{this, ""}
    , comboMetric{this, {"Area numbers: rain", "Area numbers: temperature"}}
    , textPrecip{this, ""}
    , textOutlook{this, ""}
    , states{std::make_shared<std::vector<UtilityDrought::Area>>()}
    , counties{std::make_shared<std::vector<UtilityDrought::Area>>()}
    , offices{std::make_shared<std::vector<UtilityDrought::Area>>()}
    , spc{std::make_shared<std::vector<UtilityDrought::Area>>()}
    , meso{std::make_shared<std::vector<UtilityDrought::Area>>()}
{
    setTitle("Drought");
    // above the tabs: the area and the weeks, for all of them
    buttonArea = new QPushButton{"Area: Contiguous United States  \xE2\x96\xBE", this};
    QObject::connect(buttonArea, &QPushButton::clicked, this, [this] { chooseArea(); });
    rowTop.addWidgetReal(buttonArea);
    std::vector<std::string> weeks{"Latest week"};
    for (int back = 0; back < 104; back++) {
        weeks.push_back("Week of " + QDate::fromString(QString::fromStdString(mapDate(back)), "yyyyMMdd").toString("MMMM d, yyyy").toStdString());
    }
    comboWeek.setList(weeks);
    rowTop.addWidget(comboWeek);
    rowTop.addWidget(comboCompare);
    rowTop.addStretch();
    box.addLayout(rowTop);

    tabs = new QTabWidget{this};
    // tab 1: the map beside the numbers
    auto * monitorPage = new QWidget{tabs};
    auto * monitorColumn = new QVBoxLayout{monitorPage};
    monitorColumn->setContentsMargins(4, 4, 4, 4);
    monitorColumn->addWidget(textMonitor.getView());
    auto * split = new QSplitter{Qt::Horizontal, monitorPage};
    map = new DroughtMap{split};
    auto * side = new QWidget{split};
    auto * sideColumn = new QVBoxLayout{side};
    sideColumn->setContentsMargins(0, 0, 0, 0);
    table = new QTableWidget{0, 4, side};
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMaximumHeight(240);
    sideColumn->addWidget(table);
    chart = new DroughtChart{side};
    sideColumn->addWidget(chart);
    split->addWidget(map);
    split->addWidget(side);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    split->setSizes({860, 560});
    monitorColumn->addWidget(split, 1);
    tabs->addTab(monitorPage, "Drought Monitor");
    // tabs 2 and 3: the Climate Prediction Center's pictures
    const auto page = [this] (const QString& name, HBox& row, ZoomImage& image, Text& status) {
        auto * widget = new QWidget{tabs};
        auto * column = new QVBoxLayout{widget};
        column->setContentsMargins(4, 4, 4, 4);
        row.addStretch();
        column->addLayout(row.getView());
        column->addWidget(status.getView());
        image.setMinimumHeight(380);
        column->addWidget(&image, 1);
        tabs->addTab(widget, name);
    };
    rowPrecip.addWidget(comboKind);
    rowPrecip.addWidget(comboPeriod);
    rowPrecip.addWidget(comboMetric);
    rowOutlook.addWidget(comboOutlook);
    {   // the precipitation tab: the national pictures above, the area's rain month by month below
        auto * widget = new QWidget{tabs};
        auto * column = new QVBoxLayout{widget};
        column->setContentsMargins(4, 4, 4, 4);
        rowPrecip.addStretch();
        column->addLayout(rowPrecip.getView());
        column->addWidget(textPrecip.getView());
        auto * vertical = new QSplitter{Qt::Vertical, widget};
        precipImage.setMinimumHeight(260);
        vertical->addWidget(&precipImage);
        auto * panel = new QWidget{vertical};
        auto * panelRow = new QHBoxLayout{panel};
        panelRow->setContentsMargins(0, 0, 0, 0);
        precipTable = new QTableWidget{0, 5, panel};
        precipTable->setHorizontalHeaderLabels({"Month", "Rain", "Normal", "Departure", "Of normal"});
        precipTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        precipTable->setSelectionMode(QAbstractItemView::NoSelection);
        precipTable->verticalHeader()->hide();
        precipTable->horizontalHeader()->setStretchLastSection(true);
        precipBars = new PrecipBars{panel};
        panelRow->addWidget(precipTable, 2);
        panelRow->addWidget(precipBars, 3);
        vertical->addWidget(panel);
        vertical->setStretchFactor(0, 3);
        vertical->setStretchFactor(1, 2);
        column->addWidget(vertical, 1);
        tabs->addTab(widget, "Precipitation");
    }
    page("Outlooks and soil moisture", rowOutlook, outlookImage, textOutlook);
    {   // the history tab: one column of the area's history file as a chart
        auto * widget = new QWidget{tabs};
        auto * column = new QVBoxLayout{widget};
        column->setContentsMargins(4, 4, 4, 4);
        auto * row = new QHBoxLayout;
        row->addWidget(comboHistory.getView());
        auto * exportButton = new QPushButton{"Export the table (CSV)...", widget};
        QObject::connect(exportButton, &QPushButton::clicked, this, [this] { exportHistory(); });
        row->addWidget(exportButton);
        row->addStretch();
        column->addLayout(row);
        column->addWidget(textHistory.getView());
        historyChart = new HistoryChart{widget};
        column->addWidget(historyChart, 1);
        tabs->addTab(widget, "History");
    }
    box.addWidgetReal(tabs, 1, Qt::Alignment{});
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);

    const std::string cpc = "https://www.cpc.ncep.noaa.gov/products/";
    outlooks = {{"Drought outlook, this month (CPC)", cpc + "expert_assessment/mdohomeweb.png"},
                {"Drought outlook, the season (CPC)", cpc + "expert_assessment/sdohomeweb.png"},
                {"Soil moisture percentile, ensemble (CPC)", cpc + "Drought/Figures/smp/ens.png"},
                {"Soil moisture percentile, change over 7 days", cpc + "Drought/Figures/smp/ens_7.png"},
                {"Soil moisture percentile, change over 30 days", cpc + "Drought/Figures/smp/ens_30.png"},
                {"Soil moisture rank, daily (CPC leaky bucket)", cpc + "Soilmst_Monitoring/Figures/daily/curr.w.rank.daily.gif"},
                {"Standardized precipitation index, 3 months", cpc + "Drought/Figures/index/spi3.web.gif"},
                {"Standardized precipitation index, 6 months", cpc + "Drought/Figures/index/spi6.web.gif"},
                {"Standardized precipitation index, 12 months", cpc + "Drought/Figures/index/spi12.web.gif"},
                {"Standardized precipitation index, 24 months", cpc + "Drought/Figures/index/spi24.web.gif"}};
    std::vector<std::string> names;
    for (const auto& o : outlooks) {
        names.push_back(o.label);
    }
    comboOutlook.setList(names);
    comboWeek.connect([this] { refreshMonitor(); });
    comboCompare.connect([this] { refreshMonitor(); });
    comboKind.connect([this] { loadPrecip(); });
    comboPeriod.connect([this] { loadPrecip(); });
    comboOutlook.connect([this] { loadOutlook(); });
    comboMetric.connect([this] { loadPrecipArea(); });
    comboHistory.connect([this] { showHistory(); });
    loadAreas();
    loadPrecip();
    loadOutlook();
}

// the date (yyyymmdd) of the Tuesday `weeksBack` weeks before the newest map that is out
std::string DroughtViewer::mapDate(int weeksBack) const {
    auto day = QDate::currentDate();
    while (day.dayOfWeek() != 2) {   // back to a Tuesday
        day = day.addDays(-1);
    }
    if (QDate::currentDate().daysTo(day) > -2) {   // the map for a Tuesday comes out on the Thursday after
        day = day.addDays(-7);
    }
    return day.addDays(-7 * weeksBack).toString("yyyyMMdd").toStdString();
}

// the shapes of the states (the land, and the areas to choose) and the counties, from the Census Bureau: kept for good after the first time
void DroughtViewer::loadAreas() {
    struct Loaded {
        std::vector<UtilityDrought::Area> states, counties, offices;
    };
    auto loaded = std::make_shared<Loaded>();
    new FutureVoid{this, [loaded] {
                       std::string error;
                       UtilityDrought::parseAreas(census(false), false, false, loaded->states, error);
                       UtilityDrought::parseAreas(census(true), true, false, loaded->counties, error);
                       // the weather service's county warning areas: the boundary file is 19 MB, so the thinned shapes are kept for good after the first time
                       const PermanentCache store{"drought"};
                       const std::string kept = "nws_forecast_offices_v1.bin";
                       if (!store.has(kept) || !UtilityDrought::deserialize(store.read(kept), loaded->offices)) {
                           loaded->offices.clear();
                           const auto page = URL::getBytes("https://www.weather.gov/gis/CWABounds").toStdString();
                           const auto file = UtilityDrought::newestWarningAreaFile(page);
                           if (!file.empty()) {
                               const auto zip = URL::getBytes("https://www.weather.gov/source/gis/Shapefiles/WSOM/" + file).toStdString();
                               UtilityDrought::parseWarningAreas(zip, [] (const std::string& code) {
                                   if (!WfoSites::sites) {
                                       return std::string{};
                                   }
                                   const auto found = WfoSites::sites->byCode.find(code);
                                   if (found == WfoSites::sites->byCode.end()) {
                                       return std::string{};
                                   }
                                   // "MO, Kansas City" -> "Kansas City MO", with the city's initials for a name of several words ("KC")
                                   const auto full = found->second->fullName;
                                   const auto comma = full.find(", ");
                                   const auto city = comma == std::string::npos ? full : full.substr(comma + 2), state = comma == std::string::npos ? std::string{} : full.substr(0, comma);
                                   std::string initials;
                                   bool word = true;
                                   for (const char ch : city) {
                                       if (ch == ' ' || ch == '/') {
                                           word = true;
                                       } else if (word && std::isalpha(static_cast<unsigned char>(ch))) {
                                           initials += static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
                                           word = false;
                                       }
                                   }
                                   return city + (state.empty() ? "" : " " + state) + (initials.size() >= 2 ? "|" + initials : std::string{});
                               }, loaded->offices, error);
                               if (!loaded->offices.empty()) {
                                   store.write(kept, UtilityDrought::serialize(loaded->offices));
                               }
                           }
                       }
                   },
                   [this, loaded] {
                       if (closed) {
                           return;
                       }
                       *states = std::move(loaded->states);
                       *counties = std::move(loaded->counties);
                       *offices = std::move(loaded->offices);
                       *meso = UtilityDrought::spcMesoanalysisSectors();
                       map->setLand(states);
                       map->setCounties(counties);
                       loadSpc();
                       if (const auto env = qgetenv("WXQT_METRIC"); !env.isEmpty()) {
                           comboMetric.setIndex(env.toInt());
                       }
                       if (const auto env = qgetenv("WXQT_COMPARE"); !env.isEmpty()) {   // dev: WXQT_COMPARE=<n> opens on that comparison, WXQT_AREA=<id> on that area ("08" Colorado)
                           comboCompare.setIndex(env.toInt());
                       }
                       if (const auto env = qgetenv("WXQT_AREA"); !env.isEmpty()) {
                           selectArea(env.toStdString());
                           return;
                       }
                       refreshMonitor();
                       loadPrecipArea();
                       updateHistory();
                       if (qEnvironmentVariableIsSet("WXQT_PICKAREA")) {   // dev: open the area picker
                           chooseArea();
                       }
                   }};
}

std::vector<UtilityDrought::Area> DroughtViewer::selectedAreas() const {
    std::vector<UtilityDrought::Area> out;
    if (areaId == "US") {
        return *states;   // every contiguous state
    }
    for (const auto& list : {states.get(), counties.get(), offices.get(), spc.get(), meso.get()}) {
        for (const auto& a : *list) {
            if (a.id == areaId) {
                out.push_back(a);
                return out;
            }
        }
    }
    return *states;
}

void DroughtViewer::chooseArea() {
    if (picker) {
        picker->raise();
        picker->activateWindow();
        return;
    }
    loadSpc();   // fresh for the next time the picker opens (the discussions and watches change through the day)
    std::vector<ProductPicker::Entry> entries{{"US", "Contiguous United States", "Nation"}};
    for (const auto& s : *states) {
        entries.push_back({s.id, s.name, "States"});
    }
    for (const auto& o : *offices) {
        entries.push_back({o.id, o.name, "NWS forecast offices"});
    }
    for (const auto& s : *spc) {
        entries.push_back({s.id, s.name, s.group});
    }
    for (const auto& s : *meso) {
        entries.push_back({s.id, s.name, s.group});
    }
    for (const auto& c : *counties) {
        entries.push_back({c.id, c.name, c.group + " counties"});
    }
    picker = new ProductPicker{this, "Drought", entries, areaId, {}, {}, {}};
    picker->setWording("areas", "Show this area");
    picker->resize(380, 600);
    picker->onPick = [this] (const std::string& id) { selectArea(id); };
    picker->show();
}

void DroughtViewer::selectArea(const std::string& id) {
    areaId = id;
    const auto areas = selectedAreas();
    std::string label = "Contiguous United States";
    if (id != "US" && !areas.empty()) {
        label = areas.front().name;
    }
    buttonArea->setText(QString::fromStdString("Area: " + label) + "  \xE2\x96\xBE");
    map->setArea(id == "US" ? std::vector<UtilityDrought::Area>{} : areas, true);
    refreshMonitor();
    loadPrecipArea();
    updateHistory();
}

// What the background work brings back for the Monitor tab
struct DroughtViewer::Result {
    std::shared_ptr<UtilityDrought::Monitor> monitor;                // the later week
    std::shared_ptr<DroughtMap::ChangeLayer> change;                 // null for one week
    UtilityDrought::Share later, earlier;
    bool compared{false};
    QDate laterDate, earlierDate;
    std::string error;
};

void DroughtViewer::refreshMonitor() {
    if (states->empty()) {
        return;   // the boundaries are not in yet: loadAreas() comes back here
    }
    const int mine = ++monitorGeneration;
    static const int weeksBack[] = {0, 1, 2, 4, 8, 12, 26, 52};
    const int week = std::max(0, comboWeek.getIndex()) == 0 ? 0 : comboWeek.getIndex() - 1;   // the latest, or one of the weeks listed
    const int compare = weeksBack[std::clamp(comboCompare.getIndex(), 0, 7)];
    textMonitor.setText(std::string{"Loading the Drought Monitor..."});
    const auto areas = selectedAreas();
    const auto date = mapDate(week), earlierDate = mapDate(week + compare);
    const bool latest = comboWeek.getIndex() <= 0;
    auto result = std::make_shared<Result>();
    const auto box = boxFor(areas, areaId == "US", 0.04, 260);
    new FutureVoid{this, [result, areas, date, earlierDate, compare, latest, box] {
                       std::string error;
                       auto later = std::make_shared<UtilityDrought::Monitor>();
                       if (!loadMonitor(date, *later, error)) {
                           // the newest map is not out yet (the Thursday release): the week before
                           if (!latest || !loadMonitor(mapDateBefore(date), *later, error)) {
                               result->error = error;
                               return;
                           }
                       }
                       result->monitor = later;
                       result->laterDate = later->valid;
                       const auto raster = UtilityDrought::rasterize(*later, box.west, box.south, box.east, box.north, box.step);
                       const auto mask = UtilityDrought::mask(raster, areas);
                       result->later = UtilityDrought::share(raster, mask);
                       if (compare > 0) {
                           UtilityDrought::Monitor before;
                           const auto when = latest ? QDate{later->valid}.addDays(-7 * compare).toString("yyyyMMdd").toStdString() : earlierDate;
                           if (!loadMonitor(when, before, error)) {
                               result->error = error;
                               return;
                           }
                           const auto rasterBefore = UtilityDrought::rasterize(before, box.west, box.south, box.east, box.north, box.step);
                           result->earlier = UtilityDrought::share(rasterBefore, mask);
                           result->earlierDate = before.valid;
                           result->compared = true;
                           auto layer = std::make_shared<DroughtMap::ChangeLayer>();
                           layer->moved = UtilityDrought::change(rasterBefore, raster);
                           layer->columns = raster.columns;
                           layer->rows = raster.rows;
                           layer->west = raster.west;
                           layer->north = raster.north;
                           layer->step = raster.step;
                           layer->image = QImage{raster.columns, raster.rows, QImage::Format_ARGB32};
                           layer->image.fill(Qt::transparent);
                           static const QRgb colors[9] = {qRgb(8, 48, 107), qRgb(8, 48, 107), qRgb(49, 130, 189), qRgb(158, 202, 225), 0, qRgb(253, 174, 107), qRgb(230, 85, 13), qRgb(166, 54, 3), qRgb(166, 54, 3)};
                           for (int y = 0; y < raster.rows; y++) {
                               auto * line = reinterpret_cast<QRgb *>(layer->image.scanLine(y));
                               for (int x = 0; x < raster.columns; x++) {
                                   const size_t i = static_cast<size_t>(y) * static_cast<size_t>(raster.columns) + static_cast<size_t>(x);
                                   const int moved = layer->moved[i];
                                   if (moved != 0 && mask[i]) {
                                       line[x] = colors[std::clamp(moved, -4, 4) + 4] | 0xd8000000u;
                                   } else {
                                       layer->moved[i] = mask[i] ? static_cast<int8_t>(moved) : static_cast<int8_t>(0);
                                   }
                               }
                           }
                           result->change = layer;
                       }
                   },
                   [this, result, mine] {
                       if (closed || mine != monitorGeneration) {
                           return;
                       }
                       if (!result->monitor) {
                           textMonitor.setText("The Drought Monitor could not be had: " + result->error);
                           return;
                       }
                       map->setMonitor(result->monitor);
                       map->setChange(result->change);
                       const QString credit = "U.S. Drought Monitor (National Drought Mitigation Center at the University of Nebraska-Lincoln, USDA and NOAA), drawn from its KMZ.";
                       QString what = "Valid " + result->laterDate.toString("MMMM d, yyyy");
                       if (result->compared) {
                           what += ", changed since " + result->earlierDate.toString("MMMM d, yyyy");
                       }
                       textMonitor.setText((what + ".  " + credit).toStdString());
                       // the table: the share of the area in each category, then and now
                       const int rows = 7;
                       table->setRowCount(rows);
                       table->setColumnCount(result->compared ? 4 : 2);
                       table->setHorizontalHeaderLabels(result->compared ? QStringList{"Share of the area", result->earlierDate.toString("MMM d"), result->laterDate.toString("MMM d"), "Change"}
                                                                          : QStringList{"Share of the area", result->laterDate.toString("MMM d")});
                       const QString labels[rows] = {"No drought", "D0 or worse (abnormally dry)", "D1 or worse (moderate)", "D2 or worse (severe)", "D3 or worse (extreme)", "D4 (exceptional)",
                                                     "Severity and coverage index (0-500)"};
                       const auto value = [] (const UtilityDrought::Share& s, int row) { return row == 0 ? s.none : row == 6 ? s.dsci() : s.atLeast[row - 1]; };
                       for (int r = 0; r < rows; r++) {
                           const auto cell = [r, &value] (const UtilityDrought::Share& s) { return QString::number(value(s, r), 'f', r == 6 ? 0 : 1) + (r == 6 ? "" : "%"); };
                           table->setItem(r, 0, new QTableWidgetItem{labels[r]});
                           if (result->compared) {
                               table->setItem(r, 1, new QTableWidgetItem{cell(result->earlier)});
                               table->setItem(r, 2, new QTableWidgetItem{cell(result->later)});
                               const double delta = value(result->later, r) - value(result->earlier, r);
                               const bool worse = r == 0 ? delta < 0 : delta > 0;   // less "no drought" is worse
                               auto * item = new QTableWidgetItem{QString{delta > 0 ? "+" : ""} + QString::number(delta, 'f', r == 6 ? 0 : 1) + (r == 6 ? "" : " pts") + (std::abs(delta) < 0.05 ? "" : delta > 0 ? "  \xE2\x96\xB2" : "  \xE2\x96\xBC")};
                               item->setForeground(std::abs(delta) < 0.05 ? QBrush{} : worse ? QBrush{QColor{"#c0392b"}} : QBrush{QColor{"#2b6cb0"}});
                               table->setItem(r, 3, item);
                           } else {
                               table->setItem(r, 1, new QTableWidgetItem{cell(result->later)});
                           }
                       }
                       table->resizeColumnsToContents();
                       loadSeries();
                   }};
}

// The share of the area in each category over the last months: the country from the Monitor's statistics (one request), another area from the shapes of every second week
// (the maps are kept for good once had, so the chart fills in faster each time).
void DroughtViewer::loadSeries() {
    const int mine = ++seriesGeneration;
    const QString label = areaId == "US" ? "The contiguous U.S." : QString::fromStdString(selectedAreas().empty() ? areaId : selectedAreas().front().name);
    const QString title = label + " in drought, by week (% of area)";
    {
        const std::lock_guard lock{seriesMutex};
        series.clear();
    }
    chart->setWeeks({}, title);
    if (areaId == "US") {
        const auto end = QDate::currentDate(), start = end.addDays(-370);
        const std::string url = "https://usdmdataservices.unl.edu/api/USStatistics/GetDroughtSeverityStatisticsByAreaPercent?aoi=conus&startdate=" + start.toString("M/d/yyyy").toStdString() +
            "&enddate=" + end.toString("M/d/yyyy").toStdString() + "&statisticsType=1";
        new FutureText{this, url, [this, mine, title] (std::string text) {
            if (closed || mine != seriesGeneration) {
                return;
            }
            std::vector<DroughtChart::Week> weeks;
            for (const auto& line : QString::fromStdString(text).split('\n', Qt::SkipEmptyParts)) {
                const auto cells = line.trimmed().split(',');
                if (cells.size() < 8) {
                    continue;
                }
                DroughtChart::Week w;
                w.date = QDate::fromString(cells[0], "yyyyMMdd");
                if (!w.date.isValid()) {
                    continue;
                }
                for (int c = 0; c < 5; c++) {
                    w.d[c] = cells[3 + c].toDouble();
                }
                weeks.push_back(w);
            }
            std::sort(weeks.begin(), weeks.end(), [] (const auto& a, const auto& b) { return a.date < b.date; });
            chart->setWeeks(weeks, title);
        }};
        return;
    }
    const auto areas = selectedAreas();
    const auto box = boxFor(areas, false, 0.08, 100);
    for (int back = 26; back >= 0; back -= 2) {   // every second week of the last half year, the oldest first
        const auto date = mapDate(back);
        new FutureVoid{this, [this, areas, date, mine, box] {
                           if (mine != seriesGeneration) {
                               return;
                           }
                           const NetManager::Scope background{NetManager::Priority::Background};
                           UtilityDrought::Monitor monitor;
                           std::string error;
                           if (!loadMonitor(date, monitor, error)) {
                               return;
                           }
                           const auto raster = UtilityDrought::rasterize(monitor, box.west, box.south, box.east, box.north, box.step);
                           const auto share = UtilityDrought::share(raster, UtilityDrought::mask(raster, areas));
                           DroughtChart::Week w;
                           w.date = monitor.valid;
                           for (int c = 0; c < 5; c++) {
                               w.d[c] = share.atLeast[c];
                           }
                           const std::lock_guard lock{seriesMutex};
                           if (mine == seriesGeneration) {
                               series.push_back(w);
                           }
                       },
                       [this, mine, title] {
                           if (closed || mine != seriesGeneration) {
                               return;
                           }
                           std::vector<DroughtChart::Week> weeks;
                           {
                               const std::lock_guard lock{seriesMutex};
                               weeks = series;
                           }
                           std::sort(weeks.begin(), weeks.end(), [] (const auto& a, const auto& b) { return a.date < b.date; });
                           chart->setWeeks(weeks, title);
                       }};
    }
}

// the SPC's mesoscale discussions and watches in force now, as areas (the same pages and polygons the severe dashboard reads)
void DroughtViewer::loadSpc() {
    auto found = std::make_shared<std::vector<UtilityDrought::Area>>();
    new FutureVoid{this, [found] {
                       const NetManager::Scope ahead{NetManager::Priority::Ahead};
                       {
                           const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/md/");
                           std::string latLon, numbers;
                           for (const auto& number : UtilityString::parseColumn(html, "<strong><a href=./products/md/md.....html.>Mesoscale Discussion #(.*?)</a></strong>")) {
                               const auto padded = To::stringPadLeftZeros(To::Int(number), 4);
                               numbers += padded + ":";
                               latLon += PolygonWatch::storeWatchMcdLatLon(DownloadText::byProduct("SPCMCD" + padded));
                           }
                           for (auto& a : UtilityDrought::parseSpcPolygons(latLon, numbers, "MCD", "SPC Mesoscale Discussion", "SPC mesoscale discussions (in force now)")) {
                               found->push_back(std::move(a));
                           }
                       }
                       {
                           const auto html = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/");
                           std::string latLon, numbers;
                           for (const auto& number : UtilityString::parseColumn(html, "[om] Watch #([0-9]*?)</a>")) {
                               const auto padded = To::stringPadLeftZeros(number, 4);
                               numbers += padded + ":";
                               const auto text = UtilityIO::getHtml(GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/wou" + padded + ".html");
                               latLon += PolygonWatch::storeWatchMcdLatLon(UtilityString::parseMultiLineLastMatch(text, GlobalVariables::pre2Pattern));
                           }
                           for (auto& a : UtilityDrought::parseSpcPolygons(latLon, numbers, "WW", "SPC Watch", "SPC watches (in force now)")) {
                               found->push_back(std::move(a));
                           }
                       }
                   },
                   [this, found] {
                       if (closed) {
                           return;
                       }
                       *spc = std::move(*found);   // the picker opened next lists them (one open now keeps what it had)
                       if (qEnvironmentVariableIsSet("WXQT_NETLOG")) {
                           for (const auto& a : *spc) {
                               fprintf(stderr, "spc area %s %s (%.2f..%.2f, %.2f..%.2f)\n", a.id.c_str(), a.name.c_str(), a.west, a.east, a.south, a.north);
                           }
                       }
                       if (areaId.compare(0, 2, "MC") == 0 || areaId.compare(0, 2, "WW") == 0) {
                           const bool still = std::any_of(spc->begin(), spc->end(), [this] (const auto& a) { return a.id == areaId; });
                           if (!still) {
                               buttonArea->setText(buttonArea->text() + "  (no longer in force)");
                           }
                       }
                   }};
}

// The area's rain by month over the last year, from the CPC's one degree analyses of the monthly totals and of how far each was from normal (the month so far from the day before)
void DroughtViewer::loadPrecipArea() {
    if (states->empty()) {
        return;
    }
    const int mine = ++precipAreaGeneration;
    const auto areas = selectedAreas();
    const bool country = areaId == "US";
    const auto box = boxFor(areas, country, 0.04, 260);
    const QString label = country ? "the contiguous U.S." : QString::fromStdString(areas.empty() ? areaId : areas.front().name);
    const bool temperature = comboMetric.getIndex() == 1;
    auto rows = std::make_shared<std::vector<PrecipBars::Month>>();
    new FutureVoid{this, [rows, areas, box, temperature] {
                       const NetManager::Scope background{NetManager::Priority::Ahead};
                       UtilityDrought::Raster like;
                       like.west = box.west;
                       like.north = box.north;
                       like.step = box.step;
                       like.columns = std::max(1, static_cast<int>(std::ceil((box.east - box.west) / box.step)));
                       like.rows = std::max(1, static_cast<int>(std::ceil((box.north - box.south) / box.step)));
                       const auto mask = UtilityDrought::mask(like, areas);
                       const std::string base = "https://ftp.cpc.ncep.noaa.gov/GIS/USDM_Products/precip/";
                       const auto today = QDate::currentDate();
                       const auto row = [&] (const QString& label, const std::string& totalName, const std::string& totalUrl, const std::string& anomName, const std::string& anomUrl, bool partial, const std::string& rankName = {}, const std::string& rankUrl = {}) {
                           PrecipBars::Month m;
                           m.label = label;
                           m.partial = partial;
                           if (!rankName.empty()) {   // the percentile of the whole month (there is none for a month so far)
                               const double r = UtilityDrought::meanOver(precipField(rankName, rankUrl), like, mask);
                               m.rank = std::isnan(r) ? -1.0 : std::clamp(r, 0.0, 100.0);
                           }
                           const auto anomaly = precipField(anomName, anomUrl);
                           const double a = UtilityDrought::meanOver(anomaly, like, mask);
                           if (temperature) {   // the departure from normal, in degrees C; there is no total
                               m.ok = !std::isnan(a);
                               m.anomaly = a;
                               rows->push_back(m);
                               return;
                           }
                           const auto total = precipField(totalName, totalUrl);
                           const double t = UtilityDrought::meanOver(total, like, mask);
                           m.ok = !std::isnan(t) && !std::isnan(a) && !(partial && t - a < -1.0);   // the month so far: a departure that is more than the rain itself cannot be right (the daily analysis is not the monthly one): left out
                           m.total = std::max(0.0, t);
                           m.anomaly = a;
                           rows->push_back(m);
                       };
                       for (int back = 12; back >= 1; back--) {   // the last twelve whole months
                           const auto month = QDate{today.year(), today.month(), 1}.addMonths(-back);
                           const auto key = month.toString("yyyyMM").toStdString();
                           if (temperature) {
                               row(month.toString("MMM yy"), "", "", "t.anom." + key + ".tif", "https://ftp.cpc.ncep.noaa.gov/GIS/USDM_Products/temp/anom/monthly/t.anom." + key + ".tif", false, "t.rank." + key + ".tif",
                                   "https://ftp.cpc.ncep.noaa.gov/GIS/USDM_Products/temp/percentile/monthly/t.rank." + key + ".tif");
                           } else {
                               row(month.toString("MMM yy"), "p.full." + key + ".tif", base + "total/monthly/p.full." + key + ".tif", "p.anom." + key + ".tif", base + "anom/monthly/p.anom." + key + ".tif", false, "p.rank." + key + ".tif",
                                   base + "percentile/monthly/p.rank." + key + ".tif");
                           }
                       }
                       for (int back = 1; back <= 3 && today.day() > 1; back++) {   // this month so far: the newest day that has a file
                           const auto day = today.addDays(-back).toString("yyyyMMdd").toStdString();
                           const auto before = rows->size();
                           if (temperature) {
                               row(today.toString("MMM d") + " so far", "", "", "t.anom.1stday_month_" + day + ".tif", "https://ftp.cpc.ncep.noaa.gov/GIS/USDM_Products/temp/anom/daily/t.anom.1stday_month_" + day + ".tif", true);
                           } else {
                               row(today.toString("MMM d") + " so far", "p.full.1stday_month_" + day + ".tif", base + "total/daily/p.full.1stday_month_" + day + ".tif", "p.anom.1stday_month_" + day + ".tif",
                                   base + "anom/daily/p.anom.1stday_month_" + day + ".tif", true);
                           }
                           if (rows->back().ok) {
                               rows->back().label = "To " + QDate::fromString(QString::fromStdString(day), "yyyyMMdd").toString("MMM d");
                               break;
                           }
                           rows->resize(before);
                       }
                   },
                   [this, rows, mine, label, temperature] {
                       if (closed || mine != precipAreaGeneration) {
                           return;
                       }
                       const bool inches = UIPreferences::unitsF;
                       const double unit = temperature ? (inches ? 1.8 : 1.0) : (inches ? 1.0 / 25.4 : 1.0);
                       precipBars->setMonths(*rows, (temperature ? "Temperature over " + label + " compared with normal (departure, degrees " + (inches ? "F" : "C") + ")"
                                                                 : "Rain over " + label + " compared with normal (departure, " + (inches ? "inches" : "mm") + ")"), inches, temperature);
                       precipTable->setColumnCount(temperature ? 3 : 6);
                       precipTable->setHorizontalHeaderLabels(temperature ? QStringList{"Month", "Warmer (+) or cooler (-) than normal", "Rank"} : QStringList{"Month", "Rain", "Normal", "Departure", "Of normal", "Rank"});
                       precipTable->horizontalHeaderItem(temperature ? 2 : 5)->setToolTip(temperature ? "Where the month stands among the same month of every year on record: 100 is the warmest, 0 the coolest" :
                                                                                                         "Where the month stands among the same month of every year on record: 100 is the wettest, 0 the driest");
                       precipTable->setRowCount(static_cast<int>(rows->size()));
                       for (int r = 0; r < static_cast<int>(rows->size()); r++) {
                           const auto& m = (*rows)[static_cast<size_t>(r)];
                           const double normal = m.total - m.anomaly;
                           if (temperature) {
                               precipTable->setItem(r, 0, new QTableWidgetItem{m.label});
                               auto * item = new QTableWidgetItem{m.ok ? QString{"%1%2 degrees %3"}.arg(m.anomaly > 0 ? "+" : "").arg(m.anomaly * unit, 0, 'f', 1).arg(inches ? "F" : "C") : "-"};
                               item->setForeground(m.anomaly >= 0 ? QBrush{QColor{"#c0392b"}} : QBrush{QColor{"#2b6cb0"}});
                               precipTable->setItem(r, 1, item);
                               precipTable->setItem(r, 2, new QTableWidgetItem{m.rank >= 0 ? ordinal(m.rank) : "-"});
                               continue;
                           }
                           const auto number = [unit, inches] (double mm) { return QString::number(mm * unit, 'f', inches ? 2 : 0); };
                           precipTable->setItem(r, 0, new QTableWidgetItem{m.label});
                           precipTable->setItem(r, 1, new QTableWidgetItem{m.ok ? number(m.total) : "-"});
                           precipTable->setItem(r, 2, new QTableWidgetItem{m.ok ? number(normal) : "-"});
                           auto * departure = new QTableWidgetItem{m.ok ? QString{"%1%2"}.arg(m.anomaly > 0 ? "+" : "").arg(number(m.anomaly)) : "-"};
                           departure->setForeground(m.anomaly >= 0 ? QBrush{QColor{"#2f7d4b"}} : QBrush{QColor{"#a8651f"}});
                           precipTable->setItem(r, 3, departure);
                           precipTable->setItem(r, 4, new QTableWidgetItem{m.ok && normal > 1.0 ? QString::number(m.total / normal * 100.0, 'f', 0) + "%" : "-"});
                           precipTable->setItem(r, 5, new QTableWidgetItem{m.rank >= 0 ? ordinal(m.rank) : "-"});
                       }
                       precipTable->resizeColumnsToContents();
                   }};
}

// The area's history file (drought/history/<area>.csv in the data folder): what it lacks is added in the background, a year of months at a time so that a stop loses little, and the
// chart is redrawn as it grows. The weather columns go back to January 2017 (where the CPC's rank files begin); the drought shares are taken from the Monitor map of the last Tuesday of each
// of the last 36 months.
void DroughtViewer::updateHistory() {
    if (states->empty()) {
        return;
    }
    const int mine = ++historyGeneration;
    const auto areas = selectedAreas();
    const bool country = areaId == "US";
    const std::string id = areaId;
    const std::string name = country ? "Contiguous United States" : (areas.empty() ? id : areas.front().name);
    const auto box = boxFor(areas, country, 0.04, 260);
    history = DroughtHistory::read(id);
    showHistory();
    new FutureVoid{this, [this, id, name, areas, box, mine] {
                       const NetManager::Scope background{NetManager::Priority::Background};
                       UtilityDrought::Raster like;
                       like.west = box.west;
                       like.north = box.north;
                       like.step = box.step;
                       like.columns = std::max(1, static_cast<int>(std::ceil((box.east - box.west) / box.step)));
                       like.rows = std::max(1, static_cast<int>(std::ceil((box.north - box.south) / box.step)));
                       const auto mask = UtilityDrought::mask(like, areas);
                       const std::string root = "https://ftp.cpc.ncep.noaa.gov/GIS/USDM_Products/";
                       auto rows = DroughtHistory::read(id);
                       std::map<QString, size_t> at;
                       for (size_t i = 0; i < rows.size(); i++) {
                           at[rows[i].month] = i;
                       }
                       const auto today = QDate::currentDate();
                       const auto lastMonth = QDate{today.year(), today.month(), 1}.addMonths(-1);
                       int sinceSave = 0;
                       const auto save = [&] {
                           std::sort(rows.begin(), rows.end(), [] (const auto& a, const auto& b) { return a.month < b.month; });
                           DroughtHistory::write(id, name, rows);
                           sinceSave = 0;
                           QMetaObject::invokeMethod(this, [this, mine, id] {
                               if (!closed && mine == historyGeneration) {
                                   history = DroughtHistory::read(id);
                                   showHistory();
                               }
                           }, Qt::QueuedConnection);
                       };
                       for (auto month = lastMonth; month >= QDate{2017, 1, 1}; month = month.addMonths(-1)) {   // the newest first, so the recent years show soonest
                           if (closed || mine != historyGeneration) {
                               return;
                           }
                           const auto key = month.toString("yyyyMM").toStdString();
                           const auto label = month.toString("yyyy-MM");
                           if (!at.count(label)) {
                               DroughtHistory::Row r;
                               r.month = label;
                               at[label] = rows.size();
                               rows.push_back(r);
                           }
                           auto& r = rows[at[label]];
                           bool changed = false;
                           if (!r.hasWeather()) {
                               const double total = UtilityDrought::meanOver(precipField("p.full." + key + ".tif", root + "precip/total/monthly/p.full." + key + ".tif"), like, mask);
                               const double anomaly = UtilityDrought::meanOver(precipField("p.anom." + key + ".tif", root + "precip/anom/monthly/p.anom." + key + ".tif"), like, mask);
                               const double rank = UtilityDrought::meanOver(precipField("p.rank." + key + ".tif", root + "precip/percentile/monthly/p.rank." + key + ".tif"), like, mask);
                               const double warm = UtilityDrought::meanOver(precipField("t.anom." + key + ".tif", root + "temp/anom/monthly/t.anom." + key + ".tif"), like, mask);
                               const double warmRank = UtilityDrought::meanOver(precipField("t.rank." + key + ".tif", root + "temp/percentile/monthly/t.rank." + key + ".tif"), like, mask);
                               if (!std::isnan(total) && !std::isnan(anomaly)) {
                                   r.rain = std::max(0.0, total);
                                   r.departure = anomaly;
                                   r.normal = r.rain - anomaly;
                                   r.percent = r.normal > 1.0 ? r.rain / r.normal * 100.0 : NAN;
                                   r.rainRank = std::isnan(rank) ? NAN : std::clamp(rank, 0.0, 100.0);
                                   r.temperature = warm;
                                   r.temperatureRank = std::isnan(warmRank) ? NAN : std::clamp(warmRank, 0.0, 100.0);
                                   changed = true;
                               }
                           }
                           if (!r.hasDrought() && month >= lastMonth.addMonths(-35)) {   // the last 36 months: the map of the last Tuesday of the month
                               auto tuesday = month.addMonths(1).addDays(-1);
                               while (tuesday.dayOfWeek() != 2) {
                                   tuesday = tuesday.addDays(-1);
                               }
                               UtilityDrought::Monitor monitor;
                               std::string error;
                               if (loadMonitor(tuesday.toString("yyyyMMdd").toStdString(), monitor, error)) {
                                   const auto raster = UtilityDrought::rasterize(monitor, like.west, box.south, box.east, box.north, box.step);
                                   const auto share = UtilityDrought::share(raster, mask);
                                   r.mapDate = tuesday.toString("yyyyMMdd");
                                   for (int k = 0; k < 5; k++) {
                                       r.d[k] = share.atLeast[k];
                                   }
                                   r.dsci = share.dsci();
                                   changed = true;
                               }
                           }
                           if (changed && ++sinceSave >= 6) {
                               save();
                           }
                       }
                       if (sinceSave > 0) {
                           save();
                       }
                   },
                   [this, mine] {
                       if (closed || mine != historyGeneration) {
                           return;
                       }
                       history = DroughtHistory::read(areaId);
                       showHistory();
                   }};
}

void DroughtViewer::showHistory() {
    const int column = std::clamp(comboHistory.getIndex(), 0, 6);
    const bool inches = UIPreferences::unitsF;
    std::vector<HistoryChart::Point> points;
    for (const auto& r : history) {
        HistoryChart::Point p;
        p.month = r.month;
        double v = NAN;
        switch (column) {
            case 0: v = r.departure * (inches ? 1.0 / 25.4 : 1.0); break;
            case 1: v = r.percent; break;
            case 2: v = r.rainRank; break;
            case 3: v = r.temperature * (inches ? 1.8 : 1.0); break;
            case 4: v = r.temperatureRank; break;
            case 5: v = r.d[1]; break;
            default: v = r.dsci; break;
        }
        p.ok = !std::isnan(v);
        p.value = p.ok ? v : 0.0;
        points.push_back(p);
    }
    const QString units[] = {inches ? "in" : "mm", "%", "rank", inches ? "F" : "C", "rank", "%", "index"};
    const bool bars = column == 0 || column == 3;
    const double reference = column == 1 ? 100.0 : (column == 2 || column == 4) ? 50.0 : std::nan("");
    const QString label = areaId == "US" ? "the contiguous U.S." : QString::fromStdString(selectedAreas().empty() ? areaId : selectedAreas().front().name);
    historyChart->setSeries(points, comboHistory.getValue().c_str() + QString{" - "} + label, units[column], bars, reference, column == 3, (column == 2 || column == 4) ? 0.0 : 0.0, (column == 2 || column == 4) ? 100.0 : 0.0);
    int weather = 0, drought = 0;
    for (const auto& r : history) {
        weather += r.hasWeather();
        drought += r.hasDrought();
    }
    textHistory.setText((QString::number(history.size()) + " months in the history of " + label + " (" + QString::number(weather) + " with rain and temperature, " + QString::number(drought) +
                         " with drought shares); more are added in the background.  File: " + DroughtHistory::fileFor(areaId)).toStdString());
}

void DroughtViewer::exportHistory() {
    const auto source = DroughtHistory::fileFor(areaId);
    if (!QFile::exists(source)) {
        QMessageBox::information(this, "Export", "The history of this area is still being built: try again in a moment.");
        return;
    }
    const auto target = QFileDialog::getSaveFileName(this, "Export the history", QString{"drought_history_%1.csv"}.arg(QString::fromStdString(areaId)), "CSV files (*.csv)");
    if (target.isEmpty()) {
        return;
    }
    QFile::remove(target);
    if (!QFile::copy(source, target)) {
        QMessageBox::warning(this, "Export", "The file could not be written.");
    }
}

// the picture arrives; the status line says what it is
void DroughtViewer::showPicture(ZoomImage * target, Text * status, const std::string& url, const std::string& what, int * generation) {
    const int mine = ++(*generation);
    status->setText("Loading " + what + "...");
    new FutureBytes{this, url, [this, target, status, what, generation, mine] (const QByteArray& bytes) {
        if (closed || mine != *generation) {
            return;
        }
        if (bytes.size() < 2000 || bytes.startsWith("<")) {
            status->setText(what + " is not available right now.");
            return;
        }
        target->hasImage() ? target->setBytesKeepView(bytes) : target->setBytes(bytes);
        status->setText(what);
    }};
}

void DroughtViewer::loadPrecip() {
    static const char * sets[] = {"1wk,2wk,1,2months", "3,4,5,6months", "9,12,18,24months", "2,3,4,5yrs"};
    static const char * kinds[] = {"total", "anom", "percent"};
    int kind = std::max(0, comboKind.getIndex());
    const int period = std::max(0, comboPeriod.getIndex());
    if (period == 3 && kind != 2) {   // the years are only drawn as a percent of normal
        kind = 2;
        comboKind.block();
        comboKind.setIndex(2);
        comboKind.unblock();
    }
    const std::string url = std::string{"https://www.cpc.ncep.noaa.gov/products/Drought/Figures/precip/us.4panel.fordrought.briefing.recent."} + sets[period] + ".rain." + kinds[kind] + ".png";
    showPicture(&precipImage, &textPrecip, url, std::string{comboKind.getValue()} + ", " + comboPeriod.getValue() + " (the whole country: NOAA Climate Prediction Center, from gauge and radar analyses).", &precipGeneration);
}

void DroughtViewer::loadOutlook() {
    const int i = std::clamp(comboOutlook.getIndex(), 0, static_cast<int>(outlooks.size()) - 1);
    showPicture(&outlookImage, &textOutlook, outlooks[static_cast<size_t>(i)].url, outlooks[static_cast<size_t>(i)].label + ".  NOAA Climate Prediction Center.", &outlookGeneration);
}
