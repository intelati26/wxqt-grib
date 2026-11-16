// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/ModelBiasViewer.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <QDateTime>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimeZone>
#include <QVBoxLayout>
#include "cfs/EnsemblePercentiles.h"
#include "settings/Location.h"
#include "ui/ChartExport.h"
#include "ui/FlowBox.h"
#include "ui/UiStandards.h"

namespace {
    const QColor colorA{30, 90, 200}, colorB{220, 120, 20};
    namespace E = EnsemblePercentiles;

    // everything the chart needs of one load: each model's statistics at the leads 1 to 7
    struct Result {
        QString nameA, nameB, title;
        E::Variable variable;
        int utcOffset{0}, days{30}, lead{3};
        ModelBias::Stat statA[8], statB[8];
        QString truthName{"ERA5"};
        bool station{false};
        // how each model's forecast for each day changed: model (0 = A, 1 = B) -> lead 0 to 7 -> day -> value (native units); and what happened by day
        std::map<double, float> evolution[2][8];
        std::map<double, float> happened;
        bool showEvolution{false};
    };
}

class BiasChart : public QWidget {
public:
    explicit BiasChart(QWidget * parent) : QWidget{parent} {
        setMinimumSize(640, 460);
        ChartExport::install(this, "Model bias");
    }
    void clear(const QString& text) {
        have = false;
        message = text;
        update();
    }
    void set(const Result& r) {
        result = r;
        have = true;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        p.setPen(Qt::black);
        const QFont base = p.font();
        if (!have) {
            p.drawText(rect(), Qt::AlignCenter, message.isEmpty() ? "Choose a place, two models and a variable, then Load." : message);
            return;
        }
        QFont bold = base;
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{10, 4, width() - 20.0, 22}, Qt::AlignLeft | Qt::AlignVCenter, result.title);
        p.setFont(base);
        const bool sums = result.variable.accumulate || result.station;   // (a station's rain is a day's total; its highs and lows are one value for the day)
        const QString unit = QString::fromStdString(E::unitLabel(result.variable)) + (result.variable.accumulate ? " per day" : "");
        const auto shown = [&] (float v) { return E::display(result.variable, static_cast<double>(v), true); };
        // the key
        double kx = 14;
        const auto key = [&] (const QColor& color, const QString& text) {
            p.fillRect(QRectF{kx, 33, 16, 10}, color);
            p.setPen(Qt::black);
            p.drawText(QPointF{kx + 22, 43}, text);
            kx += 22 + QFontMetricsF{p.font()}.horizontalAdvance(text) + 18;
        };
        key(colorA, result.nameA);
        key(colorB, result.nameB);
        p.setPen(Qt::black);
        p.drawText(QPointF{kx, 43}, "bar: bias (forecast minus what happened), tick: size of the miss");
        left = 62;
        const double right = 34, top = 74, gap = 52, bottom = 40;
        plotWidth = width() - left - right;
        const double room = height() - top - bottom - gap;
        const QRectF upper{left, top, plotWidth, room * 0.5}, lower{left, upper.bottom() + gap, plotWidth, room * 0.5};
        // ---- upper: by lead
        double top1 = 0.0, bottom1 = 0.0;
        for (int k = 1; k <= 7; k++) {
            for (const auto* s : {&result.statA[k], &result.statB[k]}) {
                if (s->n > 0) {
                    top1 = std::max({top1, shown(s->bias), shown(s->mae)});
                    bottom1 = std::min(bottom1, shown(s->bias));
                }
            }
        }
        Axis a1 = axis(top1, bottom1);
        frame(p, upper, a1, QString{"Bias and miss by lead time (%1), last %2 days"}.arg(unit).arg(result.days));
        const auto y1 = [&] (double v) { return upper.bottom() - (v - a1.low) / (a1.high - a1.low) * upper.height(); };
        const double slot = upper.width() / 7.0, half = std::min(26.0, slot * 0.22);
        p.setPen(QPen{QColor{90, 90, 90}, 1.0});
        p.drawLine(QPointF{upper.left(), y1(0)}, QPointF{upper.right(), y1(0)});
        for (int k = 1; k <= 7; k++) {
            const double cx = upper.left() + slot * (k - 0.5);
            p.setPen(Qt::black);
            p.drawText(QRectF{cx - slot / 2, upper.bottom() + 3, slot, 16}, Qt::AlignCenter, QString{"%1 day%2 ahead"}.arg(k).arg(k == 1 ? "" : "s"));
            for (int which = 0; which < 2; which++) {
                const auto& s = which == 0 ? result.statA[k] : result.statB[k];
                const QColor color = which == 0 ? colorA : colorB;
                const double x = cx + (which == 0 ? -half - 1 : 1);
                if (s.n == 0) {
                    p.setPen(Qt::gray);
                    p.drawText(QRectF{x, upper.top() + 4, half, 14}, Qt::AlignCenter, "-");
                    continue;
                }
                const double v = shown(s.bias);
                QColor fill = color;
                fill.setAlpha(150);
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawRect(QRectF{x, std::min(y1(0), y1(v)), half, std::max(1.0, std::fabs(y1(v) - y1(0)))});
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen{color.darker(160), 2.0});
                p.drawLine(QPointF{x - 2, y1(shown(s.mae))}, QPointF{x + half + 2, y1(shown(s.mae))});   // the size of the miss
                p.setPen(Qt::black);
                p.drawText(QRectF{x - 10, (v >= 0 ? y1(v) - 15 : y1(v) + 1), half + 20, 14}, Qt::AlignCenter, QString::number(v, 'f', result.variable.digits));
            }
        }
        if (result.showEvolution) {
            drawEvolution(p, lower);
            return;
        }
        // ---- lower: the days
        const int lead = result.lead;
        double top2 = 0.0, bottom2 = 0.0;
        double t0 = 1e18, t1 = -1e18;
        for (const auto* s : {&result.statA[lead], &result.statB[lead]}) {
            for (size_t i = 0; i < s->dayStart.size(); i++) {
                top2 = std::max(top2, shown(s->dayError[i]));
                bottom2 = std::min(bottom2, shown(s->dayError[i]));
                t0 = std::min(t0, s->dayStart[i]);
                t1 = std::max(t1, s->dayStart[i] + 86400.0);
            }
        }
        Axis a2 = axis(top2, bottom2);
        frame(p, lower, a2, QString{"The error of each day at %1 day%2 ahead (%3): a drift shows here"}.arg(lead).arg(lead == 1 ? "" : "s").arg(unit));
        if (t0 > t1) {
            p.drawText(lower, Qt::AlignCenter, "no day has both a forecast and what happened");
            return;
        }
        const auto y2 = [&] (double v) { return lower.bottom() - (v - a2.low) / (a2.high - a2.low) * lower.height(); };
        const auto x2 = [&] (double t) { return lower.left() + (t - t0) / std::max(1.0, t1 - t0) * lower.width(); };
        p.setPen(QPen{QColor{90, 90, 90}, 1.0});
        p.drawLine(QPointF{lower.left(), y2(0)}, QPointF{lower.right(), y2(0)});
        const auto zone = QTimeZone::fromSecondsAheadOfUtc(result.utcOffset);
        const int every = std::max(1, static_cast<int>(std::ceil((t1 - t0) / 86400.0 / 10.0)));
        int n = 0;
        for (double t = t0; t < t1; t += 86400.0, n++) {
            if (n % every == 0) {
                p.setPen(Qt::black);
                p.drawText(QRectF{x2(t) - 30, lower.bottom() + 3, 60, 14}, Qt::AlignCenter, QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t + 3600), zone).toString("d MMM"));
            }
        }
        for (int which = 0; which < 2; which++) {
            const auto& s = which == 0 ? result.statA[lead] : result.statB[lead];
            const QColor color = which == 0 ? colorA : colorB;
            QPainterPath path;
            for (size_t i = 0; i < s.dayStart.size(); i++) {
                const QPointF pt{x2(s.dayStart[i] + 43200.0), y2(shown(s.dayError[i]))};
                i == 0 ? path.moveTo(pt) : path.lineTo(pt);
            }
            QColor thin = color;
            thin.setAlpha(150);
            p.setPen(QPen{thin, 1.2});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
            p.setBrush(color);
            p.setPen(Qt::NoPen);
            for (size_t i = 0; i < s.dayStart.size(); i++) {
                p.drawEllipse(QPointF{x2(s.dayStart[i] + 43200.0), y2(shown(s.dayError[i]))}, 2.4, 2.4);
            }
            // the running mean of five days: the drift
            QPainterPath mean;
            bool started = false;
            for (size_t i = 0; i < s.dayStart.size(); i++) {
                double sum = 0.0;
                int count = 0;
                for (size_t j = (i >= 4 ? i - 4 : 0); j <= i; j++) {
                    sum += shown(s.dayError[j]);
                    count++;
                }
                if (i < 4) {
                    continue;
                }
                const QPointF pt{x2(s.dayStart[i] + 43200.0), y2(sum / count)};
                started ? mean.lineTo(pt) : mean.moveTo(pt);
                started = true;
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen{color, 3.0});
            p.drawPath(mean);
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{left, lower.bottom() + 20, plotWidth, 16}, Qt::AlignRight | Qt::AlignVCenter, "points: each day; thick line: the mean of the last five days; the place's own days");
    }

private:
    // the forecast for each day at every lead, 7 days ahead (pale) to the day itself (dark), the two models side by side, and what happened between them
    void drawEvolution(QPainter& p, const QRectF& lower) {
        const auto shown = [&] (float v) { return E::display(result.variable, static_cast<double>(v)); };
        double lo = 1e18, hi = -1e18, t0 = 1e18, t1 = -1e18;
        for (int m = 0; m < 2; m++) {
            for (int k = 0; k <= 7; k++) {
                for (const auto& [day, v] : result.evolution[m][k]) {
                    lo = std::min(lo, shown(v));
                    hi = std::max(hi, shown(v));
                    t0 = std::min(t0, day);
                    t1 = std::max(t1, day + 86400.0);
                }
            }
        }
        for (const auto& [day, v] : result.happened) {
            lo = std::min(lo, shown(v));
            hi = std::max(hi, shown(v));
        }
        if (t0 > t1) {
            frame(p, lower, Axis{}, "how the forecast for each day changed");
            p.drawText(lower, Qt::AlignCenter, "no days");
            return;
        }
        Axis a2 = axis(hi, lo);
        frame(p, lower, a2, QString{"How the forecast for each day changed, 7 days ahead (pale) to the day itself (dark), and what happened (black) (%1)"}.arg(QString::fromStdString(E::unitLabel(result.variable))));
        const auto y2 = [&] (double v) { return lower.bottom() - (v - a2.low) / (a2.high - a2.low) * lower.height(); };
        const auto x2 = [&] (double t) { return lower.left() + (t - t0) / std::max(1.0, t1 - t0) * lower.width(); };
        const double dayWidth = x2(t0 + 86400.0) - x2(t0), half = std::max(2.0, dayWidth * 0.18);
        const auto zone = QTimeZone::fromSecondsAheadOfUtc(result.utcOffset);
        const int every = std::max(1, static_cast<int>(std::ceil((t1 - t0) / 86400.0 / 10.0)));
        int n = 0;
        for (double t = t0; t < t1; t += 86400.0, n++) {
            if (n % every == 0) {
                p.setPen(Qt::black);
                p.drawText(QRectF{x2(t) - 30, lower.bottom() + 3, 60, 14}, Qt::AlignCenter, QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t + 3600), zone).toString("d MMM"));
            }
        }
        for (int m = 0; m < 2; m++) {
            const QColor color = m == 0 ? colorA : colorB;
            for (double t = t0; t < t1; t += 86400.0) {
                const double cx = x2(t + 43200.0) + (m == 0 ? -half - 1 : half + 1);
                QPainterPath stick;
                bool started = false;
                for (int k = 7; k >= 0; k--) {   // from the longest lead to the day itself
                    const auto found = result.evolution[m][k].find(t);
                    if (found == result.evolution[m][k].end()) {
                        continue;
                    }
                    const QPointF pt{cx, y2(shown(found->second))};
                    started ? stick.lineTo(pt) : stick.moveTo(pt);
                    started = true;
                }
                QColor line = color;
                line.setAlpha(110);
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen{line, 1.0});
                p.drawPath(stick);
                p.setPen(Qt::NoPen);
                for (int k = 7; k >= 0; k--) {
                    const auto found = result.evolution[m][k].find(t);
                    if (found == result.evolution[m][k].end()) {
                        continue;
                    }
                    QColor dot = color;
                    dot.setAlpha(60 + (7 - k) * 28);
                    p.setBrush(dot);
                    p.drawEllipse(QPointF{cx, y2(shown(found->second))}, k == 0 ? 3.2 : 2.2, k == 0 ? 3.2 : 2.2);
                }
            }
        }
        p.setBrush(Qt::black);
        p.setPen(Qt::NoPen);
        for (const auto& [day, v] : result.happened) {
            if (day >= t0 && day < t1) {
                const double cx = x2(day + 43200.0);
                p.drawPolygon(QPolygonF{{cx, y2(shown(v)) - 4}, {cx + 4, y2(shown(v))}, {cx, y2(shown(v)) + 4}, {cx - 4, y2(shown(v))}});
            }
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{left, lower.bottom() + 20, plotWidth, 16}, Qt::AlignRight | Qt::AlignVCenter, "left of each day: the first model, right: the second; diamond: what happened; the place's own days");
    }

    struct Axis {
        double low{0}, high{1}, step{1};
    };
    static Axis axis(double hi, double lo) {
        Axis a;
        const double span = std::max(hi - lo, 1e-6), rough = span / 4.0, mag = std::pow(10.0, std::floor(std::log10(rough)));
        a.step = mag;
        for (const double m : {1.0, 2.0, 2.5, 5.0, 10.0}) {
            if (mag * m >= rough) {
                a.step = mag * m;
                break;
            }
        }
        a.low = std::floor(lo / a.step) * a.step;
        a.high = std::max(std::ceil(hi / a.step) * a.step, a.low + a.step);
        if (a.low == 0.0 && lo < 0.0) {
            a.low = -a.step;
        }
        return a;
    }
    void frame(QPainter& p, const QRectF& box, const Axis& a, const QString& title) {
        p.setPen(QPen{QColor{150, 150, 150}, 1.0});
        p.drawRect(box);
        for (double v = a.low; v <= a.high + a.step * 0.01; v += a.step) {
            const double y = box.bottom() - (v - a.low) / (a.high - a.low) * box.height();
            p.setPen(QPen{QColor{230, 230, 230}, 1.0});
            p.drawLine(QPointF{box.left(), y}, QPointF{box.right(), y});
            p.setPen(Qt::black);
            p.drawText(QRectF{box.left() - 58, y - 8, 52, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', a.step < 1.0 ? 2 : (std::fabs(a.step - std::round(a.step)) > 1e-6 ? 1 : 0)));
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{box.left(), box.top() - 18, box.width(), 16}, Qt::AlignLeft | Qt::AlignVCenter, title);
    }

    bool have{false};
    QString message;
    Result result;
    double left{62}, plotWidth{0};
};

ModelBiasViewer::ModelBiasViewer(Window * parent, double lat, double lon, const std::string& idA, const std::string& idB, const std::string& variable, int days, int leadDays) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("How wrong two models have been at a place");
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
    modelA = UiStandards::combo(body);
    modelB = UiStandards::combo(body);
    for (const auto& m : ModelBias::models()) {
        modelA->addItem(QString::fromStdString(m.label), QString::fromStdString(m.id));
        modelB->addItem(QString::fromStdString(m.label), QString::fromStdString(m.id));
    }
    modelA->setCurrentIndex(std::max(0, modelA->findData(QString::fromStdString(idA))));
    modelB->setCurrentIndex(std::max(0, modelB->findData(QString::fromStdString(idB))));
    field = UiStandards::combo(body);
    for (const auto& v : E::variables()) {
        field->addItem(QString::fromStdString(v.label), QString::fromStdString(v.id));
    }
    field->setCurrentIndex(std::max(0, field->findData(QString::fromStdString(variable))));
    period = UiStandards::combo(body);
    for (const int d : {14, 30, 45}) {
        period->addItem(QString{"last %1 days"}.arg(d), d);
    }
    period->setCurrentIndex(std::max(0, period->findData(days)));
    lead = UiStandards::combo(body);
    for (int k = 1; k <= 7; k++) {
        lead->addItem(QString{"days chart: %1 day%2 ahead"}.arg(k).arg(k == 1 ? "" : "s"), k);
    }
    lead->setCurrentIndex(std::clamp(leadDays, 1, 7) - 1);
    go = UiStandards::button(body, "Load");
    lowerView = UiStandards::combo(body);
    lowerView->addItem("Lower chart: the error of each day", 0);
    lowerView->addItem("Lower chart: how each day's forecast changed", 1);
    judge = UiStandards::combo(body);
    judge->addItem("Judged on ERA5 (reanalysis, everywhere)", 0);
    judge->addItem("Judged on a station: daily highs (US)", 1);
    judge->addItem("Judged on a station: daily lows (US)", 2);
    judge->addItem("Judged on a station: daily rain (US)", 3);
    judge->setToolTip("What happened: ERA5, a reanalysis about five days behind, or the nearest station's daily highs, lows or rain (NOAA's Regional Climate Centers; no lag, the United States only; the variable box is then ignored)");
    status = new QLabel{"", body};
    status->setWordWrap(true);
    top->addWidget(place);
    top->addWidget(new QLabel{"lat", body});
    top->addWidget(latBox);
    top->addWidget(new QLabel{"lon", body});
    top->addWidget(lonBox);
    top->addWidget(modelA);
    top->addWidget(new QLabel{"and", body});
    top->addWidget(modelB);
    top->addWidget(field);
    top->addWidget(judge);
    top->addWidget(period);
    top->addWidget(lead);
    top->addWidget(lowerView);
    top->addWidget(go);
    column->addLayout(top);
    column->addWidget(status);
    chart = new BiasChart{body};
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
    for (auto * box : {modelA, modelB, field, judge, period, lead, lowerView}) {
        QObject::connect(box, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    }
    QObject::connect(go, &QPushButton::clicked, this, [this] { load(); });
    setSize(1100, 820);
    QWidget::show();
    load();
}

void ModelBiasViewer::load() {
    const int mine = ++generation;
    const double lat = latBox->value(), lon = lonBox->value();
    const auto idA = modelA->currentData().toString().toStdString(), idB = modelB->currentData().toString().toStdString();
    const auto * chosen = E::findVariable(field->currentData().toString().toStdString());
    if (chosen == nullptr) {
        return;
    }
    const int how = judge->currentData().toInt();
    auto variable = *chosen;
    if (how == 1 || how == 2) {   // a station's highs and lows: the temperature
        variable = *E::findVariable("temperature_2m");
    } else if (how == 3) {
        variable = *E::findVariable("precipitation");
    }
    const int days = period->currentData().toInt(), leadDays = lead->currentData().toInt();
    const auto nameA = modelA->currentText(), nameB = modelB->currentText();
    if (idA == idB) {
        status->setText("Choose two different models.");
        return;
    }
    go->setEnabled(false);
    status->setText("reading the past forecasts and what happened...");
    QPointer<ModelBiasViewer> self{this};
    const bool evolution = lowerView->currentData().toInt() == 1;
    std::thread{[self, mine, lat, lon, idA, idB, variable, days, leadDays, nameA, nameB, how, evolution] {
        ModelBias::Forecasts forecasts;
        ModelBias::Truth truth;
        std::map<std::string, float> stationDays;
        ModelBias::Station found;
        std::string why;
        const auto judging = how == 1 ? ModelBias::Judge::StationHigh : how == 2 ? ModelBias::Judge::StationLow : how == 3 ? ModelBias::Judge::StationRain : ModelBias::Judge::Era5;
        const bool ok = how == 0 ? ModelBias::fetch(lat, lon, variable.id, idA, idB, days, forecasts, truth, why) : ModelBias::fetchStation(lat, lon, judging, idA, idB, days, forecasts, stationDays, found, why);
        Result result;
        if (ok) {
            result.nameA = nameA;
            result.nameB = nameB;
            result.variable = variable;
            result.utcOffset = forecasts.utcOffset;
            result.days = days;
            result.lead = leadDays;
            for (int k = 1; k <= 7; k++) {
                if (how == 0) {
                    result.statA[k] = ModelBias::evaluate(forecasts.times, forecasts.utcOffset, forecasts.byModel[idA][static_cast<size_t>(k)], truth, variable.accumulate);
                    result.statB[k] = ModelBias::evaluate(forecasts.times, forecasts.utcOffset, forecasts.byModel[idB][static_cast<size_t>(k)], truth, variable.accumulate);
                } else {
                    result.statA[k] = ModelBias::evaluateDaily(forecasts.times, forecasts.utcOffset, forecasts.byModel[idA][static_cast<size_t>(k)], stationDays, judging);
                    result.statB[k] = ModelBias::evaluateDaily(forecasts.times, forecasts.utcOffset, forecasts.byModel[idB][static_cast<size_t>(k)], stationDays, judging);
                }
            }
            result.station = how != 0;
            result.showEvolution = evolution;
            const auto aggregate = how == 1 ? ModelBias::Aggregate::High : how == 2 ? ModelBias::Aggregate::Low : (how == 3 || variable.accumulate) ? ModelBias::Aggregate::Sum : ModelBias::Aggregate::Mean;
            for (int m = 0; m < 2; m++) {
                for (int k = 0; k <= 7; k++) {
                    result.evolution[m][k] = ModelBias::dailyOf(forecasts.times, forecasts.utcOffset, forecasts.byModel[m == 0 ? idA : idB][static_cast<size_t>(k)], aggregate);
                }
            }
            if (how == 0) {
                result.happened = ModelBias::dailyOf(truth.times, truth.utcOffset, truth.values, aggregate);
            } else {
                for (const auto& [date, value] : stationDays) {   // the station's days, put at the place's midnight of that date
                    const auto midnight = QDateTime{QDate::fromString(QString::fromStdString(date), "yyyy-MM-dd"), QTime{0, 0}, QTimeZone::fromSecondsAheadOfUtc(forecasts.utcOffset)};
                    result.happened[static_cast<double>(midnight.toSecsSinceEpoch())] = value;
                }
            }
            result.truthName = how == 0 ? QString{"ERA5"} : QString::fromStdString(found.name) + QString{" (%1 km)"}.arg(found.km, 0, 'f', 0);
            const QString what = how == 1 ? "daily highs" : how == 2 ? "daily lows" : how == 3 ? "daily rain" : QString::fromStdString(variable.title);
            result.title = QString{"%1 at %2, %3: %4 and %5 against %6"}.arg(what).arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2).arg(nameA.section(" (", 0, 0), nameB.section(" (", 0, 0), result.truthName);
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, mine, ok, why, result, variable] {
            if (!self || self->generation != mine) {
                return;
            }
            self->go->setEnabled(true);
            if (!ok) {
                self->status->setText("could not read: " + QString::fromStdString(why));
                self->chart->clear("No result: " + QString::fromStdString(why));
                return;
            }
            self->chart->set(result);
            // the sentence: the lead of the days chart, each model
            const auto& a = result.statA[result.lead];
            const auto& b = result.statB[result.lead];
            const auto sentence = [&] (const QString& name, const ModelBias::Stat& s) {
                if (s.n == 0) {
                    return name + ": no day to judge";
                }
                const double v = E::display(variable, static_cast<double>(s.bias), true);
                const QString unit = QString::fromStdString(E::unitLabel(variable)) + (variable.accumulate ? " per day" : "");
                return QString{"%1: %2 %3 %4 (misses by %5)"}.arg(name.section(" (", 0, 0), QString::number(std::fabs(v), 'f', variable.digits), unit,
                                                                  std::fabs(v) < std::pow(10.0, -variable.digits) ? "(no bias)" : v > 0 ? "too high" : "too low").arg(QString::number(E::display(variable, static_cast<double>(s.mae), true), 'f', variable.digits));
            };
            self->status->setText(QString{"At %1 day%2 ahead over the last %3 days, %4;  %5.  %6"}
                                      .arg(result.lead).arg(result.lead == 1 ? "" : "s").arg(result.days).arg(sentence(result.nameA, a), sentence(result.nameB, b), result.station ? "What happened is " + result.truthName + ": the days are the observers' own (usually midnight to midnight, local standard time)." : QString{"What happened is ERA5, about five days behind: the newest days are left out."}));
        }, Qt::QueuedConnection);
    }}.detach();
}

QPixmap ModelBiasViewer::picture() const {
    return chart->grab();
}

QString ModelBiasViewer::statusText() const {
    return status->text();
}
