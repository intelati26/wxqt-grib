// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/CfsPlumeViewer.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "ui/ChartExport.h"
#include "util/Utility.h"

// the chart: the daily lines on top, the monthly means below
class CfsPlume : public QWidget {
public:
    explicit CfsPlume(QWidget * parent) : QWidget{parent} {
        setMinimumSize(760, 520);
        ChartExport::install(this, "CFSv2 plume");
    }
    void set(const CfsPlumeViewer::Result& r) {
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
        QFont base = p.font();
        if (!have) {
            p.drawText(rect(), Qt::AlignCenter, "Choose a place and a field, then Load.");
            return;
        }
        QFont bold = base;
        bold.setBold(true);
        p.setFont(bold);
        p.drawText(QRectF{10, 4, width() - 20.0, 22}, Qt::AlignLeft | Qt::AlignVCenter, result.title);
        p.setFont(base);
        const double left = 62, right = 190, top = 34, gap = 46, bottomMargin = 34;
        const double plotWidth = width() - left - right;
        const double dailyHeight = (height() - top - bottomMargin - gap) * 0.62;
        const double monthlyHeight = (height() - top - bottomMargin - gap) - dailyHeight;
        drawDaily(p, QRectF{left, top, plotWidth, dailyHeight});
        drawMonthly(p, QRectF{left, top + dailyHeight + gap, plotWidth, monthlyHeight});
        // the key
        double y = top + 6;
        const auto key = [&] (const QColor& color, const QString& text, double widthOf) {
            p.setPen(QPen{color, widthOf});
            p.drawLine(QPointF{width() - right + 14, y + 6}, QPointF{width() - right + 38, y + 6});
            p.setPen(Qt::black);
            p.drawText(QPointF{width() - right + 44, y + 11}, text);
            y += 18;
        };
        for (int m = 1; m <= 4 && static_cast<size_t>(m) <= result.runNames.size(); m++) {
            key(memberColor(m), result.runNames[static_cast<size_t>(m) - 1], 1.4);
        }
        key(Qt::black, "mean of the runs", 2.6);
        key(QColor{150, 150, 150}, "same cycle, earlier days", 1.0);
    }

private:
    static QColor memberColor(int m) {
        static const QColor c[] = {QColor{40, 110, 210}, QColor{235, 130, 30}, QColor{40, 160, 80}, QColor{150, 60, 190}};
        return c[(m - 1) & 3];
    }
    static void niceRange(double lo, double hi, double& from, double& to, double& step) {
        if (!(hi > lo)) {
            hi = lo + 1.0;
        }
        const double raw = (hi - lo) / 5.0;
        const double magnitude = std::pow(10.0, std::floor(std::log10(raw)));
        const double n = raw / magnitude;
        step = (n < 1.5 ? 1.0 : n < 3.5 ? 2.0 : n < 7.5 ? 5.0 : 10.0) * magnitude;
        from = std::floor(lo / step) * step;
        to = std::ceil(hi / step) * step;
    }
    void axes(QPainter& p, const QRectF& area, double from, double to, double step, const QString& yLabel) const {
        p.setPen(QColor{220, 220, 220});
        for (double v = from; v <= to + step * 0.01; v += step) {
            const double y = area.bottom() - (v - from) / (to - from) * area.height();
            p.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
            p.setPen(Qt::black);
            p.drawText(QRectF{area.left() - 56, y - 9, 50, 18}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', step < 1.0 ? 1 : 0));
            p.setPen(QColor{220, 220, 220});
        }
        p.setPen(Qt::black);
        p.drawRect(area);
        p.save();
        p.translate(14, area.center().y());
        p.rotate(-90);
        p.drawText(QRectF{-area.height() / 2, -10, area.height(), 20}, Qt::AlignCenter, yLabel);
        p.restore();
    }
    void drawDaily(QPainter& p, const QRectF& area) const {
        double lo = 1e30, hi = -1e30, maxDay = 1;
        for (const auto& line : result.daily) {
            for (size_t i = 0; i < line.values.size(); i++) {
                if (!std::isnan(line.values[i])) {
                    lo = std::min<double>(lo, line.values[i]);
                    hi = std::max<double>(hi, line.values[i]);
                }
            }
        }
        for (const double d : result.days) {
            maxDay = std::max(maxDay, d);
        }
        if (lo > hi) {
            p.drawText(area, Qt::AlignCenter, "no values could be read");
            return;
        }
        double from, to, step;
        niceRange(lo, hi, from, to, step);
        axes(p, area, from, to, step, result.units);
        const double minDay = std::min(0.0, -std::max_element(result.daily.begin(), result.daily.end(), [] (const auto& a, const auto& b) { return a.runAgo < b.runAgo; })->runAgo);
        const auto xOf = [&] (double day) { return area.left() + (day - minDay) / (maxDay - minDay) * area.width(); };
        const auto yOf = [&] (double v) { return area.bottom() - (v - from) / (to - from) * area.height(); };
        // the day marks
        const double dayStep = maxDay > 60 ? 10 : maxDay > 20 ? 5 : 1;
        for (double d = std::ceil(minDay / dayStep) * dayStep; d <= maxDay; d += dayStep) {
            p.setPen(QColor{235, 235, 235});
            p.drawLine(QPointF{xOf(d), area.top()}, QPointF{xOf(d), area.bottom()});
            p.setPen(Qt::black);
            p.drawText(QRectF{xOf(d) - 30, area.bottom() + 2, 60, 16}, Qt::AlignCenter, result.runDay.addDays(static_cast<qint64>(d)).toString("d MMM"));
        }
        auto order = result.daily;
        std::stable_sort(order.begin(), order.end(), [] (const CfsPlumeViewer::Line& a, const CfsPlumeViewer::Line& b) { return (a.earlier ? 0 : a.member == 0 ? 2 : 1) < (b.earlier ? 0 : b.member == 0 ? 2 : 1); });
        for (const auto& line : order) {
            QPainterPath path;
            bool started = false;
            for (size_t i = 0; i < line.values.size() && i < result.days.size(); i++) {
                if (std::isnan(line.values[i])) {
                    started = false;
                    continue;
                }
                const QPointF pt{xOf(result.days[i] - line.runAgo), yOf(line.values[i])};
                if (!started) {
                    path.moveTo(pt);
                    started = true;
                } else {
                    path.lineTo(pt);
                }
            }
            p.setPen(QPen{line.earlier ? QColor{150, 150, 150} : line.member == 0 ? QColor{Qt::black} : memberColor(line.member), line.earlier ? 1.0 : line.member == 0 ? 2.6 : 1.3});
            p.drawPath(path);
        }
    }
    void drawMonthly(QPainter& p, const QRectF& area) const {
        double lo = 1e30, hi = -1e30;
        for (const auto& line : result.monthly) {
            for (const float v : line.values) {
                if (!std::isnan(v)) {
                    lo = std::min<double>(lo, v);
                    hi = std::max<double>(hi, v);
                }
            }
        }
        if (lo > hi) {
            p.drawText(area, Qt::AlignCenter, "no monthly means could be read");
            return;
        }
        double from, to, step;
        niceRange(lo, hi, from, to, step);
        axes(p, area, from, to, step, result.units + "  (month mean)");
        const size_t n = result.months.size();
        const auto xOf = [&] (size_t i) { return area.left() + (i + 0.5) / n * area.width(); };
        const auto yOf = [&] (double v) { return area.bottom() - (v - from) / (to - from) * area.height(); };
        for (size_t i = 0; i < n; i++) {
            p.setPen(Qt::black);
            p.drawText(QRectF{xOf(i) - 30, area.bottom() + 2, 60, 16}, Qt::AlignCenter, result.months[i]);
        }
        for (const auto& line : result.monthly) {
            QPainterPath path;
            bool started = false;
            for (size_t i = 0; i < line.values.size(); i++) {
                if (std::isnan(line.values[i])) {
                    continue;
                }
                const QPointF pt{xOf(i), yOf(line.values[i])};
                if (line.member == 0) {
                    started ? path.lineTo(pt) : path.moveTo(pt);
                    started = true;
                } else {
                    p.setPen(Qt::NoPen);
                    p.setBrush(memberColor(line.member));
                    p.drawEllipse(pt, 4.0, 4.0);
                }
            }
            if (line.member == 0) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen{Qt::black, 2.4});
                p.drawPath(path);
            }
        }
    }
    CfsPlumeViewer::Result result;
    bool have{false};
};

CfsPlumeViewer::CfsPlumeViewer(Window * parent, double lat, double lon) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("CFSv2 forecast at a place");
    auto * column = new QVBoxLayout;
    auto * top = new QHBoxLayout;
    place = new QComboBox{this};
    place->addItem("Latitude / longitude");
    const auto names = Location::listOfNames();
    const auto points = Location::getListLatLons();
    for (size_t i = 0; i < names.size() && i < points.size(); i++) {
        place->addItem(QString::fromStdString(names[i]), QPointF{points[i].lon(), points[i].lat()});
    }
    latBox = new QDoubleSpinBox{this};
    latBox->setRange(-90.0, 90.0);
    latBox->setDecimals(2);
    latBox->setValue(lat);
    lonBox = new QDoubleSpinBox{this};
    lonBox->setRange(-180.0, 180.0);
    lonBox->setDecimals(2);
    lonBox->setValue(lon);
    field = new QComboBox{this};
    for (const auto& v : UtilityCfs::variables()) {
        field->addItem(QString::fromStdString(v.label), QString::fromStdString(v.key));
    }
    span = new QComboBox{this};
    span->addItems({"15 days, 6 hourly", "45 days, daily", "90 days, every other day"});
    span->setCurrentIndex(1);
    earlier = new QComboBox{this};
    earlier->addItems({"The 4 newest runs", "and the same cycle of the 3 days before"});
    go = new QPushButton{"Load", this};
    status = new QLabel{"", this};
    top->addWidget(place);
    top->addWidget(new QLabel{"lat", this});
    top->addWidget(latBox);
    top->addWidget(new QLabel{"lon", this});
    top->addWidget(lonBox);
    top->addWidget(field);
    top->addWidget(span);
    top->addWidget(earlier);
    top->addWidget(go);
    top->addStretch();
    column->addLayout(top);
    column->addWidget(status);
    chart = new CfsPlume{this};
    column->addWidget(chart, 1);
    auto * central = new QWidget{this};
    central->setLayout(column);
    setCentralWidget(central);
    setSize(1100, 760);
    QWidget::show();
    QObject::connect(place, &QComboBox::currentIndexChanged, this, [this] (int index) {
        if (index > 0) {
            const auto point = place->itemData(index).toPointF();
            lonBox->setValue(point.x());
            latBox->setValue(point.y());
        }
    });
    QObject::connect(go, &QPushButton::clicked, this, [this] { load(); });
    progress.setInterval(300);
    QObject::connect(&progress, &QTimer::timeout, this, [this] {
        if (loading) {
            status->setText(QString{"reading the CFSv2 run: %1 of %2 parts"}.arg(done->load()).arg(total->load()));
        }
    });
    load();
}

void CfsPlumeViewer::load() {
    if (loading) {
        return;
    }
    loading = true;
    go->setEnabled(false);
    const double lat = latBox->value(), lon = lonBox->value();
    const auto variable = *UtilityCfs::find(field->currentData().toString().toStdString());
    std::vector<int> hours;
    switch (span->currentIndex()) {
        case 0: for (int h = 6; h <= 360; h += 6) hours.push_back(h); break;
        case 2: for (int h = 24; h <= 2160; h += 48) hours.push_back(h); break;
        default: for (int h = 24; h <= 1080; h += 24) hours.push_back(h); break;
    }
    const int earlierDays = earlier->currentIndex() == 1 ? 3 : 0;
    auto result = std::make_shared<Result>();
    auto error = std::make_shared<std::string>();
    done->store(0);
    total->store(4 + earlierDays + 4);
    progress.start();
    status->setText("looking for the newest run...");
    const auto doneCount = done;
    new FutureVoid{this,
        [=] {
            UtilityCfs::Run run;
            if (!UtilityCfs::latestRun(run)) {
                *error = "no CFSv2 run found";
                return;
            }
            const auto runTime = QDateTime::fromString(QString::fromStdString(run.date + run.cycle), "yyyyMMddHH");
            result->title = QString::fromStdString(variable.label + " at " + std::to_string(lat).substr(0, std::to_string(lat).find('.') + 3) + ", " + std::to_string(lon).substr(0, std::to_string(lon).find('.') + 3) + "   CFSv2 run " + UtilityCfs::runText(run));
            result->units = QString::fromStdString(variable.units);
            result->runDay = runTime.date();
            for (const int h : hours) {
                result->days.push_back(h / 24.0);
            }
            const auto runs = UtilityCfs::recentRuns(run, 4);
            for (size_t k = 0; k < runs.size(); k++) {
                const auto t = QDateTime::fromString(QString::fromStdString(runs[k].date + runs[k].cycle), "yyyyMMddHH");
                result->runNames.push_back(QString::fromStdString(runs[k].cycle) + "Z " + t.toString("d MMM"));
            }
            struct Job {
                int member;
                double ago;
                bool earlier;
                std::future<std::vector<float>> values;
            };
            std::vector<Job> jobs;
            for (size_t k = 0; k < runs.size(); k++) {
                const auto past = runs[k];
                jobs.push_back({static_cast<int>(k) + 1, static_cast<double>(k) * 0.25, false, std::async(std::launch::async, [=] {
                    std::vector<float> v;
                    std::string message;
                    UtilityCfs::pointSeries(past, 1, variable, hours, lat, lon, v, message);
                    doneCount->fetch_add(1);
                    return v;
                })});
            }
            for (int ago = 1; ago <= earlierDays; ago++) {
                const auto t = runTime.addDays(-ago);
                const UtilityCfs::Run past{t.toString("yyyyMMdd").toStdString(), run.cycle};
                jobs.push_back({0, static_cast<double>(ago), true, std::async(std::launch::async, [=] {
                    std::vector<float> v;
                    std::string message;
                    UtilityCfs::pointSeries(past, 1, variable, hours, lat, lon, v, message);
                    doneCount->fetch_add(1);
                    return v;
                })});
            }
            for (auto& job : jobs) {
                Line line;
                line.member = job.member;
                line.runAgo = job.ago;
                line.earlier = job.earlier;
                line.values = job.values.get();
                result->daily.push_back(std::move(line));
            }
            Line mean;
            mean.name = "mean";
            mean.values.assign(hours.size(), 0.0f);
            for (size_t i = 0; i < hours.size(); i++) {
                double sum = 0.0;
                int n = 0;
                for (const auto& l : result->daily) {
                    if (!l.earlier && i < l.values.size() && !std::isnan(l.values[i])) {
                        sum += l.values[i];
                        n++;
                    }
                }
                mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
            }
            result->daily.push_back(std::move(mean));
            // the months
            const auto months = UtilityCfs::months(run);
            std::vector<std::future<std::vector<float>>> monthlyJobs;
            for (size_t k = 0; k < runs.size(); k++) {
                const auto past = runs[k];
                monthlyJobs.push_back(std::async(std::launch::async, [=] {
                    std::vector<float> v;
                    std::string message;
                    UtilityCfs::pointMonthly(past, 1, variable, months, lat, lon, v, message);
                    doneCount->fetch_add(1);
                    return v;
                }));
            }
            for (const auto& m : months) {
                result->months.push_back(QDate{std::stoi(m.substr(0, 4)), std::stoi(m.substr(4, 2)), 1}.toString("MMM"));
            }
            Line monthlyMean;
            monthlyMean.values.assign(months.size(), 0.0f);
            std::vector<int> counts(months.size(), 0);
            std::vector<double> sums(months.size(), 0.0);
            int member = 1;
            for (auto& job : monthlyJobs) {
                Line line;
                line.member = member++;
                line.values = job.get();
                for (size_t i = 0; i < line.values.size(); i++) {
                    if (!std::isnan(line.values[i])) {
                        sums[i] += line.values[i];
                        counts[i]++;
                    }
                }
                result->monthly.push_back(std::move(line));
            }
            for (size_t i = 0; i < months.size(); i++) {
                monthlyMean.values[i] = counts[i] > 0 ? static_cast<float>(sums[i] / counts[i]) : std::nanf("");
            }
            result->monthly.push_back(std::move(monthlyMean));
        },
        [this, result, error] {
            if (closed) {
                return;
            }
            loading = false;
            progress.stop();
            go->setEnabled(true);
            if (!error->empty()) {
                status->setText(QString::fromStdString(*error));
                return;
            }
            status->setText("");
            chart->set(*result);
        }};
}
