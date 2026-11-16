// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/ModelCompareViewer.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <thread>
#include <QApplication>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimeZone>
#include <QVBoxLayout>
#include "gfs/ChartSelection.h"
#include "models/ModelViewer.h"
#include "settings/Location.h"
#include "ui/ChartExport.h"
#include "ui/FlowBox.h"
#include "ui/UiStandards.h"
#include "util/DisplayUnits.h"

namespace {
    const QColor colorA{30, 90, 200}, colorB{220, 120, 20};
}

std::string ModelCompareViewer::mapModel(const std::string& id) {
    static const std::vector<std::pair<std::string, std::string>> map{{"gfs025", "GEFS"}, {"ecmwf_ifs025", "IFS-ENS"}, {"ecmwf_aifs025", "AIFS"}, {"gem_global", "GEPS"}, {"icon_seamless", "ICON"}, {"ukmo_global_ensemble_20km", "UKMO"}};
    for (const auto& [openMeteo, program] : map) {
        if (openMeteo == id) {
            return program;
        }
    }
    return {};
}

class CompareChart : public QWidget {
public:
    explicit CompareChart(QWidget * parent) : QWidget{parent} {
        setMinimumSize(640, 460);
        setMouseTracking(true);
        ChartExport::install(this, "Model comparison");
    }
    void clear(const QString& text) {
        have = false;
        message = text;
        picked = -1;
        update();
    }
    // every member of each model on the common hours (the spaghetti)
    void setMembers(const std::vector<std::vector<float>>& ofA, const std::vector<std::vector<float>>& ofB) {
        membersA = ofA;
        membersB = ofB;
        update();
    }
    void showMembers(bool bands, bool members) {
        drawBands = bands;
        drawMembers = members;
        update();
    }
    void set(const QString& title, const QString& nameOfA, const QString& nameOfB, bool temperature, const EnsemblePercentiles::Compare& c) {
        titleText = title;
        a = nameOfA;
        b = nameOfB;
        isTemperature = temperature;
        data = c;
        have = !c.times.empty();
        picked = -1;
        update();
    }
    // the time picked (unix seconds), or 0
    double pickedTime() const { return picked >= 0 && picked < static_cast<int>(data.times.size()) ? data.times[static_cast<size_t>(picked)] : 0.0; }
    // the first hour of the largest difference of the day averages: where "Why?" looks when nothing is picked
    double biggestTime() const {
        double best = 0.0, at = 0.0;
        for (size_t i = 0; i < data.daily.size(); i++) {
            if (!std::isnan(data.daily[i]) && std::fabs(static_cast<double>(data.daily[i])) > best) {
                best = std::fabs(static_cast<double>(data.daily[i]));
                at = data.dayStart[i] + 12 * 3600.0;
            }
        }
        return at;
    }
    void pick(int index) { picked = index; update(); }
    std::function<void()> changed;   // the pick moved

protected:
    void mousePressEvent(QMouseEvent * event) override { choose(event->position().x(), true); }
    void mouseMoveEvent(QMouseEvent * event) override { choose(event->position().x(), false); }

    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing, true);
        p.fillRect(rect(), Qt::white);
        p.setPen(Qt::black);
        const QFont base = p.font();
        if (!have) {
            p.drawText(rect(), Qt::AlignCenter, message.isEmpty() ? "Choose a place, two models and a field, then Load." : message);
            return;
        }
        QFont bold = base;
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{10, 4, width() - 20.0, 22}, Qt::AlignLeft | Qt::AlignVCenter, titleText);
        p.setFont(base);
        left = 62;
        const double right = 34, top = 90, gap = 40, bottom = 44;
        plotWidth = width() - left - right;
        const double room = height() - top - bottom - gap;
        const QRectF upper{left, top, plotWidth, room * 0.55}, lower{left, upper.bottom() + gap, plotWidth, room * 0.45};
        t0 = data.times.front();
        t1 = data.times.back();
        for (size_t i = data.times.size(); i-- > 0;) {   // the axis ends at the last hour both models have
            if (!std::isnan(data.difference[i])) {
                t1 = data.times[i];
                break;
            }
        }
        const std::string unit = isTemperature ? "deg C" : "mm";
        const auto show = [&] (float v) { return DisplayUnits::value(static_cast<double>(v), unit); };
        const auto delta = [&] (float v) { return DisplayUnits::value(static_cast<double>(v), unit, DisplayUnits::Kind::Delta); };
        // the key, under the title
        double kx = 14;
        const auto key = [&] (const QColor& color, const QString& text, bool filled) {
            if (filled) {
                QColor c = color;
                c.setAlpha(90);
                p.fillRect(QRectF{kx, 33, 16, 10}, c);
            } else {
                p.setPen(QPen{color, 2.4});
                p.drawLine(QPointF{kx, 38}, QPointF{kx + 16, 38});
            }
            p.setPen(Qt::black);
            p.drawText(QPointF{kx + 22, 43}, text);
            kx += 22 + QFontMetricsF{p.font()}.horizontalAdvance(text) + 18;
        };
        key(colorA, a + " median", false);
        key(colorA, drawBands ? a + " 25 to 75 percent" : a + " members", drawBands);
        key(colorB, b + " median", false);
        key(colorB, drawBands ? b + " 25 to 75 percent" : b + " members", drawBands);
        // upper panel: both
        double lo = 1e18, hi = -1e18;
        for (size_t i = 0; i < data.times.size(); i++) {
            for (const auto* v : {&data.aLow, &data.aHigh, &data.bLow, &data.bHigh}) {
                if (!std::isnan((*v)[i])) {
                    lo = std::min(lo, show((*v)[i]));
                    hi = std::max(hi, show((*v)[i]));
                }
            }
        }
        if (drawMembers) {
            for (const auto* list : {&membersA, &membersB}) {
                for (const auto& member : *list) {
                    for (const float v : member) {
                        if (!std::isnan(v)) {
                            lo = std::min(lo, show(v));
                            hi = std::max(hi, show(v));
                        }
                    }
                }
            }
        }
        if (lo > hi) {
            p.drawText(rect(), Qt::AlignCenter, "no values in common");
            return;
        }
        axes(p, upper, lo, hi, !isTemperature, QString::fromStdString((isTemperature ? "2 m temperature (" : "precipitation added up since the first hour (") + DisplayUnits::label(unit) + ")"), yTop);
        const auto yOfTop = [&] (double v) { return upper.bottom() - (v - yTop.low) / (yTop.high - yTop.low) * upper.height(); };
        p.save();
        p.setClipRect(upper);
        if (drawBands) {
            band(p, data.aLow, data.aHigh, show, yOfTop, colorA);
            band(p, data.bLow, data.bHigh, show, yOfTop, colorB);
        }
        if (drawMembers) {   // the spaghetti: every member, thin and see-through, the models in their own colours
            for (const auto& [list, color] : {std::pair{&membersA, colorA}, std::pair{&membersB, colorB}}) {
                QColor thin = color;
                thin.setAlpha(drawBands ? 70 : 95);
                for (const auto& member : *list) {
                    if (member.size() == data.times.size()) {
                        curve(p, member, show, yOfTop, QPen{thin, 0.9});
                    }
                }
            }
        }
        curve(p, data.aMedian, show, yOfTop, QPen{colorA, 2.4});
        curve(p, data.bMedian, show, yOfTop, QPen{colorB, 2.4});
        p.restore();
        // lower panel: the difference
        double dmax = 0.5;
        for (size_t i = 0; i < data.difference.size(); i++) {
            if (!std::isnan(data.difference[i])) {
                dmax = std::max(dmax, std::fabs(delta(data.difference[i])));
            }
        }
        const double span = std::ceil(dmax);
        axes(p, lower, -span, span, false, QString::fromStdString(a.toStdString() + " minus " + b.toStdString() + ": the difference of the medians (" + DisplayUnits::label(unit) + "), and its average for each day"), yLow, true);
        const auto yOfLow = [&] (double v) { return lower.bottom() - (v - yLow.low) / (yLow.high - yLow.low) * lower.height(); };
        p.save();
        p.setClipRect(lower);
        const double zero = yOfLow(0.0);
        p.setPen(QPen{QColor{90, 90, 90}, 1.0});
        p.drawLine(QPointF{lower.left(), zero}, QPointF{lower.right(), zero});
        for (size_t d = 0; d < data.daily.size(); d++) {   // the day's mean: a bar the width of the day, warm colours when the first is higher
            const double x0 = xOf(data.dayStart[d]), x1 = xOf(data.dayStart[d] + 86400.0);
            const double v = delta(data.daily[d]);
            p.setPen(Qt::NoPen);
            p.setBrush(v >= 0 ? QColor{210, 80, 50, 90} : QColor{50, 100, 210, 90});
            p.drawRect(QRectF{x0 + 1, std::min(zero, yOfLow(v)), x1 - x0 - 2, std::fabs(yOfLow(v) - zero)});
        }
        p.setBrush(Qt::NoBrush);
        {
            QPainterPath path;
            bool down = false;
            for (size_t i = 0; i < data.times.size(); i++) {
                if (std::isnan(data.difference[i])) {
                    down = false;
                    continue;
                }
                const QPointF pt{xOf(data.times[i]), yOfLow(delta(data.difference[i]))};
                down ? path.lineTo(pt) : path.moveTo(pt);
                down = true;
            }
            p.setPen(QPen{QColor{40, 40, 40}, 1.6});
            p.drawPath(path);
        }
        p.restore();
        // the averages in words, on the key's second line
        QStringList words;
        for (const double days : {3.0, 5.0, 10.0}) {
            const auto m = EnsemblePercentiles::meanDifference(data, days);
            if (!std::isnan(m) && (days == 3.0 || data.times.back() - data.times.front() >= days * 86400.0 - 3600)) {
                words << QString{"%1 days: %2%3"}.arg(static_cast<int>(days)).arg(delta(m) >= 0 ? "+" : "").arg(delta(m), 0, 'f', isTemperature ? 1 : 2);
            }
        }
        p.setPen(Qt::black);
        p.drawText(QPointF{14, 60}, "average difference over the first " + words.join(",  ") + "   (click a time, then Why?)");
        // the day names
        QDateTime day = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t0), QTimeZone::utc());
        day.setTime(QTime{0, 0});
        if (day.toSecsSinceEpoch() < static_cast<qint64>(t0)) {
            day = day.addDays(1);
        }
        const int every = (t1 - t0) > 9 * 86400 ? 2 : 1;
        for (int n = 0; day.toSecsSinceEpoch() <= static_cast<qint64>(t1); day = day.addDays(1), n++) {
            const double x = xOf(static_cast<double>(day.toSecsSinceEpoch()));
            p.setPen(QPen{QColor{215, 215, 215}, 1.0});
            p.drawLine(QPointF{x, upper.top()}, QPointF{x, lower.bottom()});
            if (n % every == 0) {
                p.setPen(Qt::black);
                p.drawText(QRectF{x - 30, lower.bottom() + 2, 60, 14}, Qt::AlignCenter, day.toString("ddd d"));
            }
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{left, lower.bottom() + 20, plotWidth, 16}, Qt::AlignRight | Qt::AlignVCenter, "times are UTC (day lines at 00 UTC)");
        if (picked >= 0 && picked < static_cast<int>(data.times.size())) {
            const auto i = static_cast<size_t>(picked);
            const double x = xOf(data.times[i]);
            p.setPen(QPen{QColor{20, 20, 20}, 1.2});
            p.drawLine(QPointF{x, upper.top()}, QPointF{x, lower.bottom()});
            const auto when = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(data.times[i]), QTimeZone::utc()).toString("ddd d MMM HH:mm 'UTC'");
            QString text = when;
            if (!std::isnan(data.aMedian[i]) && !std::isnan(data.bMedian[i])) {
                text += QString{":  %1 %2, %3 %4, difference %5%6"}.arg(a).arg(show(data.aMedian[i]), 0, 'f', 1).arg(b).arg(show(data.bMedian[i]), 0, 'f', 1).arg(delta(data.difference[i]) >= 0 ? "+" : "").arg(delta(data.difference[i]), 0, 'f', 1);
            }
            QFontMetricsF fm{p.font()};
            const double w = fm.horizontalAdvance(text) + 12;
            const QRectF bubble{std::min(x + 8, width() - w - 4), upper.top() + 4, w, fm.height() + 6};
            p.setBrush(QColor{255, 255, 235, 235});
            p.setPen(QPen{QColor{120, 120, 120}, 1.0});
            p.drawRect(bubble);
            p.setPen(Qt::black);
            p.drawText(bubble.adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
        }
    }

private:
    struct Axis {
        double low{0}, high{1};
    };
    double xOf(double t) const { return left + (t - t0) / std::max(1.0, t1 - t0) * plotWidth; }

    void choose(double x, bool click) {
        if (!have || plotWidth <= 0 || data.times.empty() || (!click && !(QApplication::mouseButtons() & Qt::LeftButton))) {
            return;
        }
        const double t = t0 + (x - left) / plotWidth * (t1 - t0);
        int best = -1;
        double nearest = 1e18;
        for (size_t i = 0; i < data.times.size(); i++) {
            const double d = std::fabs(data.times[i] - t);
            if (d < nearest) {
                nearest = d;
                best = static_cast<int>(i);
            }
        }
        picked = x >= left && x <= left + plotWidth ? best : -1;
        update();
        if (changed) {
            changed();
        }
    }

    void axes(QPainter& p, const QRectF& box, double lo, double hi, bool fromZero, const QString& title, Axis& out, bool symmetric = false) {
        if (fromZero) {
            lo = 0.0;
        }
        const double rough = std::max(hi - lo, 1e-6) / 5.0, mag = std::pow(10.0, std::floor(std::log10(rough)));
        double step = mag;
        for (const double m : {1.0, 2.0, 2.5, 5.0, 10.0}) {
            if (mag * m >= rough) {
                step = mag * m;
                break;
            }
        }
        if (symmetric) {   // zero is a tick: from -k steps to +k steps
            const double wide = std::max(std::fabs(lo), std::fabs(hi)), s2 = std::max(1e-6, wide * 2.0 / 5.0), m2 = std::pow(10.0, std::floor(std::log10(s2)));
            step = m2;
            for (const double m : {1.0, 2.0, 2.5, 5.0, 10.0}) {
                if (m2 * m >= s2) {
                    step = m2 * m;
                    break;
                }
            }
            const double k = std::max(1.0, std::ceil(wide / step));
            lo = -k * step;
            hi = k * step;
        }
        out.low = symmetric ? lo : (fromZero ? 0.0 : std::floor(lo / step) * step);
        out.high = symmetric ? hi : std::max(std::ceil(hi / step) * step, out.low + step);
        p.setPen(QPen{QColor{150, 150, 150}, 1.0});
        p.drawRect(box);
        for (double v = out.low; v <= out.high + step * 0.01; v += step) {
            const double y = box.bottom() - (v - out.low) / (out.high - out.low) * box.height();
            p.setPen(QPen{QColor{230, 230, 230}, 1.0});
            p.drawLine(QPointF{box.left(), y}, QPointF{box.right(), y});
            p.setPen(Qt::black);
            p.drawText(QRectF{box.left() - 58, y - 8, 52, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', step < 1.0 ? 2 : (std::fabs(step - std::round(step)) > 1e-6 ? 1 : 0)));
        }
        p.setPen(Qt::black);
        p.drawText(QRectF{box.left(), box.top() - 18, box.width(), 16}, Qt::AlignLeft | Qt::AlignVCenter, title);
    }
    template <typename S, typename Y>
    void band(QPainter& p, const std::vector<float>& lo, const std::vector<float>& hi, const S& show, const Y& yOf, const QColor& color) {
        QColor fill = color;
        fill.setAlpha(70);
        QPolygonF upper, lower;
        const auto flush = [&] {
            if (upper.size() >= 2) {
                std::reverse(lower.begin(), lower.end());
                p.setPen(Qt::NoPen);
                p.setBrush(fill);
                p.drawPolygon(upper + lower);
            }
            upper.clear();
            lower.clear();
        };
        for (size_t i = 0; i < data.times.size(); i++) {
            if (std::isnan(lo[i]) || std::isnan(hi[i])) {
                flush();
                continue;
            }
            upper << QPointF{xOf(data.times[i]), yOf(show(hi[i]))};
            lower << QPointF{xOf(data.times[i]), yOf(show(lo[i]))};
        }
        flush();
        p.setBrush(Qt::NoBrush);
    }
    template <typename S, typename Y>
    void curve(QPainter& p, const std::vector<float>& values, const S& show, const Y& yOf, const QPen& pen) {
        QPainterPath path;
        bool down = false;
        for (size_t i = 0; i < data.times.size(); i++) {
            if (std::isnan(values[i])) {
                down = false;
                continue;
            }
            const QPointF pt{xOf(data.times[i]), yOf(show(values[i]))};
            down ? path.lineTo(pt) : path.moveTo(pt);
            down = true;
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    }

    std::vector<std::vector<float>> membersA, membersB;
    bool drawBands{true}, drawMembers{false};
    bool have{false}, isTemperature{true};
    QString message, titleText, a, b;
    EnsemblePercentiles::Compare data;
    Axis yTop, yLow;
    double left{62}, plotWidth{0}, t0{0}, t1{1};
    int picked{-1};
};

ModelCompareViewer::ModelCompareViewer(Window * parent, double lat, double lon, const std::string& idA, const std::string& idB, const std::string& fieldId) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Two ensembles compared at a place");
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
    for (const auto& m : EnsemblePercentiles::models()) {
        modelA->addItem(QString::fromStdString(m.label), QString::fromStdString(m.id));
        modelB->addItem(QString::fromStdString(m.label), QString::fromStdString(m.id));
    }
    modelA->setCurrentIndex(std::max(0, modelA->findData(QString::fromStdString(idA))));
    modelB->setCurrentIndex(std::max(0, modelB->findData(QString::fromStdString(idB))));
    field = UiStandards::combo(body);
    field->addItem("2 m temperature", "temperature");
    field->addItem("Precipitation added up", "precipitation");
    field->setCurrentIndex(std::max(0, field->findData(QString::fromStdString(fieldId))));
    spread = UiStandards::combo(body);
    spread->addItem("Spread: 25 to 75 percent bands", 0);
    spread->addItem("Spread: every member (spaghetti)", 1);
    spread->addItem("Spread: bands and every member", 2);
    spread->setToolTip("How each model's members are drawn on the upper chart");
    mapChoice = UiStandards::combo(body);
    mapChoice->addItem("500 mb heights and winds", "500_wnd_ht");
    mapChoice->addItem("250 mb jet stream", "250_wnd_ht");
    mapChoice->setToolTip("What Why? shows on the map: this model's chart with the other model's height lines over it");
    go = UiStandards::button(body, "Load");
    whyButton = UiStandards::button(body, "Why? Show the maps");
    whyButton->setToolTip("Opens the Model Viewer on the time picked (or on the day with the largest difference): the first model's chart with the second model's height lines over it");
    status = new QLabel{"", body};
    top->addWidget(place);
    top->addWidget(new QLabel{"lat", body});
    top->addWidget(latBox);
    top->addWidget(new QLabel{"lon", body});
    top->addWidget(lonBox);
    top->addWidget(modelA);
    top->addWidget(new QLabel{"minus", body});
    top->addWidget(modelB);
    top->addWidget(field);
    top->addWidget(spread);
    top->addWidget(go);
    top->addWidget(mapChoice);
    top->addWidget(whyButton);
    column->addLayout(top);
    column->addWidget(status);
    chart = new CompareChart{body};
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
    QObject::connect(modelA, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    QObject::connect(modelB, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    QObject::connect(field, &QComboBox::currentIndexChanged, this, [this] (int) { load(); });
    QObject::connect(spread, &QComboBox::currentIndexChanged, this, [this] (int index) { chart->showMembers(index != 1, index != 0); });
    QObject::connect(go, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(whyButton, &QPushButton::clicked, this, [this] { why(); });
    setSize(1100, 800);
    QWidget::show();
    load();
}

void ModelCompareViewer::load() {
    const int mine = ++generation;
    const double lat = latBox->value(), lon = lonBox->value();
    const auto idA = modelA->currentData().toString().toStdString(), idB = modelB->currentData().toString().toStdString();
    const auto nameA = modelA->currentText().section(" (", 0, 0), nameB = modelB->currentText().section(" (", 0, 0);
    const bool temperature = field->currentData().toString() == "temperature";
    go->setEnabled(false);
    status->setText("reading the members of both models...");
    QPointer<ModelCompareViewer> self{this};
    std::thread{[self, mine, lat, lon, idA, idB, nameA, nameB, temperature] {
        const auto read = [&] (const std::string& id, EnsemblePercentiles::Fan& fan, EnsemblePercentiles::Series& members, std::string& why) {
            EnsemblePercentiles::Series t, r;
            if (!EnsemblePercentiles::fetch(lat, lon, id, t, r, why)) {
                return false;
            }
            members = temperature ? t : EnsemblePercentiles::accumulated(r);
            fan = EnsemblePercentiles::fan(members);
            return true;
        };
        EnsemblePercentiles::Fan fa, fb;
        EnsemblePercentiles::Series sa, sb;
        std::string whyA, whyB;
        auto second = std::async(std::launch::async, [&] { return read(idB, fb, sb, whyB); });
        const bool okA = read(idA, fa, sa, whyA);
        const bool okB = second.get();
        EnsemblePercentiles::Compare compared;
        if (okA && okB) {
            compared = EnsemblePercentiles::compare(fa, fb);
        }
        std::vector<std::vector<float>> linesA, linesB;
        if (!compared.times.empty()) {
            linesA = EnsemblePercentiles::onTimes(sa, compared.times);
            linesB = EnsemblePercentiles::onTimes(sb, compared.times);
        }
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, mine, okA, okB, whyA, whyB, compared, linesA, linesB, lat, lon, nameA, nameB, temperature] {
            if (!self || self->generation != mine) {
                return;
            }
            self->go->setEnabled(true);
            if (!okA || !okB || compared.times.empty()) {
                const auto why = !okA ? QString::fromStdString(whyA) : !okB ? QString::fromStdString(whyB) : QString{"the two models have no hours in common"};
                self->status->setText("could not compare: " + why);
                self->chart->clear("No comparison: " + why);
                return;
            }
            const auto first = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(compared.times.front()), QTimeZone::utc());
            self->status->setText(QString{"%1 hours in common, from %2 UTC (members by Open-Meteo)"}.arg(compared.times.size()).arg(first.toString("ddd d MMM HH:mm")));
            self->chart->setMembers(linesA, linesB);
            self->chart->showMembers(self->spread->currentData().toInt() != 1, self->spread->currentData().toInt() != 0);
            self->chart->set(QString{"%1 minus %2 at %3, %4"}.arg(nameA, nameB).arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2), nameA, nameB, temperature, compared);
        }, Qt::QueuedConnection);
    }}.detach();
}

ModelViewer * ModelCompareViewer::why() {
    double t = chart->pickedTime();
    if (t == 0.0) {
        t = chart->biggestTime();
    }
    if (t == 0.0) {
        status->setText("Load a comparison first.");
        return nullptr;
    }
    const auto mainModel = mapModel(modelA->currentData().toString().toStdString());
    const auto otherModel = mapModel(modelB->currentData().toString().toStdString());
    if (mainModel.empty() || otherModel.empty()) {
        status->setText("The program has no map of one of these models.");
        return nullptr;
    }
    // the map time: the six hours at or before the time picked
    auto valid = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(t), QTimeZone::utc());
    valid.setTime(QTime{valid.time().hour() / 6 * 6, 0});
    // the chart: the one asked for, then the plain 500 mb chart, then the ensemble mean and spread of the 500 mb height (the IFS ensemble has no wind chart); on the first model that has one, the other's
    // height lines over it (so when the first has none of these, the second is drawn and the first is the overlay)
    const auto asked = mapChoice->currentData().toString().toStdString();
    std::vector<std::string> wanted{asked, "500_wnd_ht", "spread_500_ht"};
    ChartSelection selection;
    selection.valid = valid;
    bool found = false;
    std::string shown, over, note;
    for (const auto& id : wanted) {   // (the chart asked for on either model before a fallback chart on the first)
        for (const auto& [first, second] : {std::pair{mainModel, otherModel}, std::pair{otherModel, mainModel}}) {
            ChartSelection trial;
            trial.model = first;
            trial.param = id;
            if (trial.usable()) {
                selection = trial;
                // the IFS ensemble has no height chart to take lines from: the Euro's own high resolution run stands for it
                const auto lines = second == "IFS-ENS" ? std::string{"IFS"} : second;
                selection.overlayModels = {lines};
                if (lines != second) {
                    note = " (" + lines + ", the ECMWF high resolution run, stands for the ensemble: it has no height chart)";
                }
                selection.valid = valid;
                shown = first;
                over = second;
                found = true;
                break;
            }
        }
        if (found) {
            break;
        }
    }
    if (!found) {
        status->setText(QString::fromStdString("Neither " + mainModel + " nor " + otherModel + " has a 500 mb or 250 mb chart."));
        return nullptr;
    }
    auto * viewer = new ModelViewer{this, "NCEP"};   // (the model screen; the model is chosen by the selection)
    viewer->applySelection(selection);
    status->setText(QString::fromStdString(shown + " " + selection.param + " for " + valid.toString("ddd d MMM HH 'UTC'").toStdString() + ", with " + over + "'s height lines over it." + note));
    return viewer;
}

QPixmap ModelCompareViewer::picture() const {
    return chart->grab();
}

QString ModelCompareViewer::statusText() const {
    return status->text();
}
