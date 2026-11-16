// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/EnsemblePercentileViewer.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimeZone>
#include <QVBoxLayout>
#include "settings/Location.h"
#include "ui/ChartExport.h"
#include "ui/FlowBox.h"
#include "ui/UiStandards.h"
#include "util/DisplayUnits.h"

namespace {
    using E = EnsemblePercentiles::Fan;
    constexpr double measurable = 0.254;   // 0.01 inch, in millimetres: what "measurable" means
    const QColor wide{70, 120, 200, 55}, middle{70, 120, 200, 110}, line{20, 60, 150}, extreme{70, 120, 200, 150};
}

// The three panels: temperature, the precipitation added up, the chance of measurable precipitation in each six hours.
class PercentileChart : public QWidget {
public:
    explicit PercentileChart(QWidget * parent) : QWidget{parent} {
        setMinimumSize(640, 460);
        setMouseTracking(true);
        ChartExport::install(this, "Ensemble percentiles");
    }
    void clear(const QString& text) {
        have = false;
        message = text;
        update();
    }
    void set(const QString& title, const EnsemblePercentiles::Series& temperature, const EnsemblePercentiles::Series& precipitation) {
        titleText = title;
        members = static_cast<int>(temperature.members.size());
        tempFan = EnsemblePercentiles::fan(temperature);
        const auto total = EnsemblePercentiles::accumulated(precipitation);
        rainFan = EnsemblePercentiles::fan(total);
        chance = EnsemblePercentiles::chanceOfAtLeast(precipitation, 6, measurable);
        have = true;
        hover = -1;
        update();
    }

protected:
    void mouseMoveEvent(QMouseEvent * event) override {
        if (have && plotWidth > 0 && !tempFan.times.empty()) {
            const double t = t0 + (event->position().x() - left) / plotWidth * (t1 - t0);
            int best = -1;
            double nearest = 1e18;
            for (size_t i = 0; i < tempFan.times.size(); i++) {
                const double d = std::fabs(tempFan.times[i] - t);
                if (d < nearest) {
                    nearest = d;
                    best = static_cast<int>(i);
                }
            }
            hover = event->position().x() >= left && event->position().x() <= left + plotWidth ? best : -1;
            update();
        }
    }
    void leaveEvent(QEvent *) override {
        hover = -1;
        update();
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        p.setPen(Qt::black);
        const QFont base = p.font();
        if (!have) {
            p.drawText(rect(), Qt::AlignCenter, message.isEmpty() ? "Choose a place and a model, then Load." : message);
            return;
        }
        QFont bold = base;
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{10, 4, width() - 20.0, 22}, Qt::AlignLeft | Qt::AlignVCenter, titleText);
        p.setFont(base);
        left = 62;
        const double right = 34, top = 72, gap = 40, bottom = 44;
        plotWidth = width() - left - right;
        const double room = height() - top - bottom - 2 * gap;
        const QRectF tempBox{left, top, plotWidth, room * 0.44}, rainBox{left, tempBox.bottom() + gap, plotWidth, room * 0.30}, chanceBox{left, rainBox.bottom() + gap, plotWidth, room * 0.26};
        t0 = tempFan.times.front();
        t1 = tempFan.times.back();
        for (size_t i = tempFan.times.size(); i-- > 0;) {   // the axis ends at the last hour with values (a model that stops at 10 days leaves the rest of the request empty)
            if (tempFan.count[i] >= 2) {
                t1 = tempFan.times[i];
                break;
            }
        }
        // temperature
        const auto tconv = [] (float v) { return static_cast<double>(DisplayUnits::value(static_cast<double>(v), "deg C")); };
        panel(p, tempBox, tempFan, tconv, QString::fromStdString("2 m temperature (" + DisplayUnits::label("deg C") + ")"), false);
        // the precipitation added up
        const auto pconv = [] (float v) { return static_cast<double>(DisplayUnits::value(static_cast<double>(v), "mm")); };
        panel(p, rainBox, rainFan, pconv, QString::fromStdString("Precipitation added up since the first hour (" + DisplayUnits::label("mm") + ")"), true);
        drawChance(p, chanceBox);
        // the time axis, once, under the last panel
        p.setPen(Qt::black);
        p.drawText(QRectF{left, chanceBox.bottom() + 20, plotWidth, 16}, Qt::AlignRight | Qt::AlignVCenter, "times are UTC (day lines at 00 UTC)");
        key(p, tempBox);
        readout(p, tempBox, rainBox, chanceBox, tconv, pconv);
    }

private:
    double xOf(double t) const { return left + (t - t0) / std::max(1.0, t1 - t0) * plotWidth; }

    void dayLines(QPainter& p, const QRectF& box, bool labels) const {
        QDateTime day = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t0), QTimeZone::utc());
        day.setTime(QTime{0, 0});
        if (day.toSecsSinceEpoch() < static_cast<qint64>(t0)) {
            day = day.addDays(1);
        }
        const int every = (t1 - t0) > 9 * 86400 ? 2 : 1;
        int n = 0;
        for (; day.toSecsSinceEpoch() <= static_cast<qint64>(t1); day = day.addDays(1), n++) {
            const double x = xOf(static_cast<double>(day.toSecsSinceEpoch()));
            p.setPen(QPen{QColor{215, 215, 215}, 1.0});
            p.drawLine(QPointF{x, box.top()}, QPointF{x, box.bottom()});
            if (labels && n % every == 0) {
                p.setPen(Qt::black);
                p.drawText(QRectF{x - 30, box.bottom() + 2, 60, 14}, Qt::AlignCenter, day.toString("ddd d"));
            }
        }
    }

    template <typename Convert>
    void panel(QPainter& p, const QRectF& box, const E& fan, const Convert& convert, const QString& title, bool fromZero) {
        double lo = 1e18, hi = -1e18;
        for (const int level : {0, 100}) {
            for (const float v : fan.value.at(level)) {
                if (!std::isnan(v)) {
                    lo = std::min(lo, convert(v));
                    hi = std::max(hi, convert(v));
                }
            }
        }
        if (lo > hi) {
            p.setPen(Qt::black);
            p.drawText(box, Qt::AlignCenter, title + ": no values");
            return;
        }
        if (fromZero) {
            lo = 0.0;
        }
        const double span = std::max(hi - lo, 1e-6);
        // a round step for the axis
        const double rough = span / 4.0, mag = std::pow(10.0, std::floor(std::log10(rough)));
        double step = mag;
        for (const double m : {1.0, 2.0, 2.5, 5.0, 10.0}) {
            if (mag * m >= rough) {
                step = mag * m;
                break;
            }
        }
        const double axisLow = fromZero ? 0.0 : std::floor(lo / step) * step, axisHigh = std::ceil(hi / step) * step + (hi == axisLow ? step : 0.0);
        const auto yOf = [&] (double v) { return box.bottom() - (v - axisLow) / std::max(1e-9, axisHigh - axisLow) * box.height(); };
        p.setPen(QPen{QColor{150, 150, 150}, 1.0});
        p.drawRect(box);
        p.save();
        p.setClipRect(box);
        dayLines(p, box, false);
        p.restore();
        p.setPen(Qt::black);
        for (double v = axisLow; v <= axisHigh + step * 0.01; v += step) {
            const double y = yOf(v);
            p.setPen(QPen{QColor{230, 230, 230}, 1.0});
            p.drawLine(QPointF{box.left(), y}, QPointF{box.right(), y});
            p.setPen(Qt::black);
            p.drawText(QRectF{box.left() - 58, y - 8, 52, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', step < 1.0 ? 2 : (step < 10 && std::fabs(step - std::round(step)) > 1e-6 ? 1 : 0)));
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{box.left(), box.top() - 18, box.width(), 16}, Qt::AlignLeft | Qt::AlignVCenter, title);
        const auto band = [&] (int a, int b, const QColor& color) {
            QPolygonF upper, lower;
            for (size_t i = 0; i < fan.times.size(); i++) {
                const float va = fan.value.at(a)[i], vb = fan.value.at(b)[i];
                if (std::isnan(va) || std::isnan(vb)) {
                    if (upper.size() >= 2) {
                        p.setPen(Qt::NoPen);
                        p.setBrush(color);
                        std::reverse(lower.begin(), lower.end());
                        p.drawPolygon(upper + lower);
                    }
                    upper.clear();
                    lower.clear();
                    continue;
                }
                upper << QPointF{xOf(fan.times[i]), yOf(convert(vb))};
                lower << QPointF{xOf(fan.times[i]), yOf(convert(va))};
            }
            if (upper.size() >= 2) {
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                std::reverse(lower.begin(), lower.end());
                p.drawPolygon(upper + lower);
            }
        };
        const auto curve = [&] (const std::vector<float>& values, const QPen& pen) {
            QPainterPath path;
            bool pen_down = false;
            for (size_t i = 0; i < fan.times.size() && i < values.size(); i++) {
                if (std::isnan(values[i])) {
                    pen_down = false;
                    continue;
                }
                const QPointF pt{xOf(fan.times[i]), yOf(convert(values[i]))};
                pen_down ? path.lineTo(pt) : path.moveTo(pt);
                pen_down = true;
            }
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        p.save();
        p.setClipRect(box);
        band(0, 100, wide);
        band(10, 90, wide);
        band(25, 75, middle);
        curve(fan.value.at(0), QPen{extreme, 1.0, Qt::DotLine});
        curve(fan.value.at(100), QPen{extreme, 1.0, Qt::DotLine});
        curve(fan.mean, QPen{QColor{110, 110, 110}, 1.2, Qt::DashLine});
        curve(fan.value.at(50), QPen{line, 2.4});
        p.restore();
        dayLinesLabelsOnly(p, box);
    }
    void dayLinesLabelsOnly(QPainter&, const QRectF&) const {}

    void drawChance(QPainter& p, const QRectF& box) {
        p.setPen(QPen{QColor{150, 150, 150}, 1.0});
        p.drawRect(box);
        p.save();
        p.setClipRect(box);
        dayLines(p, box, false);
        p.restore();
        p.setPen(Qt::black);
        const auto label = DisplayUnits::us() ? QString{"0.01 in"} : QString{"0.25 mm"};
        p.drawText(QRectF{box.left(), box.top() - 18, box.width(), 16}, Qt::AlignLeft | Qt::AlignVCenter, "Chance of " + label + " or more in each 6 hours (share of members)");
        for (const double v : {0.0, 0.5, 1.0}) {
            const double y = box.bottom() - v * box.height();
            p.setPen(QPen{QColor{230, 230, 230}, 1.0});
            p.drawLine(QPointF{box.left(), y}, QPointF{box.right(), y});
            p.setPen(Qt::black);
            p.drawText(QRectF{box.left() - 58, y - 8, 52, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(v * 100)) + "%");
        }
        const double barWidth = std::max(1.0, (xOf(t0 + 6 * 3600) - xOf(t0)) - 1.0);
        p.setPen(Qt::NoPen);
        for (size_t i = 0; i < chance.times.size(); i++) {
            const double share = static_cast<double>(chance.share[i]);
            const double x = xOf(chance.times[i]) - barWidth;   // the block ends at `times`
            const int shade = static_cast<int>(60 + 150 * share);
            p.setBrush(QColor{40, 110, 210, shade});
            p.drawRect(QRectF{x, box.bottom() - share * box.height(), barWidth, share * box.height()});
        }
        // the day names, under this panel (the last)
        QDateTime day = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t0), QTimeZone::utc());
        day.setTime(QTime{0, 0});
        if (day.toSecsSinceEpoch() < static_cast<qint64>(t0)) {
            day = day.addDays(1);
        }
        const int every = (t1 - t0) > 9 * 86400 ? 2 : 1;
        p.setPen(Qt::black);
        for (int n = 0; day.toSecsSinceEpoch() <= static_cast<qint64>(t1); day = day.addDays(1), n++) {
            if (n % every == 0) {
                p.drawText(QRectF{xOf(static_cast<double>(day.toSecsSinceEpoch())) - 30, box.bottom() + 2, 60, 14}, Qt::AlignCenter, day.toString("ddd d"));
            }
        }
    }

    void key(QPainter& p, const QRectF& box) {
        // a small key inside the top right of the temperature panel's title line
        const QStringList names{"lowest to highest member", "10 to 90 percent", "25 to 75 percent", "median", "mean"};
        const double ky = 42;   // a line of its own under the window's title
        double x = box.right() - 6;
        QFontMetricsF fm{p.font()};
        for (int i = names.size() - 1; i >= 0; i--) {
            const double w = fm.horizontalAdvance(names[i]);
            x -= w;
            p.setPen(Qt::black);
            p.drawText(QPointF{x, ky}, names[i]);
            x -= 22;
            switch (i) {
                case 0: p.setPen(QPen{extreme, 1.0, Qt::DotLine}); p.drawLine(QPointF{x, ky - 5}, QPointF{x + 16, ky - 5}); break;
                case 1: p.fillRect(QRectF{x, ky - 10, 16, 10}, wide); break;
                case 2: p.fillRect(QRectF{x, ky - 10, 16, 10}, middle); break;
                case 3: p.setPen(QPen{line, 2.4}); p.drawLine(QPointF{x, ky - 5}, QPointF{x + 16, ky - 5}); break;
                default: p.setPen(QPen{QColor{110, 110, 110}, 1.2, Qt::DashLine}); p.drawLine(QPointF{x, ky - 5}, QPointF{x + 16, ky - 5}); break;
            }
            x -= 14;
        }
    }

    template <typename CT, typename CP>
    void readout(QPainter& p, const QRectF& tempBox, const QRectF& rainBox, const QRectF& chanceBox, const CT& ct, const CP& cp) {
        if (hover < 0 || hover >= static_cast<int>(tempFan.times.size())) {
            return;
        }
        const auto i = static_cast<size_t>(hover);
        const double x = xOf(tempFan.times[i]);
        p.setPen(QPen{QColor{90, 90, 90}, 1.0});
        p.drawLine(QPointF{x, tempBox.top()}, QPointF{x, chanceBox.bottom()});
        QStringList lines;
        lines << QDateTime::fromSecsSinceEpoch(static_cast<qint64>(tempFan.times[i]), QTimeZone::utc()).toString("ddd d MMM HH:mm 'UTC'");
        const auto row = [&] (const QString& name, const E& fan, const auto& convert, int digits) {
            if (i >= fan.times.size() || std::isnan(fan.value.at(50)[i])) {
                lines << name + ": no value";
                return;
            }
            lines << QString{"%1: %2 | %3 | %4 | %5 | %6  (lowest, 10, 50, 90, highest)"}.arg(name)
                         .arg(convert(fan.value.at(0)[i]), 0, 'f', digits).arg(convert(fan.value.at(10)[i]), 0, 'f', digits).arg(convert(fan.value.at(50)[i]), 0, 'f', digits)
                         .arg(convert(fan.value.at(90)[i]), 0, 'f', digits).arg(convert(fan.value.at(100)[i]), 0, 'f', digits);
        };
        row("Temperature", tempFan, ct, 1);
        row("Precipitation total", rainFan, cp, 2);
        for (size_t k = 0; k < chance.times.size(); k++) {   // the six hour block this time is the end of, or in
            if (tempFan.times[i] > chance.times[k] - 6 * 3600 && tempFan.times[i] <= chance.times[k]) {
                lines << QString{"Chance of measurable precipitation in the 6 hours to %1: %2%"}.arg(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(chance.times[k]), QTimeZone::utc()).toString("HH 'UTC'")).arg(static_cast<int>(std::lround(static_cast<double>(chance.share[k]) * 100.0)));
                break;
            }
        }
        lines << QString{"%1 members have a value for temperature here"}.arg(tempFan.count[i]);
        QFontMetricsF fm{p.font()};
        double w = 0;
        for (const auto& l : lines) {
            w = std::max(w, fm.horizontalAdvance(l));
        }
        const double h = lines.size() * (fm.height() + 1) + 8;
        double bx = x + 10;
        if (bx + w + 12 > width()) {
            bx = x - w - 22;
        }
        const QRectF bubble{bx, rainBox.top() - 6, w + 12, h};
        p.setBrush(QColor{255, 255, 235, 235});
        p.setPen(QPen{QColor{120, 120, 120}, 1.0});
        p.drawRect(bubble);
        p.setPen(Qt::black);
        double y = bubble.top() + 4 + fm.ascent();
        for (const auto& l : lines) {
            p.drawText(QPointF{bubble.left() + 6, y}, l);
            y += fm.height() + 1;
        }
    }

    bool have{false};
    QString message, titleText;
    int members{0};
    E tempFan, rainFan;
    EnsemblePercentiles::Chance chance;
    double left{62}, plotWidth{0}, t0{0}, t1{1};
    int hover{-1};
};

EnsemblePercentileViewer::EnsemblePercentileViewer(Window * parent, double lat, double lon, const std::string& modelId) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Ensemble percentiles at a place");
    auto * body = new QWidget;
    auto * column = new QVBoxLayout{body};
    auto * top = new FlowLayout{6};
    place = UiStandards::combo(body);
    place->addItem("Latitude / longitude");
    const auto names = Location::listOfNames();
    const auto points = Location::getListLatLons();
    for (size_t i = 0; i < names.size() && i < points.size(); i++) {
        place->addItem(QString::fromStdString(names[i]), QPointF{points[i].lon(), points[i].lat()});
    }
    latBox = new QDoubleSpinBox{body};
    latBox->setRange(-90.0, 90.0);
    latBox->setDecimals(2);
    latBox->setValue(lat);
    lonBox = new QDoubleSpinBox{body};
    lonBox->setRange(-180.0, 180.0);
    lonBox->setDecimals(2);
    lonBox->setValue(lon);
    model = UiStandards::combo(body);
    for (const auto& m : EnsemblePercentiles::models()) {
        model->addItem(QString::fromStdString(m.label), QString::fromStdString(m.id));
    }
    model->setCurrentIndex(std::max(0, model->findData(QString::fromStdString(modelId))));
    go = UiStandards::button(body, "Load");
    status = new QLabel{"", body};
    top->addWidget(place);
    top->addWidget(new QLabel{"lat", body});
    top->addWidget(latBox);
    top->addWidget(new QLabel{"lon", body});
    top->addWidget(lonBox);
    top->addWidget(model);
    top->addWidget(go);
    column->addLayout(top);
    column->addWidget(status);
    chart = new PercentileChart{body};
    column->addWidget(chart, 1);
    setCentralWidget(body);
    QObject::connect(place, &QComboBox::currentIndexChanged, this, [this] (int index) {
        if (index > 0) {
            const auto point = place->itemData(index).toPointF();
            lonBox->setValue(point.x());
            latBox->setValue(point.y());
            load();
        }
    });
    QObject::connect(model, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    QObject::connect(go, &QPushButton::clicked, this, [this] { load(); });
    setSize(1100, 800);
    QWidget::show();
    load();
}

void EnsemblePercentileViewer::load() {
    const int mine = ++generation;
    const double lat = latBox->value(), lon = lonBox->value();
    const auto id = model->currentData().toString().toStdString();
    const auto label = model->currentText();
    loading = true;
    go->setEnabled(false);
    status->setText("reading the members...");
    QPointer<EnsemblePercentileViewer> self{this};
    std::thread{[self, mine, lat, lon, id, label] {
        EnsemblePercentiles::Series t, r;
        std::string why;
        const bool ok = EnsemblePercentiles::fetch(lat, lon, id, t, r, why);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, mine, ok, t, r, why, lat, lon, label] {
            if (!self || self->generation != mine) {
                return;
            }
            self->loading = false;
            self->go->setEnabled(true);
            if (!ok) {
                self->status->setText("could not read the ensemble: " + QString::fromStdString(why));
                self->chart->clear("No ensemble: " + QString::fromStdString(why));
                return;
            }
            const auto first = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t.times.front()), QTimeZone::utc());
            self->status->setText(QString{"%1 members, hours from %2 UTC (Open-Meteo)"}.arg(t.members.size()).arg(first.toString("ddd d MMM HH:mm")));
            self->chart->set(QString{"%1 at %2, %3"}.arg(label).arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2), t, r);
        }, Qt::QueuedConnection);
    }}.detach();
}

QPixmap EnsemblePercentileViewer::picture() const {
    return chart->grab();
}

QString EnsemblePercentileViewer::statusText() const {
    return status->text();
}
