// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartExport.h"
#include "ui/ChartPainter.h"
#include "misc/ForecastPointViewer.h"
#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QFile>
#include <QGroupBox>
#include <QImage>
#include <QTimeZone>
#include <QListWidget>
#include <QPushButton>
#include <future>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QTextStream>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
#include "misc/TextViewerStatic.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "ui/HoverTip.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

using UtilityForecastPoint::has;

namespace {
    QColor blend(const QColor& a, const QColor& b, double f) {
        f = std::clamp(f, 0.0, 1.0);
        return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * f, a.greenF() + (b.greenF() - a.greenF()) * f, a.blueF() + (b.blueF() - a.blueF()) * f, 0.55);
    }

    QColor temperatureColor(double f) {   // degrees F: blue under freezing, green mild, red hot
        if (f < 32.0) return blend(QColor{"#5a8fe0"}, QColor{"#bcd8f5"}, (f + 10.0) / 42.0);
        if (f < 60.0) return blend(QColor{"#bcd8f5"}, QColor{"#a9d98a"}, (f - 32.0) / 28.0);
        if (f < 80.0) return blend(QColor{"#a9d98a"}, QColor{"#f4d35e"}, (f - 60.0) / 20.0);
        return blend(QColor{"#f4d35e"}, QColor{"#e0502e"}, (f - 80.0) / 25.0);
    }
    QColor chanceColor(double percent) { return percent < 5.0 ? QColor{} : blend(QColor{"#dbeaf5"}, QColor{"#2f6fc4"}, percent / 100.0); }
    QColor windColor(double mph) { return mph < 10.0 ? QColor{} : blend(QColor{"#f5f0c0"}, QColor{"#e0502e"}, (mph - 10.0) / 40.0); }

    struct Row {
        const char * label;
        double UtilityForecastPoint::Day::* value;
        int kind;        // 0 temperature, 1 chance, 2 wind, 3 plain
        bool compact;
        bool onlyWithData;
    };

    const std::vector<Row>& rows() {
        using D = UtilityForecastPoint::Day;
        static const std::vector<Row> list{
            {"Max temp, F", &D::maxTemp, 0, true, false}, {"Min temp, F", &D::minTemp, 0, true, false}, {"Min wind chill, F", &D::minChill, 0, false, true}, {"Max heat index, F", &D::maxHeat, 0, false, true},
            {"Max wind, mph", &D::maxWind, 2, true, false}, {"Min wind, mph", &D::minWind, 2, false, false}, {"Max gust, mph", &D::maxGust, 2, true, false},
            {"Max chance of precip, %", &D::maxPop, 1, true, false}, {"Max chance of thunder, %", &D::maxThunder, 1, true, false},
            {"Max dew point, F", &D::maxDew, 3, true, false}, {"Min dew point, F", &D::minDew, 3, false, false},
            {"Max wet bulb, F", &D::maxWetBulb, 0, true, false}, {"Min wet bulb, F", &D::minWetBulb, 0, true, false}, {"Max wet bulb globe temp, F", &D::maxWbgt, 0, false, true}, {"Max RH, %", &D::maxRh, 3, false, false}, {"Min RH, %", &D::minRh, 3, true, false},
            {"Max cloud cover, %", &D::maxCloud, 3, true, false}, {"Min cloud cover, %", &D::minCloud, 3, false, false}, {"Max wave height, ft", &D::maxWave, 3, false, true},
        };
        return list;
    }

    // The order of the table's rows is the person's: dragged on a row's name, kept as the names (one list for the home card and the full screen, which show different subsets of the rows)
    const char * rowOrderPref = "FORECAST_POINT_ROW_ORDER";

    std::vector<std::string> savedRowOrder() {
        std::vector<std::string> out;
        const auto text = QString::fromStdString(Utility::readPref(rowOrderPref, ""));
        for (const auto& part : text.split('|', Qt::SkipEmptyParts)) {
            out.push_back(part.toStdString());
        }
        return out;
    }

    // the rows to show in the saved order; rows the list does not know yet keep their own order, after the ones it does
    void orderRows(std::vector<const Row *>& shown) {
        const auto saved = savedRowOrder();
        const auto rank = [&saved] (const Row * row) {
            const auto found = std::find(saved.begin(), saved.end(), std::string{row->label});
            return found == saved.end() ? saved.size() : static_cast<size_t>(found - saved.begin());
        };
        std::stable_sort(shown.begin(), shown.end(), [&rank] (const Row * a, const Row * b) { return rank(a) < rank(b); });
    }

    // The new order of the rows on show, merged into the saved list: the shown rows keep the places they had in it, in their new order, and the rest stay where they were
    void saveRowOrder(const std::vector<std::string>& shownInNewOrder) {
        auto order = savedRowOrder();
        for (const auto& row : rows()) {   // the full list, so a row never seen is placed too
            if (std::find(order.begin(), order.end(), std::string{row.label}) == order.end()) {
                order.push_back(row.label);
            }
        }
        std::vector<size_t> places;
        for (size_t i = 0; i < order.size(); i++) {
            if (std::find(shownInNewOrder.begin(), shownInNewOrder.end(), order[i]) != shownInNewOrder.end()) {
                places.push_back(i);
            }
        }
        for (size_t k = 0; k < places.size() && k < shownInNewOrder.size(); k++) {
            order[places[k]] = shownInNewOrder[k];
        }
        QStringList parts;
        for (const auto& name : order) {
            parts << QString::fromStdString(name);
        }
        Utility::writePref(rowOrderPref, parts.join('|').toStdString());
    }

    QColor outlookColor(const QString& category) {
        const auto c = category.toLower();
        if (c.isEmpty()) return QColor{};
        if (c.contains("high")) return QColor{"#ff66ff"};
        if (c.contains("moderate")) return QColor{"#ff6666"};
        if (c.contains("enhanced")) return QColor{"#ffa366"};
        if (c.contains("slight")) return QColor{"#ffe066"};
        if (c.contains("marginal")) return QColor{"#8fd28f"};
        if (c.contains("general") || c.contains("thunder")) return QColor{"#d8f0d8"};
        return QColor{};
    }
}

// ---- the weekly table ----

ForecastPointTable::ForecastPointTable(QWidget * parent) : QTableWidget{parent} {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    verticalHeader()->setDefaultSectionSize(22);
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    horizontalHeader()->setFixedHeight(44);   // two lines: the day and the date
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // drag a row's name to move the row; the order is kept
    verticalHeader()->setSectionsMovable(true);
    verticalHeader()->setToolTip("Drag a row's name up or down to put the rows in the order you like");
    QObject::connect(verticalHeader(), &QHeaderView::sectionMoved, this, [this] (int, int, int) {
        std::vector<std::string> names;
        for (int visual = 0; visual < rowCount(); visual++) {
            if (const auto * item = verticalHeaderItem(verticalHeader()->logicalIndex(visual))) {
                names.push_back(item->text().toStdString());
            }
        }
        saveRowOrder(names);
    });
}

void ForecastPointTable::setData(const UtilityForecastPoint::Data& data, bool compact) {
    std::vector<const Row *> shown;
    for (const auto& row : rows()) {
        bool any = false;
        for (const auto& day : data.days) {
            any = any || has(day.*row.value);
        }
        if ((!compact || row.compact) && (any || !row.onlyWithData)) {
            shown.push_back(&row);
        }
    }
    orderRows(shown);
    {   // a refresh starts from the sections in their own order again (the saved order is applied to the rows, not to the header)
        QSignalBlocker blocker{verticalHeader()};
        for (int i = 0; i < verticalHeader()->count(); i++) {
            verticalHeader()->moveSection(verticalHeader()->visualIndex(i), i);
        }
    }
    setRowCount(static_cast<int>(shown.size()));
    setColumnCount(static_cast<int>(data.days.size()));
    QStringList heads;
    for (const auto& day : data.days) {
        heads << day.date.toString("ddd\nMMM d");
    }
    setHorizontalHeaderLabels(heads);
    QStringList labels;
    for (const auto * row : shown) {
        labels << row->label;
    }
    setVerticalHeaderLabels(labels);
    for (size_t r = 0; r < shown.size(); r++) {
        for (size_t c = 0; c < data.days.size(); c++) {
            const double v = data.days[c].*(shown[r]->value);
            auto * item = new QTableWidgetItem{has(v) ? QString::number(static_cast<int>(std::lround(v))) : QString{}};
            item->setTextAlignment(Qt::AlignCenter);
            if (has(v)) {
                const auto color = shown[r]->kind == 0 ? temperatureColor(v) : shown[r]->kind == 1 ? chanceColor(v) : shown[r]->kind == 2 ? windColor(v) : QColor{};
                if (color.isValid()) {
                    item->setBackground(color);
                }
            }
            setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }
    resizeRowsToContents();
    setFixedHeight(sizeHint().height());
}

QSize ForecastPointTable::sizeHint() const {
    int rowsHeight = 0;
    for (int r = 0; r < rowCount(); r++) {
        rowsHeight += rowHeight(r);
    }
    return {620, 44 + rowsHeight + 2 * frameWidth() + 2};
}

QSize ForecastPointTable::minimumSizeHint() const {
    return {300, sizeHint().height()};
}

// ---- the outlooks ----

ForecastPointOutlooks::ForecastPointOutlooks(QWidget * parent) : QTableWidget{parent} {
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setRowCount(2);
    setColumnCount(3);
    setHorizontalHeaderLabels({"Day 1", "Day 2", "Day 3"});
    setVerticalHeaderLabels({"Severe thunderstorm", "Excessive rainfall"});
    horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    verticalHeader()->setDefaultSectionSize(24);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFixedHeight(horizontalHeader()->height() + 2 * 24 + 4);
}

void ForecastPointOutlooks::setData(const UtilityForecastPoint::Data& data) {
    for (int c = 0; c < 3; c++) {
        for (int r = 0; r < 2; r++) {
            const auto text = r == 0 ? data.severe[c] : data.rain[c];
            auto * item = new QTableWidgetItem{text.isEmpty() ? QString{"Not expected"} : text};
            item->setTextAlignment(Qt::AlignCenter);
            const auto color = outlookColor(text);
            if (color.isValid()) {
                item->setBackground(color);
                item->setForeground(QColor{20, 20, 20});
            }
            setItem(r, c, item);
        }
    }
}

QSize ForecastPointOutlooks::sizeHint() const {
    return {620, horizontalHeader()->height() + 2 * 24 + 4};
}

QSize ForecastPointOutlooks::minimumSizeHint() const {
    return {300, sizeHint().height()};
}

// ---- the hourly graph ----

ForecastPointChart::ForecastPointChart(QWidget * parent) : QWidget{parent} {
    setMinimumHeight(220);
    setMouseTracking(true);
}

void ForecastPointChart::setSeries(const std::shared_ptr<UtilityForecastPoint::Data>& d, const std::string& k) {
    data = d;
    key = k;
    update();
}

void ForecastPointChart::paintEvent(QPaintEvent *) {
    ChartPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), palette().window());
    p.setPen(palette().color(QPalette::WindowText));
    const auto found = data ? data->hourly.find(key) : decltype(data->hourly.end()){};
    if (!data || found == data->hourly.end() || found->second.empty()) {
        p.drawText(rect(), Qt::AlignCenter, "No data for this series at this point.");
        return;
    }
    const UtilityForecastPoint::Parameter * parameter = nullptr;
    for (const auto& candidate : UtilityForecastPoint::parameters()) {
        if (key == candidate.key) {
            parameter = &candidate;
        }
    }
    const auto& series = found->second;
    double lo = series.front().second, hi = lo;
    for (const auto& [t, v] : series) {
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    }
    if (parameter && parameter->bars) {
        lo = std::min(lo, 0.0);
    }
    if (hi - lo < 1e-9) {
        hi = lo + 1.0;
    }
    const double pad = (hi - lo) * 0.08;
    hi += pad;
    lo = parameter && parameter->bars ? lo : lo - pad;
    const QRectF plot{54.0, 28.0, width() - 66.0, height() - 62.0};
    const qint64 t0 = series.front().first, t1 = series.back().first + 3600;
    const auto x = [&] (qint64 t) { return plot.left() + plot.width() * static_cast<double>(t - t0) / static_cast<double>(t1 - t0); };
    const auto y = [&] (double v) { return plot.bottom() - (v - lo) / (hi - lo) * plot.height(); };
    p.drawText(QPointF{plot.left(), 18.0}, QString{"%1 (%2) - %3"}.arg(parameter ? parameter->label : key.c_str(), parameter ? parameter->unit : "", data->place));
    // the scale
    const double raw = (hi - lo) / 5.0, mag = std::pow(10.0, std::floor(std::log10(raw)));
    const double step = mag * (raw / mag > 5 ? 10 : raw / mag > 2 ? 5 : raw / mag > 1 ? 2 : 1);
    for (double v = std::ceil(lo / step) * step; v <= hi; v += step) {
        p.setPen(QColor{128, 128, 128, 70});
        p.drawLine(QPointF{plot.left(), y(v)}, QPointF{plot.right(), y(v)});
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRectF{0, y(v) - 8, plot.left() - 4, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', step < 1 ? 1 : 0));
    }
    // the days
    for (qint64 t = t0; t < t1; t += 3600) {
        const auto local = QDateTime::fromSecsSinceEpoch(t, data->zone);
        if (local.time().hour() == 0 || t == t0) {
            if (local.time().hour() == 0) {
                p.setPen(QColor{128, 128, 128, 120});
                p.drawLine(QPointF{x(t), plot.top()}, QPointF{x(t), plot.bottom()});
            }
            p.setPen(palette().color(QPalette::WindowText));
            p.drawText(QRectF{x(t), plot.bottom() + 4, 70, 16}, Qt::AlignLeft, local.toString("ddd M/d"));
        }
    }
    // now
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    if (now >= t0 && now <= t1) {
        p.setPen(QPen{QColor{200, 60, 60}, 1.2, Qt::DashLine});
        p.drawLine(QPointF{x(now), plot.top()}, QPointF{x(now), plot.bottom()});
    }
    // the series
    if (parameter && parameter->bars) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{"#2f6fc4"});
        for (const auto& [t, v] : series) {
            const double w = std::max(1.0, plot.width() / static_cast<double>((t1 - t0) / 3600) - 0.5);
            p.drawRect(QRectF{x(t), y(v), w, y(0.0) - y(v)});
        }
    } else {
        QPainterPath path;
        bool first = true;
        for (const auto& [t, v] : series) {
            const QPointF at{x(t) + 0.5, y(v)};
            if (first) {
                path.moveTo(at);
                first = false;
            } else {
                path.lineTo(at);
            }
        }
        p.setPen(QPen{QColor{"#c0392b"}, 2.0});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }
}

void ForecastPointChart::mouseMoveEvent(QMouseEvent * event) {
    if (!data) {
        return;
    }
    const auto found = data->hourly.find(key);
    if (found == data->hourly.end() || found->second.empty()) {
        return;
    }
    const auto& series = found->second;
    const QRectF plot{54.0, 28.0, width() - 66.0, height() - 62.0};
    const qint64 t0 = series.front().first, t1 = series.back().first + 3600;
    const double f = (event->position().x() - plot.left()) / plot.width();
    if (f < 0.0 || f > 1.0) {
        HoverTip::hide();
        return;
    }
    const qint64 t = t0 + static_cast<qint64>(f * static_cast<double>(t1 - t0));
    for (const auto& [at, v] : series) {
        if (t >= at && t < at + 3600) {
            HoverTip::show(this, event->globalPosition().toPoint(), QDateTime::fromSecsSinceEpoch(at, data->zone).toString("ddd M/d h ap") + ":  " + QString::number(v, 'f', std::abs(v) < 10 ? 2 : 0));
            return;
        }
    }
}

// ---- the home screen's card ----

CardForecastPoint::CardForecastPoint(Window * parent) : QWidget{parent} {
    auto * column = new QVBoxLayout{this};
    column->setContentsMargins(0, 4, 0, 4);
    auto * head = new QHBoxLayout;
    title = new QLabel{"Forecast point", this};
    title->setStyleSheet("font-weight: bold;");
    open = UiStandards::button(this, "Full page...");
    head->addWidget(title, 1);
    head->addWidget(open);
    column->addLayout(head);
    outlooks = new ForecastPointOutlooks{this};
    column->addWidget(outlooks);
    table = new ForecastPointTable{this};
    column->addWidget(table);
    QObject::connect(open, &QPushButton::clicked, this, [this] {
        if (onOpen) {
            onOpen();
        }
    });
    setVisible(false);
}

void CardForecastPoint::setAllowed(bool on) {
    allowed = on;
    setVisible(allowed && have);
}

void CardForecastPoint::setData(const std::shared_ptr<UtilityForecastPoint::Data>& data) {
    have = data && data->ok;
    if (!have) {
        setVisible(false);
        return;
    }
    title->setText("Forecast point: " + data->place + "  (NWS " + data->office + (data->updated.isValid() ? ", made " + data->updated.toTimeZone(data->zone).toString("ddd h:mm ap") : QString{}) + ")");
    outlooks->setData(*data);
    table->setData(*data, true);
    setVisible(allowed);
}

// ---- the full page ----

ForecastPointViewer::ForecastPointViewer(Window * parent, const std::shared_ptr<UtilityForecastPoint::Data>& d)
    : Window{parent}
    , comboPoint{this}
    , buttonOther{this, None, "Other point..."}
    , buttonRefresh{this, None, "Refresh"}
    , buttonDiscussion{this, None, "Forecast discussion"}
    , buttonCsv{this, None, "Export..."}
    , comboSeries{this}
    , data{d}
{
    setAttribute(Qt::WA_DeleteOnClose);
    // the saved locations first, then the point being looked at when it is none of them
    std::vector<string> names = Location::listOfNames();
    for (const auto& latLon : Location::getListLatLons()) {
        savedPoints.emplace_back(latLon.lat(), latLon.lon());
    }
    int at = -1;
    for (size_t i = 0; i < savedPoints.size(); i++) {
        if (std::abs(savedPoints[i].first - data->lat) < 0.01 && std::abs(savedPoints[i].second - data->lon) < 0.01) {
            at = static_cast<int>(i);
        }
    }
    if (at < 0) {
        names.push_back(data->place.toStdString() + " (" + QString::number(data->lat, 'f', 2).toStdString() + ", " + QString::number(data->lon, 'f', 2).toStdString() + ")");
        savedPoints.emplace_back(data->lat, data->lon);
        at = static_cast<int>(names.size()) - 1;
    }
    comboPoint.block();
    comboPoint.setList(names);
    comboPoint.setIndex(static_cast<size_t>(at));
    comboPoint.unblock();
    comboPoint.connect([this] {
        const auto i = static_cast<size_t>(std::max(0, comboPoint.getIndex()));
        if (i < savedPoints.size()) {
            loadPoint(savedPoints[i].first, savedPoints[i].second);
        }
    });
    buttonOther.connect([this] { choosePoint(); });
    buttonRefresh.connect([this] { loadPoint(data->lat, data->lon); });
    buttonDiscussion.connect([this] { openDiscussion(); });
    buttonCsv.connect([this] { exportCsv(); });
    rowTop.addWidget(comboPoint);
    rowTop.addWidget(buttonOther);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonDiscussion);
    rowTop.addWidget(buttonCsv);
    rowTop.addStretch();
    title = new QLabel{this};
    tabs = new QTabWidget{this};
    {   // the summary and one graph
        auto * page = new QWidget{tabs};
        auto * column = new QVBoxLayout{page};
        column->setContentsMargins(0, 4, 0, 0);
        outlooks = new ForecastPointOutlooks{page};
        table = new ForecastPointTable{page};
        chart = new ForecastPointChart{page};
        column->addWidget(outlooks);
        column->addWidget(table);
        column->addWidget(comboSeries.getView());
        column->addWidget(chart, 1);
        tabs->addTab(page, "Summary and graph");
    }
    hourlyTable = new QTableWidget{tabs};
    hourlyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    hourlyTable->setSelectionMode(QAbstractItemView::NoSelection);
    hourlyTable->verticalHeader()->setDefaultSectionSize(22);
    hourlyTable->horizontalHeader()->setDefaultSectionSize(48);
    tabs->addTab(hourlyTable, "Hourly table");
    {   // every series, one under another
        auto * scroll = new QScrollArea{tabs};
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        allGraphs = new QWidget{scroll};
        allLayout = new QVBoxLayout{allGraphs};
        scroll->setWidget(allGraphs);
        tabs->addTab(scroll, "All graphs");
    }
    comboSeries.connect([this] { showSeries(); });
    box.addLayout(rowTop);
    box.addWidgetReal(title);
    box.addWidgetReal(tabs, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 860);
    apply();
}

// every part of the page from the data of the point
void ForecastPointViewer::apply() {
    setTitle("Forecast point - " + data->place.toStdString());
    title->setText("<b>" + data->place + "</b>  -  NWS " + data->office + (data->updated.isValid() ? ", forecast made " + data->updated.toTimeZone(data->zone).toString("dddd h:mm ap") : QString{}) +
                   "  -  " + QString::number(data->lat, 'f', 3) + ", " + QString::number(data->lon, 'f', 3));
    outlooks->setData(*data);
    table->setData(*data, false);
    std::vector<string> labels;
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (data->hourly.count(parameter.key)) {
            labels.push_back(parameter.label);
        }
    }
    const auto before = comboSeries.getValue();
    comboSeries.block();
    comboSeries.setList(labels);
    comboSeries.setIndexByValue(before);
    if (comboSeries.getIndex() < 0 && !labels.empty()) {
        comboSeries.setIndex(0);
    }
    comboSeries.unblock();
    showSeries();
    fillHourly();
    fillAllGraphs();
}

void ForecastPointViewer::loadPoint(double lat, double lon) {
    const int mine = ++generation;
    title->setText("Loading the forecast of " + QString::number(lat, 'f', 3) + ", " + QString::number(lon, 'f', 3) + "...");
    auto fetched = std::make_shared<UtilityForecastPoint::Data>();
    new FutureVoid{this, [lat, lon, fetched] { *fetched = UtilityForecastPoint::fetch(lat, lon); }, [this, fetched, mine] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (!fetched->ok) {
                           title->setText("<b>" + fetched->error + "</b>");
                           return;
                       }
                       data = fetched;
                       apply();
                   }};
}

// a point of one's own: "35.2, -97.4" (a latitude and a longitude)
void ForecastPointViewer::choosePoint() {
    bool ok = false;
    const auto text = QInputDialog::getText(this, "Other point", "Latitude and longitude of the point (north and east positive), for example 44.8, -92.5:", QLineEdit::Normal, "", &ok);
    if (!ok) {
        return;
    }
    const auto parts = text.split(QRegularExpression{"[,;\\s]+"}, Qt::SkipEmptyParts);
    bool a = false, b = false;
    const double lat = parts.size() >= 2 ? parts[0].toDouble(&a) : 0.0, lon = parts.size() >= 2 ? parts[1].toDouble(&b) : 0.0;
    if (!a || !b || std::abs(lat) > 90.0 || std::abs(lon) > 180.0) {
        QMessageBox::information(this, "Other point", "Give the latitude and the longitude as two numbers, like 44.8, -92.5.");
        return;
    }
    // it joins the list, so that the saved locations stay a click away
    savedPoints.emplace_back(lat, lon);
    auto names = comboPoint.getItems();
    names.push_back("Point " + QString::number(lat, 'f', 2).toStdString() + ", " + QString::number(lon, 'f', 2).toStdString());
    comboPoint.block();
    comboPoint.setList(names);
    comboPoint.setIndex(names.size() - 1);
    comboPoint.unblock();
    loadPoint(lat, lon);
}

void ForecastPointViewer::openDiscussion() {
    const string office = data->office.toStdString();
    new FutureText{this, "AFD" + office, [this, office] (const string& text) {
        new TextViewerStatic{this, text.empty() ? string{"The forecast discussion of "} + office + " is not available right now." : text, "Forecast discussion - " + office, 860, 760};
    }};
}

void ForecastPointViewer::showSeries() {
    const auto label = comboSeries.getValue();
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (label == parameter.label) {
            chart->setSeries(data, parameter.key);
        }
    }
}

namespace {
    // the hours of the table: from the start of the first series, a week of them
    std::vector<qint64> tableHours(const UtilityForecastPoint::Data& data) {
        qint64 first = 0, last = 0;
        for (const auto& [key, series] : data.hourly) {
            if (!series.empty()) {
                first = first == 0 ? series.front().first : std::min(first, series.front().first);
                last = std::max(last, series.back().first);
            }
        }
        std::vector<qint64> hours;
        for (qint64 t = first; first != 0 && t <= std::min(last, first + 7 * 24 * 3600); t += 3600) {
            hours.push_back(t);
        }
        return hours;
    }
}

void ForecastPointViewer::fillHourly() {
    const auto hours = tableHours(*data);
    std::vector<const UtilityForecastPoint::Parameter *> shown;
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (data->hourly.count(parameter.key)) {
            shown.push_back(&parameter);
        }
    }
    hourlyTable->clear();
    hourlyTable->setRowCount(static_cast<int>(shown.size()));
    hourlyTable->setColumnCount(static_cast<int>(hours.size()));
    QStringList heads;
    for (const auto t : hours) {
        const auto local = QDateTime::fromSecsSinceEpoch(t, data->zone);
        heads << (local.time().hour() == 0 ? local.toString("ddd\nM/d") : local.toString("h ap"));
    }
    hourlyTable->setHorizontalHeaderLabels(heads);
    QStringList labels;
    for (const auto * parameter : shown) {
        labels << QString{"%1, %2"}.arg(parameter->label, parameter->unit);
    }
    hourlyTable->setVerticalHeaderLabels(labels);
    for (size_t r = 0; r < shown.size(); r++) {
        std::map<qint64, double> byHour;
        for (const auto& [t, v] : data->hourly.at(shown[r]->key)) {
            byHour[t] = v;
        }
        for (size_t c = 0; c < hours.size(); c++) {
            const auto found = byHour.find(hours[c]);
            if (found == byHour.end()) {
                continue;
            }
            const double v = found->second;
            auto * item = new QTableWidgetItem{QString::number(v, 'f', std::string{shown[r]->unit} == "in" ? 2 : 0)};
            item->setTextAlignment(Qt::AlignCenter);
            const std::string key = shown[r]->key;
            const auto color = key == "temperature" || key == "dewpoint" || key == "windChill" || key == "heatIndex" || key == "wetBulb" || key == "wetBulbGlobeTemperature" ? temperatureColor(v) : (shown[r]->bars ? chanceColor(shown[r]->unit == std::string{"%"} ? v : v * 100.0) : QColor{});
            if (color.isValid()) {
                item->setBackground(color);
            }
            hourlyTable->setItem(static_cast<int>(r), static_cast<int>(c), item);
        }
    }
    hourlyTable->horizontalHeader()->setFixedHeight(40);
}

void ForecastPointViewer::fillAllGraphs() {
    while (auto * item = allLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (!data->hourly.count(parameter.key)) {
            continue;
        }
        auto * one = new ForecastPointChart{allGraphs};
        one->setMinimumHeight(190);
        one->setSeries(data, parameter.key);
        allLayout->addWidget(one);
    }
    allLayout->addStretch();
}

namespace {
    struct ExportPoint {
        QString name;
        double lat{0.0}, lon{0.0};
        std::shared_ptr<UtilityForecastPoint::Data> data;   // empty until fetched
    };

    // Which forecast points, which rows and which days go into a table that is saved, copied or drawn as a picture: the weekly summary or the hourly values, a day to a column as on the
    // screen or a line to a day (hour), as comma separated text for a spreadsheet. What was chosen is kept for the next time.
    class ExportDialog : public QDialog {
    public:
        ExportDialog(QWidget * parent, std::vector<ExportPoint> list, size_t current)
            : QDialog{parent}
            , points{std::move(list)}
            , currentIndex{current}
        {
            setWindowTitle("Export forecast points");
            auto * column = new QVBoxLayout{this};
            auto * top = new QHBoxLayout;
            kind = UiStandards::combo(this);
            kind->addItems({"Weekly summary (a value for each day)", "Hourly values"});
            layoutCombo = UiStandards::combo(this);
            layoutCombo->addItems({"Days across (as on the screen)", "One line for each day or hour"});
            layoutCombo->setToolTip("The picture is always drawn with the days across");
            step = UiStandards::combo(this);
            step->addItems({"every hour", "every 3 hours", "every 6 hours", "every 12 hours"});
            header = new QCheckBox{"Say where and when", this};
            header->setToolTip("A first line with the place, the NWS office and the time the forecast was made");
            top->addWidget(kind);
            top->addWidget(layoutCombo);
            top->addWidget(step);
            top->addWidget(header);
            top->addStretch();
            column->addLayout(top);
            auto * lists = new QHBoxLayout;
            pointList = addColumn(lists, "Points");
            rowList = addColumn(lists, "Rows");
            dayList = addColumn(lists, "Days");
            column->addLayout(lists, 1);
            status = new QLabel{this};
            column->addWidget(status);
            auto * buttons = new QHBoxLayout;
            auto * image = UiStandards::button(this, "Save image...");
            auto * save = UiStandards::button(this, "Save CSV...");
            auto * copy = UiStandards::button(this, "Copy", "Copy the table for a spreadsheet");
            auto * close = UiStandards::button(this, "Close");
            buttons->addStretch();
            buttons->addWidget(image);
            buttons->addWidget(save);
            buttons->addWidget(copy);
            buttons->addWidget(close);
            column->addLayout(buttons);

            // what was chosen last time
            kind->setCurrentIndex(std::clamp(Utility::readPrefInt("FPEXPORT_KIND", 0), 0, 1));
            layoutCombo->setCurrentIndex(std::clamp(Utility::readPrefInt("FPEXPORT_LAYOUT", 0), 0, 1));
            step->setCurrentIndex(std::clamp(Utility::readPrefInt("FPEXPORT_STEP", 0), 0, 3));
            header->setChecked(Utility::readPref("FPEXPORT_HEADER", "true") == "true");
            shownKind = kind->currentIndex();
            step->setEnabled(shownKind == 1);
            const auto savedPoints = split(Utility::readPref("FPEXPORT_POINTS", ""), ';');
            bool anyPoint = false;
            for (size_t i = 0; i < points.size(); i++) {
                const bool was = std::find(savedPoints.begin(), savedPoints.end(), pointKey(points[i])) != savedPoints.end();
                anyPoint = anyPoint || was;
            }
            for (size_t i = 0; i < points.size(); i++) {
                const bool was = std::find(savedPoints.begin(), savedPoints.end(), pointKey(points[i])) != savedPoints.end();
                addItem(pointList, points[i].name, QString::fromStdString(pointKey(points[i])), i == currentIndex || (anyPoint && was));
            }
            // the point on show is always among them
            QObject::connect(kind, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int) {
                rememberRows(shownKind);   // the ticks of the list that is going away
                shownKind = kind->currentIndex();
                step->setEnabled(shownKind == 1);
                fillRows();
            });
            QObject::connect(image, &QPushButton::clicked, this, [this] { saveImage(); });
            QObject::connect(save, &QPushButton::clicked, this, [this] { saveCsv(); });
            QObject::connect(copy, &QPushButton::clicked, this, [this] { copyText(); });
            QObject::connect(close, &QPushButton::clicked, this, &QDialog::accept);
            fillRows();
            fillDays();
            resize(800, 540);
        }

        void done(int result) override {
            remember();
            QDialog::done(result);
        }

    private:
        static std::vector<std::string> split(const std::string& text, char separator) {
            std::vector<std::string> out;
            for (const auto& part : QString::fromStdString(text).split(separator, Qt::SkipEmptyParts)) out.push_back(part.toStdString());
            return out;
        }
        static std::string pointKey(const ExportPoint& p) {
            return QString::number(p.lat, 'f', 2).toStdString() + "," + QString::number(p.lon, 'f', 2).toStdString();
        }
        static QString rowPref(int forKind) { return forKind == 0 ? "FPEXPORT_ROWS_WEEKLY" : "FPEXPORT_ROWS_HOURLY"; }
        void rememberRows(int forKind) {
            if (rowList->count() == 0) {
                return;
            }
            QStringList marks;   // "+key" ticked, "-key" not: a row that appears later is not taken for a choice
            for (int i = 0; i < rowList->count(); i++) {
                marks << (rowList->item(i)->checkState() == Qt::Checked ? "+" : "-") + rowList->item(i)->data(Qt::UserRole).toString();
            }
            Utility::writePref(rowPref(forKind).toStdString(), marks.join('|').toStdString());
        }

        // the rows and days ticked, and the options, as the next time's start
        void remember() {
            Utility::writePrefInt("FPEXPORT_KIND", kind->currentIndex());
            Utility::writePrefInt("FPEXPORT_LAYOUT", layoutCombo->currentIndex());
            Utility::writePrefInt("FPEXPORT_STEP", step->currentIndex());
            Utility::writePref("FPEXPORT_HEADER", header->isChecked() ? "true" : "false");
            QStringList chosen;
            for (const auto& v : checked(pointList)) chosen << v.toString();
            Utility::writePref("FPEXPORT_POINTS", chosen.join(';').toStdString());
            rememberRows(shownKind);
            QStringList weekdays;
            for (int i = 0; i < dayList->count(); i++) {
                if (dayList->item(i)->checkState() == Qt::Checked) weekdays << QString::number(QDate::fromJulianDay(dayList->item(i)->data(Qt::UserRole).toLongLong()).dayOfWeek());
            }
            Utility::writePref("FPEXPORT_WEEKDAYS", weekdays.join(',').toStdString());
        }

        QListWidget * addColumn(QHBoxLayout * lists, const char * title) {
            auto * box = new QGroupBox{title, this};
            auto * inner = new QVBoxLayout{box};
            auto * list = new QListWidget{box};
            list->setSelectionMode(QAbstractItemView::NoSelection);
            auto * row = new QHBoxLayout;
            auto * all = UiStandards::button(box, "All");
            auto * none = UiStandards::button(box, "None");
            const auto setAll = [list] (Qt::CheckState state) {
                for (int i = 0; i < list->count(); i++) list->item(i)->setCheckState(state);
            };
            QObject::connect(all, &QPushButton::clicked, list, [setAll] { setAll(Qt::Checked); });
            QObject::connect(none, &QPushButton::clicked, list, [setAll] { setAll(Qt::Unchecked); });
            row->addWidget(all);
            row->addWidget(none);
            row->addStretch();
            inner->addWidget(list, 1);
            inner->addLayout(row);
            lists->addWidget(box, 1);
            return list;
        }
        const UtilityForecastPoint::Data * reference() const {
            const auto& d = points[currentIndex].data;
            return d ? d.get() : nullptr;
        }
        static void addItem(QListWidget * list, const QString& text, const QVariant& value, bool checkedNow) {
            auto * item = new QListWidgetItem{text, list};
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(checkedNow ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, value);
        }
        void fillRows() {
            rowList->clear();
            const auto * d = reference();
            if (d == nullptr) {
                return;
            }
            const auto marks = split(Utility::readPref(rowPref(kind->currentIndex()).toStdString(), ""), '|');
            const auto choice = [&marks] (const QString& key, bool fallback) {   // the saved tick of a row, or the default
                for (const auto& m : marks) {
                    if (m.size() > 1 && QString::fromStdString(m.substr(1)) == key) return m[0] == '+';
                }
                return fallback;
            };
            if (kind->currentIndex() == 0) {
                std::vector<const Row *> shown;
                for (const auto& row : rows()) shown.push_back(&row);
                orderRows(shown);
                for (const auto * row : shown) {
                    bool any = false;
                    for (const auto& day : d->days) any = any || has(day.*row->value);
                    addItem(rowList, row->label, QString::fromStdString(row->label), choice(QString::fromStdString(row->label), any));
                }
            } else {
                for (const auto& parameter : UtilityForecastPoint::parameters()) {
                    if (d->hourly.count(parameter.key)) {
                        addItem(rowList, QString{"%1, %2"}.arg(parameter.label, parameter.unit), QString{parameter.key}, choice(QString{parameter.key}, true));
                    }
                }
            }
        }
        void fillDays() {
            dayList->clear();
            const auto * d = reference();
            if (d == nullptr) {
                return;
            }
            const auto saved = split(Utility::readPref("FPEXPORT_WEEKDAYS", ""), ',');
            for (const auto& day : d->days) {
                const bool was = saved.empty() || std::find(saved.begin(), saved.end(), std::to_string(day.date.dayOfWeek())) != saved.end();
                addItem(dayList, day.date.toString("dddd MMM d"), day.date.toJulianDay(), was);
            }
        }
        static std::vector<QVariant> checked(const QListWidget * list) {
            std::vector<QVariant> out;
            for (int i = 0; i < list->count(); i++) {
                if (list->item(i)->checkState() == Qt::Checked) out.push_back(list->item(i)->data(Qt::UserRole));
            }
            return out;
        }
        static QString field(QString text, bool csv) {
            if (!csv) {
                return text.replace('\t', ' ').replace('\n', ' ');
            }
            if (text.contains(',') || text.contains('"') || text.contains('\n')) {
                text.replace('"', "\"\"");
                return '"' + text + '"';
            }
            return text;
        }
        // fetch the points that were not loaded yet, with the window kept alive
        bool gather() {
            std::vector<size_t> wanted;
            for (int i = 0; i < pointList->count(); i++) {
                if (pointList->item(i)->checkState() == Qt::Checked) wanted.push_back(static_cast<size_t>(i));
            }
            if (wanted.empty()) {
                status->setText("Tick at least one point.");
                return false;
            }
            std::vector<std::pair<size_t, std::future<UtilityForecastPoint::Data>>> pending;
            for (const auto i : wanted) {
                if (!points[i].data) {
                    const double lat = points[i].lat, lon = points[i].lon;
                    pending.emplace_back(i, std::async(std::launch::async, [lat, lon] { return UtilityForecastPoint::fetch(lat, lon); }));
                }
            }
            for (size_t n = 0; n < pending.size(); n++) {
                status->setText(QString{"Reading the forecast of %1 (%2 of %3)..."}.arg(points[pending[n].first].name).arg(n + 1).arg(pending.size()));
                while (pending[n].second.wait_for(std::chrono::milliseconds(40)) != std::future_status::ready) {
                    QApplication::processEvents();
                }
                auto fetched = std::make_shared<UtilityForecastPoint::Data>(pending[n].second.get());
                if (fetched->ok) points[pending[n].first].data = fetched;
            }
            status->clear();
            return true;
        }

        // one point's table: the columns (a day, or an hour of a chosen day) and the rows with a way to read each value
        struct Col { QString label; QDate date; qint64 time{0}; bool dayStart{false}; };
        struct RowOut {
            QString label;
            std::function<double(const Col&)> value;
            int kind{3};          // how a value is tinted: 0 temperature, 1 chance, 2 wind, 3 none, 5 bars of amounts
            int decimals{0};
        };
        struct Block {
            QString title;
            QString zoneId;
            bool hourly{false};
            std::vector<Col> cols;
            std::vector<RowOut> rows;
        };

        std::vector<Block> blocks() {
            std::vector<Block> out;
            if (!gather()) {
                return out;
            }
            const auto rowKeys = checked(rowList);
            const auto dayKeys = checked(dayList);
            if (rowKeys.empty() || dayKeys.empty()) {
                status->setText("Tick at least one row and one day.");
                return out;
            }
            std::vector<qint64> julian;
            for (const auto& v : dayKeys) julian.push_back(v.toLongLong());
            const bool hourly = kind->currentIndex() == 1;
            static const int steps[] = {1, 3, 6, 12};
            const int every = steps[std::clamp(step->currentIndex(), 0, 3)];
            for (int i = 0; i < pointList->count(); i++) {
                if (pointList->item(i)->checkState() != Qt::Checked || !points[static_cast<size_t>(i)].data) continue;
                const auto& pt = points[static_cast<size_t>(i)];
                const auto& d = *pt.data;
                Block block;
                block.hourly = hourly;
                block.zoneId = QString::fromLatin1(d.zone.id());
                block.title = (pt.name == d.place ? QString{} : pt.name + "  -  ") + d.place + " (NWS " + d.office + ", " + QString::number(d.lat, 'f', 3) + ", " + QString::number(d.lon, 'f', 3) + ")" +
                    (d.updated.isValid() ? ", forecast made " + d.updated.toTimeZone(d.zone).toString("yyyy-MM-dd HH:mm t") : QString{});
                if (!hourly) {
                    for (const auto& day : d.days) {
                        if (std::find(julian.begin(), julian.end(), day.date.toJulianDay()) != julian.end()) block.cols.push_back({day.date.toString("yyyy-MM-dd"), day.date, 0, true});
                    }
                } else {
                    for (const auto t : tableHours(d)) {
                        const auto local = QDateTime::fromSecsSinceEpoch(t, d.zone);
                        if (std::find(julian.begin(), julian.end(), local.date().toJulianDay()) != julian.end() && local.time().hour() % every == 0) {
                            block.cols.push_back({local.toString("yyyy-MM-dd HH:mm"), local.date(), t, local.time().hour() == 0});
                        }
                    }
                }
                for (const auto& key : rowKeys) {
                    if (!hourly) {
                        for (const auto& row : rows()) {
                            if (QString::fromStdString(row.label) != key.toString()) continue;
                            const auto member = row.value;
                            block.rows.push_back({QString::fromStdString(row.label), [&d, member] (const Col& c) {
                                for (const auto& day : d.days) if (day.date == c.date) return day.*member;
                                return UtilityForecastPoint::missing;
                            }, row.kind, 0});
                        }
                    } else {
                        for (const auto& parameter : UtilityForecastPoint::parameters()) {
                            if (QString{parameter.key} != key.toString() || !d.hourly.count(parameter.key)) continue;
                            auto map = std::make_shared<std::map<qint64, double>>();
                            for (const auto& [t, v] : d.hourly.at(parameter.key)) (*map)[t] = v;
                            const std::string k = parameter.key;
                            const bool temperature = k == "temperature" || k == "dewpoint" || k == "windChill" || k == "heatIndex" || k == "wetBulb" || k == "wetBulbGlobeTemperature";
                            const int tint = temperature ? 0 : (parameter.bars ? (std::string{parameter.unit} == "%" ? 1 : 5) : 3);
                            block.rows.push_back({QString{"%1, %2"}.arg(parameter.label, parameter.unit), [map] (const Col& c) {
                                const auto found = map->find(c.time);
                                return found == map->end() ? UtilityForecastPoint::missing : found->second;
                            }, tint, std::string{parameter.unit} == "in" ? 2 : 0});
                        }
                    }
                }
                out.push_back(std::move(block));
            }
            return out;
        }

        QString build(bool csv) {
            const auto all = blocks();
            if (all.empty()) {
                return {};
            }
            const auto sep = csv ? QString{","} : QString{"\t"};
            const bool wide = layoutCombo->currentIndex() == 0;
            const auto number = [] (double v) {   // two decimals at most, no trailing zeros
                if (!has(v)) return QString{};
                auto text = QString::number(v, 'f', 2);
                while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) text.chop(1);
                return text;
            };
            QString out;
            for (size_t n = 0; n < all.size(); n++) {
                const auto& block = all[n];
                if (n > 0) out += "\n";
                if (header->isChecked() || all.size() > 1) out += field("# " + block.title, false) + "\n";
                const QString first = block.hourly ? QString{"time (%1)"}.arg(block.zoneId) : QString{"date"};
                if (wide) {
                    QStringList head{field(first, csv)};
                    for (const auto& c : block.cols) head << field(c.label, csv);
                    out += head.join(sep) + "\n";
                    for (const auto& row : block.rows) {
                        QStringList line{field(row.label, csv)};
                        for (const auto& c : block.cols) line << field(number(row.value(c)), csv);
                        out += line.join(sep) + "\n";
                    }
                } else {
                    QStringList head{field(first, csv)};
                    for (const auto& row : block.rows) head << field(row.label, csv);
                    out += head.join(sep) + "\n";
                    for (const auto& c : block.cols) {
                        QStringList line{field(c.label, csv)};
                        for (const auto& row : block.rows) line << field(number(row.value(c)), csv);
                        out += line.join(sep) + "\n";
                    }
                }
            }
            return out;
        }

        // the tables as one picture, days across and tinted as on the screen, a title over each
        QImage draw() {
            const auto all = blocks();
            if (all.empty()) {
                return {};
            }
            QFont font{QApplication::font()};
            font.setPixelSize(13);
            QFont bold{font};
            bold.setBold(true);
            QFont small{font};
            small.setPixelSize(11);
            const QFontMetrics fm{font};
            const int pad = 12, rowH = 24, labelW = 220;
            int widest = 0, height = pad;
            std::vector<int> cellW;
            for (const auto& block : all) {
                const int cell = block.hourly ? 40 : 66;
                cellW.push_back(cell);
                widest = std::max(widest, labelW + cell * static_cast<int>(block.cols.size()));
                height += 28 + 40 + rowH * static_cast<int>(block.rows.size()) + 22;
            }
            height += 22;
            QImage image{widest + 2 * pad, height, QImage::Format_RGB32};
            image.fill(Qt::white);
            QPainter p{&image};
            p.setRenderHint(QPainter::TextAntialiasing);
            int y = pad;
            for (size_t n = 0; n < all.size(); n++) {
                const auto& block = all[n];
                const int cw = cellW[n];
                p.setFont(bold);
                p.setPen(QColor{30, 30, 30});
                p.drawText(QRect{pad, y, image.width() - 2 * pad, 24}, Qt::AlignVCenter | Qt::AlignLeft, block.title);
                y += 28;
                p.setFont(small);
                for (size_t c = 0; c < block.cols.size(); c++) {
                    const QRect cell{pad + labelW + static_cast<int>(c) * cw, y, cw, 40};
                    p.fillRect(cell, block.cols[c].dayStart || !block.hourly ? QColor{225, 232, 240} : QColor{240, 243, 247});
                    p.setPen(QColor{40, 40, 40});
                    const auto local = block.hourly ? QDateTime::fromSecsSinceEpoch(block.cols[c].time, QTimeZone{block.zoneId.toLatin1()}) : QDateTime{};
                    const QString text = block.hourly ? (local.time().hour() == 0 ? local.toString("ddd\nM/d") : local.toString("h ap")) : block.cols[c].date.toString("ddd\nMMM d");
                    p.drawText(cell, Qt::AlignCenter, text);
                }
                y += 40;
                p.setFont(font);
                for (size_t r = 0; r < block.rows.size(); r++) {
                    const auto& row = block.rows[r];
                    p.setPen(QColor{30, 30, 30});
                    p.drawText(QRect{pad + 4, y, labelW - 8, rowH}, Qt::AlignVCenter | Qt::AlignLeft, fm.elidedText(row.label, Qt::ElideRight, labelW - 8));
                    for (size_t c = 0; c < block.cols.size(); c++) {
                        const QRect cell{pad + labelW + static_cast<int>(c) * cw, y, cw, rowH};
                        const double v = row.value(block.cols[c]);
                        if (has(v)) {
                            const auto color = row.kind == 0 ? temperatureColor(v) : row.kind == 1 ? chanceColor(v) : row.kind == 2 ? windColor(v) : row.kind == 5 ? chanceColor(v * 100.0) : QColor{};
                            if (color.isValid()) {
                                p.fillRect(cell, color);
                            }
                            p.setPen(QColor{20, 20, 20});
                            p.drawText(cell, Qt::AlignCenter, row.decimals > 0 ? QString::number(v, 'f', row.decimals) : QString::number(static_cast<int>(std::lround(v))));
                        }
                        p.setPen(QColor{215, 215, 215});
                        p.drawRect(cell);
                    }
                    y += rowH;
                }
                y += 22;
            }
            p.setFont(small);
            p.setPen(QColor{110, 110, 110});
            p.drawText(QRect{pad, image.height() - 24, image.width() - 2 * pad, 20}, Qt::AlignVCenter | Qt::AlignLeft, "National Weather Service forecast point data, made with wxqt-grib");
            return image;
        }
        void saveImage() {
            const auto image = draw();
            if (image.isNull()) {
                return;
            }
            const auto target = ChartExport::askSave(this, "Save the forecast points as a picture", "forecast points", "png", "PNG images (*.png)");
            if (target.isEmpty()) {
                return;
            }
            status->setText(image.save(target) ? "Saved " + target : "The picture could not be written.");
        }
        void saveCsv() {
            const auto text = build(true);
            if (text.isEmpty()) {
                return;
            }
            const auto target = ChartExport::askSave(this, "Save the forecast points", "forecast points", "csv", "CSV files (*.csv)");
            if (target.isEmpty()) {
                return;
            }
            QFile file{target};
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QMessageBox::warning(this, "Export", "The file could not be written.");
                return;
            }
            QTextStream out{&file};
            out << text;
            status->setText("Saved " + target);
        }
        void copyText() {
            const auto text = build(false);
            if (!text.isEmpty()) {
                QApplication::clipboard()->setText(text);
                status->setText("Copied: paste it into a spreadsheet.");
            }
        }
        std::vector<ExportPoint> points;
        size_t currentIndex;
        int shownKind{0};
        QComboBox * kind;
        QComboBox * layoutCombo;
        QComboBox * step;
        QCheckBox * header;
        QListWidget * pointList;
        QListWidget * rowList;
        QListWidget * dayList;
        QLabel * status;
    };
}


void ForecastPointViewer::exportCsv() {
    std::vector<ExportPoint> points;
    const auto names = Location::listOfNames();
    size_t current = 0;
    bool found = false;
    for (size_t i = 0; i < savedPoints.size(); i++) {
        ExportPoint point;
        point.lat = savedPoints[i].first;
        point.lon = savedPoints[i].second;
        point.name = i < names.size() ? QString::fromStdString(names[i]) : data->place;
        if (std::abs(point.lat - data->lat) < 0.01 && std::abs(point.lon - data->lon) < 0.01) {
            point.data = data;
            current = i;
            found = true;
        }
        points.push_back(point);
    }
    if (!found) {
        points.push_back({data->place, data->lat, data->lon, data});
        current = points.size() - 1;
    }
    ExportDialog dialog{this, std::move(points), current};
    dialog.exec();
}
