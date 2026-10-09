// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "misc/ForecastPointViewer.h"
#include <algorithm>
#include <cmath>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>
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
            {"Max dew point, F", &D::maxDew, 3, true, false}, {"Min dew point, F", &D::minDew, 3, false, false}, {"Max RH, %", &D::maxRh, 3, false, false}, {"Min RH, %", &D::minRh, 3, true, false},
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

void CardForecastPoint::setData(const std::shared_ptr<UtilityForecastPoint::Data>& data) {
    if (!data || !data->ok) {
        setVisible(false);
        return;
    }
    title->setText("Forecast point: " + data->place + "  (NWS " + data->office + (data->updated.isValid() ? ", made " + data->updated.toTimeZone(data->zone).toString("ddd h:mm ap") : QString{}) + ")");
    outlooks->setData(*data);
    table->setData(*data, true);
    setVisible(true);
}

// ---- the full page ----

ForecastPointViewer::ForecastPointViewer(Window * parent, const std::shared_ptr<UtilityForecastPoint::Data>& d)
    : Window{parent}
    , comboSeries{this}
    , data{d}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Forecast point - " + data->place.toStdString());
    title = new QLabel{"<b>" + data->place + "</b>  -  NWS " + data->office + (data->updated.isValid() ? ", forecast made " + data->updated.toTimeZone(data->zone).toString("dddd h:mm ap") : QString{}), this};
    outlooks = new ForecastPointOutlooks{this};
    outlooks->setData(*data);
    table = new ForecastPointTable{this};
    table->setData(*data, false);
    std::vector<string> labels;
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (data->hourly.count(parameter.key)) {
            labels.push_back(parameter.label);
        }
    }
    comboSeries.setList(labels);
    comboSeries.connect([this] { showSeries(); });
    chart = new ForecastPointChart{this};
    box.addWidgetReal(title);
    box.addWidgetReal(outlooks);
    box.addWidgetReal(table);
    box.addWidget(comboSeries);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(920, 820);
    showSeries();
}

void ForecastPointViewer::showSeries() {
    const auto label = comboSeries.getValue();
    for (const auto& parameter : UtilityForecastPoint::parameters()) {
        if (label == parameter.label) {
            chart->setSeries(data, parameter.key);
        }
    }
}
