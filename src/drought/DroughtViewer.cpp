// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "drought/DroughtViewer.h"
#include <algorithm>
#include <cmath>
#include <QHeaderView>
#include <QPainter>
#include <QPainterPath>
#include <QSplitter>
#include <QVBoxLayout>
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "objects/NetManager.h"
#include "objects/URL.h"
#include "ui/ActivityLabel.h"
#include "util/PermanentCache.h"

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
    , textPrecip{this, ""}
    , textOutlook{this, ""}
    , states{std::make_shared<std::vector<UtilityDrought::Area>>()}
    , counties{std::make_shared<std::vector<UtilityDrought::Area>>()}
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
    rowOutlook.addWidget(comboOutlook);
    page("Precipitation", rowPrecip, precipImage, textPrecip);
    page("Outlooks and soil moisture", rowOutlook, outlookImage, textOutlook);
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
    auto loaded = std::make_shared<std::pair<std::vector<UtilityDrought::Area>, std::vector<UtilityDrought::Area>>>();
    new FutureVoid{this, [loaded] {
                       std::string error;
                       UtilityDrought::parseAreas(census(false), false, false, loaded->first, error);
                       UtilityDrought::parseAreas(census(true), true, false, loaded->second, error);
                   },
                   [this, loaded] {
                       if (closed) {
                           return;
                       }
                       *states = std::move(loaded->first);
                       *counties = std::move(loaded->second);
                       map->setLand(states);
                       map->setCounties(counties);
                       if (const auto env = qgetenv("WXQT_COMPARE"); !env.isEmpty()) {   // dev: WXQT_COMPARE=<n> opens on that comparison, WXQT_AREA=<id> on that area ("08" Colorado)
                           comboCompare.setIndex(env.toInt());
                       }
                       if (const auto env = qgetenv("WXQT_AREA"); !env.isEmpty()) {
                           selectArea(env.toStdString());
                           return;
                       }
                       refreshMonitor();
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
    for (const auto& list : {states.get(), counties.get()}) {
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
    std::vector<ProductPicker::Entry> entries{{"US", "Contiguous United States", "Nation"}};
    for (const auto& s : *states) {
        entries.push_back({s.id, s.name, "States"});
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
                               auto * item = new QTableWidgetItem{QString{delta > 0 ? "+" : ""} + QString::number(delta, 'f', r == 6 ? 0 : 1) + (r == 6 ? "" : " pts") + (std::abs(delta) < 0.05 ? "" : worse ? "  \xE2\x96\xB2" : "  \xE2\x96\xBC")};
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
