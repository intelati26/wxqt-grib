// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "cfs/CfsPlumeViewer.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <semaphore>
#include <QDateTime>
#include <QHBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "settings/Location.h"
#include "settings/UIPreferences.h"
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
        const double left = 62, right = 215, top = 34, gap = 46, bottomMargin = 34;
        const double plotWidth = width() - left - right;
        const bool months = !result.months.empty();
        const double dailyHeight = months ? (height() - top - bottomMargin - gap) * 0.62 : height() - top - bottomMargin;
        const double monthlyHeight = (height() - top - bottomMargin - gap) - dailyHeight;
        drawDaily(p, QRectF{left, top, plotWidth, dailyHeight});
        if (months) {
            drawMonthly(p, QRectF{left, top + dailyHeight + gap, plotWidth, monthlyHeight});
        }
        // the key
        double y = top + 6;
        const auto key = [&] (const QColor& color, const QString& text, double widthOf) {
            p.setPen(QPen{color, widthOf});
            p.drawLine(QPointF{width() - right + 14, y + 6}, QPointF{width() - right + 38, y + 6});
            p.setPen(Qt::black);
            p.drawText(QPointF{width() - right + 44, y + 11}, text);
            y += 18;
        };
        if (result.ensemble) {
            key(QColor{40, 110, 210, 140}, result.low.empty() ? QString{"%1 members"}.arg(result.daily.size() - 1) : QString{"mean +/- spread"}, 1.2);
        } else if (!result.runNames.empty()) {
            key(memberColor(1), result.runNames.front() + " (newest)", 1.4);
            if (result.runNames.size() > 2) {
                key(memberColor(static_cast<int>(result.runNames.size()) / 2 + 1), QString{"%1 runs between"}.arg(result.runNames.size() - 2), 1.4);
            }
            key(memberColor(static_cast<int>(result.runNames.size())), result.runNames.back() + " (oldest)", 1.4);
        }
        key(Qt::black, result.ensemble ? "ensemble mean" : "mean of the runs", 2.6);
    }

private:
    // the newest run is blue, the oldest orange, the ones between along the way
    QColor memberColor(int m) const {
        const double n = std::max<size_t>(2, result.runNames.size());
        return QColor::fromHsv(static_cast<int>(225.0 - 195.0 * (m - 1) / (n - 1.0)), 210, 215);
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
        for (const float v : result.low) {
            if (!std::isnan(v)) {
                lo = std::min<double>(lo, v);
            }
        }
        for (const float v : result.high) {
            if (!std::isnan(v)) {
                hi = std::max<double>(hi, v);
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
        for (double n = std::ceil((minDay + result.runFraction) / dayStep) * dayStep; n - result.runFraction <= maxDay; n += dayStep) {   // the marks are at midnight UTC
            const double d = n - result.runFraction;
            p.setPen(QColor{235, 235, 235});
            p.drawLine(QPointF{xOf(d), area.top()}, QPointF{xOf(d), area.bottom()});
            p.setPen(Qt::black);
            p.drawText(QRectF{xOf(d) - 30, area.bottom() + 2, 60, 16}, Qt::AlignCenter, result.runDay.addDays(static_cast<qint64>(n)).toString("d MMM"));
        }
        // the spread of the runs: the shading runs from the lowest to the highest at each day
        {
            QPolygonF upper, lower;
            for (size_t i = 0; i < result.days.size(); i++) {
                float a = 1e30f, b = -1e30f;
                if (!result.low.empty() && i < result.low.size() && !std::isnan(result.low[i])) {   // a given band
                    a = result.low[i];
                    b = result.high[i];
                }
                for (const auto& l : result.daily) {
                    if (result.low.empty() && l.member > 0 && i < l.values.size() && !std::isnan(l.values[i])) {
                        a = std::min(a, l.values[i]);
                        b = std::max(b, l.values[i]);
                    }
                }
                if (a <= b) {
                    upper << QPointF{xOf(result.days[i]), yOf(b)};
                    lower.prepend(QPointF{xOf(result.days[i]), yOf(a)});
                }
            }
            p.setPen(Qt::NoPen);
            p.setBrush(QColor{120, 120, 120, 45});
            p.drawPolygon(upper + lower);
            p.setBrush(Qt::NoBrush);
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
            p.setPen(QPen{line.earlier ? QColor{150, 150, 150} : line.member == 0 ? QColor{Qt::black} : result.ensemble ? QColor{40, 110, 210, 110} : memberColor(line.member), line.earlier ? 1.0 : line.member == 0 ? 2.6 : result.ensemble ? 1.0 : 1.3});
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
        axes(p, area, from, to, step, result.monthlyUnits);
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

CfsPlumeViewer::CfsPlumeViewer(Window * parent, double lat, double lon, const QString& modelId) : Window{parent} {
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
    model = new QComboBox{this};
    model->addItem("GEFS (31 members, 16 days)", "gefs");
    model->addItem("GFS (16 days)", "gfs");
    model->addItem("CFSv2 (9 months)", "cfs");
    model->setCurrentIndex(std::max(0, model->findData(modelId)));
    field = new QComboBox{this};
    span = new QComboBox{this};
    count = new QComboBox{this};
    count = new QComboBox{this};
    stride = new QComboBox{this};
    stride->addItem("every 6 hours", 6);
    stride->addItem("every 12 hours", 12);
    stride->addItem("every 24 hours", 24);
    monthlyShow = new QComboBox{this};
    monthlyShow->addItem("months: values", 0);
    monthlyShow->addItem("months: departure from the CFS normal", 1);
    monthlyShow->addItem("months: departure in std deviations", 2);
    fillChoices();
    go = new QPushButton{"Load", this};
    status = new QLabel{"", this};
    top->addWidget(place);
    top->addWidget(new QLabel{"lat", this});
    top->addWidget(latBox);
    top->addWidget(new QLabel{"lon", this});
    top->addWidget(lonBox);
    top->addWidget(model);
    top->addWidget(field);
    top->addWidget(span);
    top->addWidget(count);
    top->addWidget(stride);
    top->addWidget(monthlyShow);
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
    QObject::connect(model, &QComboBox::currentIndexChanged, this, [this] (int) { fillChoices(); });
    QObject::connect(go, &QPushButton::clicked, this, [this] { load(); });
    progress.setInterval(300);
    QObject::connect(&progress, &QTimer::timeout, this, [this] {
        if (loading) {
            status->setText(QString{"reading the CFSv2 run: %1 of %2 parts"}.arg(done->load()).arg(total->load()));
        }
    });
    load();
}

void CfsPlumeViewer::choose(const QString& modelId, int runs, int everyHours, int monthly) {
    monthlyShow->setCurrentIndex(std::clamp(monthly, 0, 2));
    model->setCurrentIndex(std::max(0, model->findData(modelId)));
    count->setCurrentIndex(std::max(0, count->findData(runs)));
    stride->setCurrentIndex(std::max(0, stride->findData(everyHours)));
    if (loading) {
        again = true;
    } else {
        load();
    }
}

// the field and span choices of the model
void CfsPlumeViewer::fillChoices() {
    const QString id = model->currentData().toString();
    const bool gfs = id == "gfs", gefs = id == "gefs";
    field->clear();
    for (const auto& v : gefs ? UtilityCfs::gefsVariables() : gfs ? UtilityCfs::gfsVariables() : UtilityCfs::variables()) {
        field->addItem(QString::fromStdString(v.label), QString::fromStdString(v.key));
    }
    span->clear();
    if (gfs || gefs) {
        span->addItems({"5 days, 6 hourly", "10 days, 6 hourly", "16 days, daily (00Z)"});
        span->setCurrentIndex(0);   // (each member or run is a field for every step: 31 members of 5 days at 6 hours is 620 reads, a few minutes the first time)
    } else {
        span->addItems({"15 days, 6 hourly", "45 days, daily (00Z)", "90 days, every other day"});
        span->setCurrentIndex(1);
    }
    count->clear();
    if (gefs) {
        count->addItem("all 31 members", 31);
        count->addItem("mean and spread (fast)", 0);
    } else {
        count->addItem("4 runs", 4);
        count->addItem("8 runs", 8);
        count->addItem("12 runs", 12);
        count->addItem("16 runs", 16);
    }
    count->setCurrentIndex(0);
    stride->setEnabled(!gefs);
    monthlyShow->setEnabled(!gefs && !gfs);
}

void CfsPlumeViewer::load() {
    if (loading) {
        return;
    }
    loading = true;
    go->setEnabled(false);
    const double lat = latBox->value(), lon = lonBox->value();
    const QString modelId = model->currentData().toString();
    const bool gfs = modelId == "gfs", gefs = modelId == "gefs";
    const std::string key = field->currentData().toString().toStdString();
    const auto & list = gefs ? UtilityCfs::gefsVariables() : gfs ? UtilityCfs::gfsVariables() : UtilityCfs::variables();
    const auto found = std::find_if(list.begin(), list.end(), [&key] (const auto& v) { return v.key == key; });
    if (found == list.end()) {
        loading = false;
        go->setEnabled(true);
        return;
    }
    const auto variable = *found;
    // the valid times are the same for every run (a multiple of the step from midnight UTC, after the newest run's own time): each run is read at the hour that lands on them
    int stepHours = 24, spanHours = 1080;
    if (gfs || gefs) {
        stepHours = span->currentIndex() == 2 ? 24 : 6;
        spanHours = span->currentIndex() == 0 ? 120 : span->currentIndex() == 1 ? 240 : 384;
    } else {
        stepHours = span->currentIndex() == 0 ? 6 : span->currentIndex() == 1 ? 24 : 48;
        spanHours = span->currentIndex() == 0 ? 360 : span->currentIndex() == 1 ? 1080 : 2160;
    }
    const int longestHour = gfs || gefs ? 384 : 6984;
    const int monthlyMode = gfs || gefs ? 0 : monthlyShow->currentData().toInt();
    const int baseline = Utility::readPrefInt("MM_CFS_BASE", 1999) == 1982 ? 1982 : 1999;
    const int runCount = count->currentData().toInt(), strideHours = stride->currentData().toInt();
    const bool us = UIPreferences::unitsF;   // the program's units: Fahrenheit and inches, or Celsius and millimeters
    auto result = std::make_shared<Result>();
    auto error = std::make_shared<std::string>();
    done->store(0);
    total->store(gefs ? (runCount > 0 ? runCount : 2) : gfs ? runCount : 2 * runCount);
    progress.start();
    status->setText("looking for the newest run...");
    const auto doneCount = done;
    new FutureVoid{this,
        [=] {
            UtilityCfs::Run run;
            if (!(gefs ? UtilityCfs::gefsLatestRun(run) : gfs ? UtilityCfs::gfsLatestRun(run) : UtilityCfs::latestRun(run))) {
                *error = gefs ? "no GEFS run found" : gfs ? "no GFS run found" : "no CFSv2 run found";
                return;
            }
            const auto runTime = UtilityCfs::runTime(run);
            result->title = QString::fromStdString(variable.label + " at " + std::to_string(lat).substr(0, std::to_string(lat).find('.') + 3) + ", " + std::to_string(lon).substr(0, std::to_string(lon).find('.') + 3) +
                                                   (gefs ? "   GEFS run " : gfs ? "   GFS run " : "   CFSv2 run ") + UtilityCfs::runText(run));
            result->runDay = runTime.date();
            result->runFraction = runTime.time().hour() / 24.0;
            result->ensemble = gefs;
            const qint64 runSeconds = runTime.toSecsSinceEpoch();
            std::vector<qint64> validTimes;
            for (qint64 t = (runSeconds / (stepHours * 3600) + 1) * stepHours * 3600; t <= runSeconds + static_cast<qint64>(spanHours) * 3600; t += static_cast<qint64>(stepHours) * 3600) {
                validTimes.push_back(t);
                result->days.push_back((t - runSeconds) / 86400.0);
            }
            // one run's (or one member's) values at those times: the hour is the time minus the run's own, and a time beyond the run's end stays empty
            const auto seriesOf = [=] (const UtilityCfs::Run& past, const std::string& member) {
                const auto at = UtilityCfs::runTime(past).toSecsSinceEpoch();
                std::vector<int> hours;
                std::vector<size_t> where;
                for (size_t i = 0; i < validTimes.size(); i++) {
                    const qint64 h = (validTimes[i] - at) / 3600;
                    if (h > 0 && h <= longestHour) {
                        hours.push_back(static_cast<int>(h));
                        where.push_back(i);
                    }
                }
                std::vector<float> got, all(validTimes.size(), std::nanf(""));
                std::string message;
                if (gefs) {
                    UtilityCfs::gefsPointSeries(past, variable, member, hours, lat, lon, got, message);
                } else if (gfs) {
                    UtilityCfs::gfsPointSeries(past, variable, hours, lat, lon, got, message);
                } else {
                    UtilityCfs::pointSeries(past, 1, variable, hours, lat, lon, got, message);
                }
                for (size_t i = 0; i < where.size() && i < got.size(); i++) {
                    all[where[i]] = got[i];
                }
                if (qEnvironmentVariableIsSet("WXQT_PLUME_DEBUG")) {
                    int good = 0;
                    for (const float v : got) good += std::isnan(v) ? 0 : 1;
                    qWarning("plume: run %s%s %s asked %zu hours (%d .. %d) got %d [%s]", past.date.c_str(), past.cycle.c_str(), member.c_str(), hours.size(), hours.empty() ? 0 : hours.front(), hours.empty() ? 0 : hours.back(), good, message.c_str());
                }
                return all;
            };
            struct Job {
                int member;
                std::future<std::vector<float>> values;
            };
            std::vector<Job> jobs;
            std::counting_semaphore<16> gate{gfs ? 3 : 6};
            const auto start = [&] (int member, const UtilityCfs::Run& past, const std::string& name) {
                jobs.push_back({member, std::async(std::launch::async, [=, &gate] {
                    gate.acquire();   // NOMADS answers a few requests at a time (the GFS runs are read three at a time)
                    auto v = seriesOf(past, name);
                    gate.release();
                    doneCount->fetch_add(1);
                    return v;
                })});
            };
            std::vector<UtilityCfs::Run> runs;
            if (gefs) {
                runs = {run};
                if (runCount > 0) {   // the control and the thirty members
                    start(1, run, "c00");
                    for (int m = 1; m <= 30; m++) {
                        start(m + 1, run, std::string{"p"} + (m < 10 ? "0" : "") + std::to_string(m));
                    }
                } else {              // the mean and the spread of the members
                    start(0, run, "");
                    start(-1, run, "spr");
                }
                result->runNames.push_back(QString::fromStdString(run.cycle) + "Z " + runTime.toString("d MMM"));
            } else {
                runs = UtilityCfs::recentRuns(run, runCount, strideHours);
                for (size_t k = 0; k < runs.size(); k++) {
                    result->runNames.push_back(QString::fromStdString(runs[k].cycle) + "Z " + UtilityCfs::runTime(runs[k]).toString("d MMM"));
                    start(static_cast<int>(k) + 1, runs[k], "");
                }
            }
            Line spread;
            for (auto& job : jobs) {
                Line line;
                line.member = job.member;
                line.values = job.values.get();
                if (job.member < 0) {
                    spread = std::move(line);
                } else {
                    result->daily.push_back(std::move(line));
                }
            }
            if (gefs && runCount == 0 && !result->daily.empty()) {   // the mean is drawn as the mean; the spread makes the band round it
                const auto& mean = result->daily.front().values;
                for (size_t i = 0; i < mean.size(); i++) {
                    result->low.push_back(i < spread.values.size() ? mean[i] - spread.values[i] : std::nanf(""));
                    result->high.push_back(i < spread.values.size() ? mean[i] + spread.values[i] : std::nanf(""));
                }
            } else {
                Line mean;
                mean.name = "mean";
                mean.values.assign(validTimes.size(), 0.0f);
                for (size_t i = 0; i < validTimes.size(); i++) {
                    double sum = 0.0;
                    int n = 0;
                    for (const auto& l : result->daily) {
                        if (i < l.values.size() && !std::isnan(l.values[i])) {
                            sum += l.values[i];
                            n++;
                        }
                    }
                    mean.values[i] = n > 0 ? static_cast<float>(sum / n) : std::nanf("");
                }
                result->daily.push_back(std::move(mean));
            }
            // the units of the program: a temperature in degrees F and a depth in inches unless the settings say metric
            const bool temperature = variable.units == "deg C";
            const bool depth = variable.units == "mm" || variable.units == "mm/day";
            const auto convert = [&] (std::vector<float>& values, bool delta) {
                for (auto& v : values) {
                    if (std::isnan(v)) {
                        continue;
                    }
                    if (us && temperature) {
                        v = static_cast<float>(v * 1.8 + (delta ? 0.0 : 32.0));
                    } else if (us && depth) {
                        v = static_cast<float>(v / 25.4);
                    }
                }
            };
            const QString unitText = us && temperature ? QString{"deg F"} : us && depth ? QString::fromStdString(variable.units == "mm" ? "in" : "in/day") : QString::fromStdString(variable.units);
            result->units = unitText;
            const auto finish = [&] {
                for (auto& line : result->daily) {
                    convert(line.values, false);
                }
                convert(result->low, false);
                convert(result->high, false);
                for (auto& line : result->monthly) {
                    if (monthlyMode != 2) {
                        convert(line.values, monthlyMode == 1);
                    }
                }
                result->monthlyUnits = monthlyMode == 2 ? QString{"standard deviations  (month)"} : monthlyMode == 1 ? unitText + "  (month, departure)" : unitText + "  (month mean)";
            };
            if (gfs || gefs) {
                finish();
                return;   // (no monthly means: 16 days)
            }
            // the months
            const auto months = UtilityCfs::months(run);
            std::vector<std::future<std::vector<float>>> monthlyJobs;
            for (size_t k = 0; k < runs.size(); k++) {
                const auto past = runs[k];
                monthlyJobs.push_back(std::async(std::launch::async, [=] {
                    std::vector<float> v;
                    std::string message;
                    if (monthlyMode == 0) { UtilityCfs::pointMonthly(past, 1, variable, months, lat, lon, v, message); } else { UtilityCfs::pointMonthlyDeparture(past, 1, variable, months, monthlyMode == 2, baseline, lat, lon, v, message); }
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
            finish();
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
            if (again) {
                again = false;
                load();
                return;
            }
            chart->set(*result);
        }};
}
