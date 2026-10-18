// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/TempHistoryViewer.h"
#include <algorithm>
#include <cmath>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QToolTip>
#include <QMouseEvent>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "util/DisplayUnits.h"
#include "util/UtilityIO.h"

namespace {
    struct YearValue {
        int year{0};
        double value{std::nan("")};   // degrees C
        int count{0};
    };
}

// the bars: one for each year
class TempHistoryChart : public QWidget {
public:
    explicit TempHistoryChart(QWidget * parent) : QWidget{parent} {
        setMouseTracking(true);
        setMinimumSize(500, 300);
    }
    std::vector<YearValue> years;
    int shown{0};              // the year asked for
    double climate{std::nan("")};   // the 1991-2020 average, degrees C
    QString title;
    void setData(std::vector<YearValue> y, int year, double clim, const QString& t) {
        years = std::move(y);
        shown = year;
        climate = clim;
        title = t;
        update();
    }

protected:
    QRectF plot() const { return QRectF{64.0, 34.0, width() - 84.0, height() - 74.0}; }
    void range(double& lo, double& hi) const {
        lo = 1e9;
        hi = -1e9;
        for (const auto& y : years) {
            if (!std::isnan(y.value)) {
                lo = std::min(lo, y.value);
                hi = std::max(hi, y.value);
            }
        }
        if (lo > hi) {
            lo = 0;
            hi = 1;
        }
        const double pad = std::max(1.0, (hi - lo) * 0.08);
        lo -= pad;
        hi += pad;
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), palette().window());
        const auto r = plot();
        p.setPen(palette().text().color());
        QFont bold = p.font();
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{0, 4, double(width()), 24}, Qt::AlignCenter, title);
        p.setFont(font());
        if (years.empty()) {
            p.drawText(rect(), Qt::AlignCenter, "no data yet");
            return;
        }
        double lo, hi;
        range(lo, hi);
        const auto yOf = [&] (double c) { return r.bottom() - (c - lo) / (hi - lo) * r.height(); };
        // the axis in the program's units
        const double dispLo = DisplayUnits::value(lo, "deg C"), dispHi = DisplayUnits::value(hi, "deg C");
        const double step = std::max(1.0, std::pow(10.0, std::floor(std::log10((dispHi - dispLo) / 5.0))) * ((dispHi - dispLo) / 5.0 > 5 * std::pow(10.0, std::floor(std::log10((dispHi - dispLo) / 5.0))) ? 5 : (dispHi - dispLo) / 5.0 > 2 * std::pow(10.0, std::floor(std::log10((dispHi - dispLo) / 5.0))) ? 2 : 1));
        p.setPen(QColor{128, 128, 128, 90});
        for (double v = std::ceil(dispLo / step) * step; v <= dispHi; v += step) {
            const double c = DisplayUnits::us() ? (v - 32.0) / 1.8 : v;
            p.drawLine(QPointF{r.left(), yOf(c)}, QPointF{r.right(), yOf(c)});
            p.setPen(palette().text().color());
            p.drawText(QRectF{0, yOf(c) - 9, r.left() - 6, 18}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', 0));
            p.setPen(QColor{128, 128, 128, 90});
        }
        p.setPen(palette().text().color());
        p.drawText(QRectF{4, r.top() - 18, 120, 16}, Qt::AlignLeft, QString::fromStdString(DisplayUnits::label("deg C")));
        const double barW = r.width() / years.size();
        const int first = years.front().year;
        for (size_t i = 0; i < years.size(); i++) {
            const auto& y = years[i];
            if (std::isnan(y.value)) {
                continue;
            }
            const double departure = std::isnan(climate) ? 0.0 : y.value - climate;
            const double t = std::clamp(departure / 3.0, -1.0, 1.0);
            const QColor color = t < 0 ? QColor::fromRgbF(0.25 + 0.5 * (1 + t), 0.45 + 0.4 * (1 + t), 0.9, 1.0) : QColor::fromRgbF(0.9, 0.6 - 0.4 * t, 0.45 - 0.3 * t, 1.0);
            const double top = yOf(y.value), base = std::isnan(climate) ? r.bottom() : yOf(climate);
            QRectF bar{r.left() + i * barW + barW * 0.1, std::min(top, base), barW * 0.8, std::abs(top - base)};
            p.setPen(Qt::NoPen);
            p.setBrush(color);
            p.drawRect(bar);
            if (y.year == shown) {
                p.setPen(QPen{palette().text().color(), 2.0});
                p.setBrush(Qt::NoBrush);
                p.drawRect(bar.adjusted(-1, -1, 1, 1));
            }
        }
        if (!std::isnan(climate)) {
            p.setPen(QPen{palette().text().color(), 1.2, Qt::DashLine});
            p.drawLine(QPointF{r.left(), yOf(climate)}, QPointF{r.right(), yOf(climate)});
            p.drawText(QRectF{r.left() + 4, yOf(climate) - 18, 200, 16}, Qt::AlignLeft, "1991-2020 average");
        }
        // the trend: a straight line through the years
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        int n = 0;
        for (const auto& y : years) {
            if (!std::isnan(y.value)) {
                sx += y.year;
                sy += y.value;
                sxx += double(y.year) * y.year;
                sxy += y.year * y.value;
                n++;
            }
        }
        if (n > 10) {
            const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx), icpt = (sy - slope * sx) / n;
            p.setPen(QPen{QColor{20, 20, 20}, 1.6});
            const double a = years.front().year, b = years.back().year;
            p.drawLine(QPointF{r.left() + barW * 0.5, yOf(icpt + slope * a)}, QPointF{r.right() - barW * 0.5, yOf(icpt + slope * b)});
            p.drawText(QRectF{r.right() - 330, r.top() - 18, 326, 16}, Qt::AlignRight,
                       QString{"trend %1 %2 a decade"}.arg(DisplayUnits::value(slope * 10, "deg C", DisplayUnits::Kind::Delta) >= 0 ? "+" : "").arg(DisplayUnits::value(slope * 10, "deg C", DisplayUnits::Kind::Delta), 0, 'f', 2));
        }
        p.setPen(palette().text().color());
        for (int y = (first / 10 + 1) * 10; y <= years.back().year; y += 10) {
            const double x = r.left() + (y - first) * barW + barW / 2;
            p.drawLine(QPointF{x, r.bottom()}, QPointF{x, r.bottom() + 4});
            p.drawText(QRectF{x - 24, r.bottom() + 5, 48, 16}, Qt::AlignCenter, QString::number(y));
        }
    }
    void mouseMoveEvent(QMouseEvent * e) override {
        const auto r = plot();
        if (years.empty() || !r.contains(e->position())) {
            QToolTip::hideText();
            return;
        }
        const size_t i = std::min(years.size() - 1, static_cast<size_t>((e->position().x() - r.left()) / (r.width() / years.size())));
        const auto& y = years[i];
        if (std::isnan(y.value)) {
            return;
        }
        QString text = QString{"%1: %2 %3"}.arg(y.year).arg(DisplayUnits::value(y.value, "deg C"), 0, 'f', 1).arg(QString::fromStdString(DisplayUnits::label("deg C")));
        if (!std::isnan(climate)) {
            const double d = DisplayUnits::value(y.value - climate, "deg C", DisplayUnits::Kind::Delta);
            text += QString{"  (%1%2 from the 1991-2020 average)"}.arg(d >= 0 ? "+" : "").arg(d, 0, 'f', 1);
        }
        QToolTip::showText(e->globalPosition().toPoint(), text, this);
    }
};

TempHistoryViewer::TempHistoryViewer(Window * parent, double lat, double lon, const QDate& upTo) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Temperature history at a place (ERA5)");
    auto * page = new QWidget{this};
    auto * column = new QVBoxLayout{page};
    auto * row = new QHBoxLayout;
    savedBox = new QComboBox{page};
    savedBox->addItem("Saved locations...", QVariant{});
    {
        const auto places = Location::getListLatLons();
        const auto names = Location::listOfNames();
        for (size_t i = 0; i < places.size(); i++) {
            savedBox->addItem(QString::fromStdString(i < names.size() ? names[i] : std::string{"Location"}), QPointF{places[i].lat(), places[i].lon()});
        }
    }
    latBox = new QDoubleSpinBox{page};
    lonBox = new QDoubleSpinBox{page};
    latBox->setRange(-90, 90);
    lonBox->setRange(-180, 180);
    latBox->setDecimals(2);
    lonBox->setDecimals(2);
    latBox->setValue(lat);
    lonBox->setValue(lon > 180 ? lon - 360 : lon);
    latBox->setPrefix("lat ");
    lonBox->setPrefix("lon ");
    periodBox = new QComboBox{page};
    periodBox->addItems({"Week (7 days up to the date)", "Month (to the date)", "Year (to the date)"});
    metricBox = new QComboBox{page};
    metricBox->addItems({"Average temperature", "Average high", "Average low"});
    dateEdit = new QDateEdit{page};
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    dateEdit->setDateRange(QDate{1940, 1, 1}, QDate::currentDate().addDays(-6));
    dateEdit->setDate(upTo.isValid() ? std::min(upTo, QDate::currentDate().addDays(-6)) : QDate::currentDate().addDays(-6));
    reloadButton = new QPushButton{"Load", page};
    for (QWidget * w : std::vector<QWidget *>{savedBox, latBox, lonBox, periodBox, metricBox, dateEdit, reloadButton}) {
        row->addWidget(w);
    }
    row->addStretch();
    column->addLayout(row);
    status = new QLabel{page};
    status->setWordWrap(true);
    column->addWidget(status);
    chart = new TempHistoryChart{page};
    column->addWidget(chart, 1);
    summary = new QLabel{page};
    summary->setWordWrap(true);
    summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    column->addWidget(summary);
    setCentralWidget(page);
    setSize(1100, 760);
    QObject::connect(reloadButton, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(savedBox, &QComboBox::activated, this, [this] (int i) {
        const auto point = savedBox->itemData(i).toPointF();
        if (i > 0) {
            latBox->setValue(point.x());
            lonBox->setValue(point.y());
            load();
        }
    });
    for (auto * box : {periodBox, metricBox}) {
        QObject::connect(box, &QComboBox::currentIndexChanged, this, [this] { recompute(); });
    }
    QObject::connect(dateEdit, &QDateEdit::dateChanged, this, [this] { recompute(); });
    QWidget::show();
    load();
}

// the whole record of daily temperatures at the place, in one request
void TempHistoryViewer::load() {
    if (loading) {
        return;
    }
    loading = true;
    const double lat = latBox->value(), lon = lonBox->value();
    status->setText("Reading the ERA5 daily temperatures since 1940 for " + QString::number(lat, 'f', 2) + ", " + QString::number(lon, 'f', 2) + " ...");
    auto result = std::make_shared<std::vector<Day>>();
    auto note = std::make_shared<QString>();
    auto where = std::make_shared<std::pair<double, double>>(lat, lon);
    const QString last = QDate::currentDate().addDays(-6).toString("yyyy-MM-dd");
    new FutureVoid{this, [=] {
        const auto url = "https://archive-api.open-meteo.com/v1/archive?latitude=" + std::to_string(lat) + "&longitude=" + std::to_string(lon) + "&start_date=1940-01-01&end_date=" + last.toStdString() +
                         "&daily=temperature_2m_mean,temperature_2m_max,temperature_2m_min&timezone=UTC&models=era5";
        const auto text = UtilityIO::getHtml(url);
        const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(text));
        const auto daily = doc.object().value("daily").toObject();
        const auto times = daily.value("time").toArray();
        if (times.isEmpty()) {
            *note = doc.object().value("reason").toString("no answer from the archive service");
            return;
        }
        const auto mean = daily.value("temperature_2m_mean").toArray(), high = daily.value("temperature_2m_max").toArray(), low = daily.value("temperature_2m_min").toArray();
        const auto number = [] (const QJsonValue& v) { return v.isNull() ? std::nan("") : v.toDouble(std::nan("")); };
        for (int i = 0; i < times.size(); i++) {
            result->push_back({QDate::fromString(times[i].toString(), "yyyy-MM-dd"), number(mean[i]), number(high[i]), number(low[i])});
        }
        where->first = doc.object().value("latitude").toDouble(lat);
        where->second = doc.object().value("longitude").toDouble(lon);
    }, [=] {
        loading = false;
        if (closed) {
            return;
        }
        if (result->empty()) {
            status->setText("No temperatures: " + *note);
            return;
        }
        days = std::move(*result);
        usedLat = where->first;
        usedLon = where->second;
        recompute();
    }};
}

void TempHistoryViewer::recompute() {
    if (days.empty()) {
        return;
    }
    const int mode = periodBox->currentIndex(), metric = metricBox->currentIndex();
    const QDate end = dateEdit->date();
    std::vector<YearValue> years;
    for (int y = days.front().date.year(); y <= days.back().date.year(); y++) {
        const int month = end.month();
        const int day = (month == 2 && end.day() == 29 && !QDate::isLeapYear(y)) ? 28 : end.day();
        const QDate stop{y, month, std::min(day, QDate{y, month, 1}.daysInMonth())};
        const QDate start = mode == 0 ? stop.addDays(-6) : mode == 1 ? QDate{y, month, 1} : QDate{y, 1, 1};
        const int expected = static_cast<int>(start.daysTo(stop)) + 1;
        double sum = 0;
        int n = 0;
        const auto from = std::lower_bound(days.begin(), days.end(), start, [] (const Day& d, const QDate& v) { return d.date < v; });
        for (auto it = from; it != days.end() && it->date <= stop; ++it) {
            const double v = metric == 0 ? it->mean : metric == 1 ? it->high : it->low;
            if (!std::isnan(v)) {
                sum += v;
                n++;
            }
        }
        YearValue value;
        value.year = y;
        value.count = n;
        if (n >= expected * 0.9 && n > 0) {
            value.value = sum / n;
        }
        years.push_back(value);
    }
    // the 1991-2020 average and the rank of the year asked for
    double climateSum = 0;
    int climateN = 0;
    for (const auto& y : years) {
        if (y.year >= 1991 && y.year <= 2020 && !std::isnan(y.value)) {
            climateSum += y.value;
            climateN++;
        }
    }
    const double climate = climateN > 0 ? climateSum / climateN : std::nan("");
    const int year = end.year();
    const YearValue * now = nullptr;
    for (const auto& y : years) {
        if (y.year == year) {
            now = &y;
        }
    }
    static const char * const periodNames[] = {"the 7 days up to", "the days of the month up to", "the days of the year up to"};
    static const char * const metricNames[] = {"average temperature", "average high", "average low"};
    const QString dayText = end.toString("MMM d");
    const QString title = QString{"%1, %2 %3 (every year %4-%5)"}.arg(metricNames[metric]).arg(periodNames[mode]).arg(dayText).arg(years.front().year).arg(years.back().year);
    chart->setData(years, year, climate, title.left(1).toUpper() + title.mid(1));
    const auto unit = QString::fromStdString(DisplayUnits::label("deg C"));
    const auto show = [&] (double c) { return QString::number(DisplayUnits::value(c, "deg C"), 'f', 1) + " " + unit; };
    const auto delta = [&] (double c) { const double d = DisplayUnits::value(c, "deg C", DisplayUnits::Kind::Delta); return QString{"%1%2 %3"}.arg(d >= 0 ? "+" : "").arg(d, 0, 'f', 1).arg(unit); };
    std::vector<const YearValue *> ranked;
    for (const auto& y : years) {
        if (!std::isnan(y.value)) {
            ranked.push_back(&y);
        }
    }
    std::sort(ranked.begin(), ranked.end(), [] (const YearValue * a, const YearValue * b) { return a->value > b->value; });
    QString text;
    if (now != nullptr && !std::isnan(now->value)) {
        const auto position = std::find(ranked.begin(), ranked.end(), now);
        const int rank = static_cast<int>(position - ranked.begin()) + 1;
        text = QString{"%1: %2"}.arg(year).arg(show(now->value));
        if (!std::isnan(climate)) {
            text += QString{", %1 from the 1991-2020 average of %2"}.arg(delta(now->value - climate)).arg(show(climate));
        }
        text += QString{". The %1 warmest of the %2 years (and the %3 coldest)."}.arg(rank).arg(ranked.size()).arg(static_cast<int>(ranked.size()) - rank + 1);
    } else {
        text = QString{"%1: not enough days (the service's record ends about five days ago)."}.arg(year);
    }
    const auto list = [&] (bool warm) {
        QStringList parts;
        for (int i = 0; i < 5 && i < static_cast<int>(ranked.size()); i++) {
            const auto * y = warm ? ranked[static_cast<size_t>(i)] : ranked[ranked.size() - 1 - static_cast<size_t>(i)];
            parts << QString{"%1 (%2)"}.arg(y->year).arg(show(y->value));
        }
        return parts.join(",  ");
    };
    text += "\nWarmest: " + list(true) + "\nColdest: " + list(false);
    summary->setText(text);
    status->setText(QString{"ERA5 reanalysis (ECMWF / Copernicus Climate Change Service, 0.25 degree), through Open-Meteo.com, for the grid point at %1, %2 (the place asked for snaps to it). Days are UTC days."}.arg(usedLat, 0, 'f', 2).arg(usedLon, 0, 'f', 2));
}
