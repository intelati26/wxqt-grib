// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "misc/ForecastPointViewer.h"
#include <algorithm>
#include <cmath>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMessageBox>
#include <QRegularExpression>
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
    open = new QPushButton{"Full page...", this};
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
    , buttonCsv{this, None, "Hourly table (CSV)..."}
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

void ForecastPointViewer::exportCsv() {
    const auto target = QFileDialog::getSaveFileName(this, "Save the hourly table", QString{"forecast_%1.csv"}.arg(data->office), "CSV files (*.csv)");
    if (target.isEmpty()) {
        return;
    }
    QFile file{target};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Hourly table", "The file could not be written.");
        return;
    }
    QTextStream out{&file};
    const auto hours = tableHours(*data);
    out << "time (" << data->zone.id() << ")";
    std::vector<const UtilityForecastPoint::Parameter *> shown;
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (data->hourly.count(parameter.key)) {
            shown.push_back(&parameter);
            out << "," << parameter.label << " (" << parameter.unit << ")";
        }
    }
    out << "\n";
    std::vector<std::map<qint64, double>> maps;
    for (const auto * parameter : shown) {
        std::map<qint64, double> byHour;
        for (const auto& [t, v] : data->hourly.at(parameter->key)) {
            byHour[t] = v;
        }
        maps.push_back(std::move(byHour));
    }
    for (const auto t : hours) {
        out << QDateTime::fromSecsSinceEpoch(t, data->zone).toString("yyyy-MM-dd HH:mm");
        for (const auto& byHour : maps) {
            const auto found = byHour.find(t);
            out << "," << (found == byHour.end() ? QString{} : QString::number(found->second, 'f', 2));
        }
        out << "\n";
    }
}
