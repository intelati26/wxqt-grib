// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SpaceWeatherViewer.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <QDate>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include "hurricane/UtilityHdob.h"
#include "misc/ImageViewer.h"
#include "misc/TextViewerStatic.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"

namespace {
    // NOAA's scale colours: none green, 1 yellow, 2 orange, 3 dark orange, 4 red, 5 dark red
    QColor scaleColor(int n) {
        static const QColor colors[] = {QColor{70, 170, 90}, QColor{240, 210, 40}, QColor{245, 160, 30}, QColor{240, 110, 30}, QColor{225, 50, 40}, QColor{150, 20, 30}};
        return n < 0 ? QColor{150, 150, 150} : colors[std::min(n, 5)];
    }

    QString tile(const QString& letter, int n) {
        const auto color = scaleColor(n);
        return "<td align='center' style='background:" + color.name() + "; color:" + (n == 1 ? QString{"#202020"} : QString{"#ffffff"}) + "; font-weight:bold; padding:4px 10px'>" +
            letter + (n < 0 ? QString{"-"} : QString::number(n)) + "</td>";
    }

    QString dayName(const std::string& date) {
        const auto d = QDate::fromString(QString::fromStdString(date), Qt::ISODate);
        return d.isValid() ? QLocale{QLocale::English}.toString(d, "ddd d MMM") : QString::fromStdString(date);
    }

    long nowSeconds() {
        return static_cast<long>(std::time(nullptr));
    }
}

// ---- the charts ----

SpaceChart::SpaceChart(Kind k, QWidget * parent) : QWidget{parent}, kind{k} {
    setMinimumSize(640, 210);
    ChartExport::install(this, k == Kp ? "Planetary K index" : k == Xray ? "GOES X-ray flux" : k == Wind ? "Solar wind" : "Interplanetary magnetic field");
}

void SpaceChart::setData(const std::shared_ptr<SpaceData::Bundle>& d) {
    data = d;
    update();
}

void SpaceChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{250, 250, 250});
    if (!data) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "Loading...");
        return;
    }
    const long now = nowSeconds();
    const bool isKp = kind == Kp;
    // the time span: Kp from four days ago to three days ahead, the rest the last day
    const double spanBefore = isKp ? 4.0 * 86400.0 : 86400.0;
    const double spanAfter = isKp ? 3.0 * 86400.0 : 0.0;
    const double t0 = static_cast<double>(now) - spanBefore;
    const double t1 = static_cast<double>(now) + spanAfter;
    const QRectF area{58.0, 26.0, width() - 58.0 - (kind == Wind ? 52.0 : 16.0), height() - 26.0 - 30.0};
    const auto xOf = [&] (double seconds) { return area.left() + area.width() * (seconds - t0) / (t1 - t0); };
    QFont base{p.font()};
    base.setPixelSize(11);
    QFont bold{base};
    bold.setBold(true);
    bold.setPixelSize(12);
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    static const char * titles[] = {"Planetary K index (Kp)", "GOES X-ray flux, 0.1 to 0.8 nm (W/m2)", "Solar wind speed (blue, km/s) and density (orange, particles/cm3)", "Interplanetary magnetic field: total Bt (grey) and Bz (nT)"};
    p.drawText(QPointF{area.left(), area.top() - 8}, titles[kind]);
    p.setFont(base);
    p.setPen(QColor{190, 190, 190});
    p.setBrush(Qt::white);
    p.drawRect(area);
    // the time axis: days for Kp, every 4 hours for the rest
    const double step = isKp ? 86400.0 : 4.0 * 3600.0;
    const long firstTick = static_cast<long>(std::ceil(t0 / step) * step);
    for (double t = static_cast<double>(firstTick); t <= t1; t += step) {
        p.setPen(QColor{232, 232, 232});
        p.drawLine(QPointF{xOf(t), area.top()}, QPointF{xOf(t), area.bottom()});
        p.setPen(QColor{70, 70, 70});
        const auto text = QString::fromStdString(UtilityHdob::timeText(static_cast<long>(t)));   // "08 Oct 12:00Z"
        p.drawText(QRectF{xOf(t) - 40, area.bottom() + 3, 80, 14}, Qt::AlignHCenter, isKp ? text.left(6) : text.mid(7));
    }
    const auto yTicks = [&] (double lo, double hi, double step2, int decimals) {
        for (double v = std::ceil(lo / step2) * step2; v <= hi + 1e-9; v += step2) {
            const double y = area.bottom() - area.height() * (v - lo) / (hi - lo);
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{area.left() - 52, y - 7, 48, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(v, 'f', decimals));
        }
    };
    p.save();
    if (kind == Kp) {
        yTicks(0, 9, 1, 0);
        p.setClipRect(area);
        // the G levels: Kp 5 to 9 are G1 to G5
        for (int g = 1; g <= 5; g++) {
            const double y = area.bottom() - area.height() * (g + 4) / 9.0;
            p.setPen(QPen{QColor{200, 120, 40, 140}, 1.0, Qt::DashLine});
            p.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
        }
        const double barWidth = std::max(2.0, area.width() * 10800.0 / (t1 - t0) - 1.0);
        for (const auto& pt : data->kp) {
            if (pt.seconds + 10800 < t0 || pt.seconds > t1) {
                continue;
            }
            static const QColor colors[] = {QColor{70, 170, 90}, QColor{240, 210, 40}, QColor{245, 160, 30}, QColor{240, 110, 30}, QColor{225, 50, 40}, QColor{150, 20, 30}};
            const int g = UtilitySpace::kpScale(pt.value);
            QColor color = pt.value >= 4.0 && g == 0 ? QColor{190, 200, 40} : colors[g];
            if (pt.kind == 2) {
                color.setAlpha(120);
            }
            const double h = area.height() * pt.value / 9.0;
            p.setPen(pt.kind == 2 ? QPen{color.darker(130), 1.0, Qt::DashLine} : QPen{Qt::NoPen});
            p.setBrush(color);
            p.drawRect(QRectF{xOf(static_cast<double>(pt.seconds)), area.bottom() - h, barWidth, h});
        }
    } else if (kind == Xray) {
        // log10 of the flux from 1e-9 to 1e-3, the class bands A B C M X
        const double lo = -9.0;
        const double hi = -3.0;
        const auto yOf = [&] (double flux) { return area.bottom() - area.height() * (std::log10(std::max(flux, 1e-10)) - lo) / (hi - lo); };
        static const char * names[] = {"A", "B", "C", "M", "X"};
        for (int i = 0; i < 5; i++) {
            const double y = yOf(std::pow(10.0, -8.0 + i));
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
            p.setPen(QColor{70, 70, 70});
            p.drawText(QRectF{area.left() - 52, y - 7, 48, 14}, Qt::AlignRight | Qt::AlignVCenter, QString{"1e-%1"}.arg(8 - i));
            p.drawText(QRectF{area.right() - 24, yOf(std::pow(10.0, -8.0 + i + 0.5)) - 7, 20, 14}, Qt::AlignRight | Qt::AlignVCenter, names[i]);
        }
        p.setClipRect(area);
        QPainterPath path;
        bool started = false;
        for (const auto& pt : data->xray) {
            if (pt.seconds < t0) {
                continue;
            }
            const QPointF at{xOf(static_cast<double>(pt.seconds)), yOf(pt.value)};
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
        }
        p.setPen(QPen{QColor{200, 70, 30}, 1.8});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
    } else if (kind == Wind) {
        double speedHigh = 600.0;
        double densityHigh = 10.0;
        for (const auto& pt : data->wind) {
            if (pt.seconds >= t0) {
                speedHigh = std::max(speedHigh, pt.value);
                if (UtilitySpace::has(pt.second)) densityHigh = std::max(densityHigh, pt.second);
            }
        }
        speedHigh = std::ceil(speedHigh / 100.0) * 100.0;
        densityHigh = std::ceil(densityHigh / 5.0) * 5.0;
        yTicks(200, speedHigh, speedHigh > 800 ? 200 : 100, 0);
        p.setClipRect(area);
        const auto line = [&] (bool density, const QColor& color) {
            QPainterPath path;
            bool started = false;
            for (const auto& pt : data->wind) {
                const double v = density ? pt.second : pt.value;
                if (pt.seconds < t0 || !UtilitySpace::has(v)) {
                    continue;
                }
                const double y = density ? area.bottom() - area.height() * v / densityHigh : area.bottom() - area.height() * (v - 200.0) / (speedHigh - 200.0);
                const QPointF at{xOf(static_cast<double>(pt.seconds)), std::clamp(y, area.top(), area.bottom())};
                started ? path.lineTo(at) : path.moveTo(at);
                started = true;
            }
            p.setPen(QPen{color, 1.7});
            p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        };
        line(true, QColor{235, 140, 30});
        line(false, QColor{30, 90, 200});
        p.restore();
        p.setPen(QColor{235, 140, 30});
        for (double v = 0; v <= densityHigh + 0.01; v += densityHigh / 4.0) {
            p.drawText(QRectF{area.right() + 4, area.bottom() - area.height() * v / densityHigh - 7, 46, 14}, Qt::AlignLeft | Qt::AlignVCenter, QString::number(v, 'f', 0));
        }
        p.save();
        p.setClipRect(area);
    } else {
        double reach = 10.0;
        for (const auto& pt : data->mag) {
            if (pt.seconds >= t0) {
                reach = std::max({reach, pt.value, std::fabs(UtilitySpace::has(pt.second) ? pt.second : 0.0)});
            }
        }
        reach = std::ceil(reach / 5.0) * 5.0;
        yTicks(-reach, reach, reach > 20 ? 10 : 5, 0);
        p.setClipRect(area);
        const auto yOf = [&] (double v) { return area.center().y() - area.height() / 2.0 * v / reach; };
        p.setPen(QPen{QColor{90, 90, 90}, 1.2});
        p.drawLine(QPointF{area.left(), yOf(0)}, QPointF{area.right(), yOf(0)});
        // Bz as a filled area about zero: southward (negative, the geoeffective side) in red, northward in blue
        QPainterPath north, south;
        QPainterPath total;
        bool started = false;
        const double base0 = yOf(0);
        for (size_t i = 0; i < data->mag.size(); i++) {
            const auto& pt = data->mag[i];
            if (pt.seconds < t0) {
                continue;
            }
            const double x = xOf(static_cast<double>(pt.seconds));
            if (UtilitySpace::has(pt.second)) {
                const double y = yOf(pt.second);
                auto& target = pt.second < 0 ? south : north;
                target.moveTo(x, base0);
                target.lineTo(x, y);
            }
            const QPointF at{x, yOf(pt.value)};
            started ? total.lineTo(at) : total.moveTo(at);
            started = true;
        }
        p.setPen(QPen{QColor{230, 70, 70, 190}, 1.6});
        p.drawPath(south);
        p.setPen(QPen{QColor{70, 110, 230, 190}, 1.6});
        p.drawPath(north);
        p.setPen(QPen{QColor{110, 110, 110}, 1.4});
        p.setBrush(Qt::NoBrush);
        p.drawPath(total);
    }
    // now
    if (isKp) {
        p.setPen(QPen{QColor{215, 40, 40}, 1.2, Qt::DotLine});
        p.drawLine(QPointF{xOf(static_cast<double>(now)), area.top()}, QPointF{xOf(static_cast<double>(now)), area.bottom()});
    }
    p.restore();
    p.setPen(QColor{110, 110, 110});
    QFont small{base};
    small.setPixelSize(10);
    p.setFont(small);
    p.drawText(QPointF{area.left(), height() - 4.0}, "NOAA Space Weather Prediction Center (services.swpc.noaa.gov), times in UTC" + QString{isKp ? "; paler bars are forecasts" : ""});
}

// ---- the screen ----

SpaceWeatherViewer::SpaceWeatherViewer(Window * parent)
    : Window{parent}
    , buttonRefresh{this, None, "Refresh"}
    , textStatus{this, "Loading the space weather..."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Space weather - NOAA SWPC");
    textStatus.setWordWrap(false);
    buttonRefresh.connect([this] { load(); });
    row.addWidget(buttonRefresh);
    static const std::pair<const char *, const char *> texts[] = {
        {"3-day forecast", "text/3-day-forecast.txt"}, {"Discussion", "text/discussion.txt"}, {"Geomagnetic forecast", "text/3-day-geomag-forecast.txt"},
        {"27-day outlook", "text/27-day-outlook.txt"}, {"Weekly report", "text/weekly.txt"}};
    for (const auto& [label, path] : texts) {
        auto * button = new Button{this, None, label};
        const std::string url = SpaceData::url(path);
        const std::string title = label;
        button->connect([this, url, title] {
            new FutureText{this, url, [this, title] (const std::string& text) {
                new TextViewerStatic{this, text.empty() ? title + " is not available right now." : text, "SWPC: " + title, 820, 720};
            }};
        });
        row.addWidget(*button);
        textButtons.push_back(button);
    }
    row.addStretch();
    content = new QWidget{this};
    layout = new QVBoxLayout{content};
    layout->setContentsMargins(4, 0, 12, 8);
    scalesLabel = new QLabel{content};
    scalesLabel->setTextFormat(Qt::RichText);
    layout->addWidget(scalesLabel);
    nowLabel = new QLabel{content};
    nowLabel->setTextFormat(Qt::RichText);
    nowLabel->setWordWrap(true);
    layout->addWidget(nowLabel);
    for (int i = 0; i < 4; i++) {
        charts[i] = new SpaceChart{static_cast<SpaceChart::Kind>(i), content};
        layout->addWidget(charts[i]);
    }
    auto * heading = new QLabel{"<b>Pictures</b> (click one for the large picture)", content};
    layout->addWidget(heading);
    pictures = new QHBoxLayout;
    layout->addLayout(pictures);
    alertsLabel = new QLabel{content};
    alertsLabel->setTextFormat(Qt::RichText);
    alertsLabel->setWordWrap(true);
    layout->addWidget(alertsLabel);
    layout->addStretch();
    auto * scroll = new QScrollArea{this};
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    box.addLayout(row);
    box.addWidget(textStatus);
    box.addWidgetReal(scroll, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 900);
    addPicture("Aurora forecast, north", "images/animations/ovation/north/latest.jpg");
    addPicture("Aurora forecast, south", "images/animations/ovation/south/latest.jpg");
    addPicture("Sun, 304 A (SUVI)", "images/animations/suvi/primary/304/latest.png");
    addPicture("Sun, 195 A (SUVI)", "images/animations/suvi/primary/195/latest.png");
    addPicture("Corona (LASCO C2)", "images/animations/lasco-c2/latest.jpg");
    load();
}

// a thumbnail that opens the large picture; filled in when it arrives
void SpaceWeatherViewer::addPicture(const QString& title, const std::string& path) {
    auto * holder = new QVBoxLayout;
    auto * thumbnail = new QPushButton{content};
    thumbnail->setFlat(true);
    thumbnail->setFixedSize(150, 150);
    thumbnail->setCursor(Qt::PointingHandCursor);
    thumbnail->setToolTip(title);
    auto * caption = new QLabel{title, content};
    caption->setAlignment(Qt::AlignHCenter);
    caption->setWordWrap(true);
    caption->setFixedWidth(150);
    holder->addWidget(thumbnail);
    holder->addWidget(caption);
    pictures->addLayout(holder);
    const std::string url = SpaceData::url(path.c_str());
    QPointer<QPushButton> guard{thumbnail};
    new FutureBytes{this, url, [this, guard, url, title] (const QByteArray& bytes) {
        QPixmap picture;
        if (closed || guard.isNull() || bytes.size() < 500 || !picture.loadFromData(bytes)) {
            return;
        }
        guard->setIcon(QIcon{picture.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation)});
        guard->setIconSize(QSize{150, 150});
        QObject::connect(guard.data(), &QPushButton::clicked, [this, url, title] { new ImageViewer{this, url, title.toStdString()}; });
    }};
}

void SpaceWeatherViewer::load() {
    const int mine = ++generation;
    textStatus.setText(std::string{"Loading the space weather..."});
    auto fresh = std::make_shared<SpaceData::Bundle>();
    new FutureVoid{this, [fresh] { SpaceData::load(*fresh); }, [this, mine, fresh] {
        if (closed || mine != generation) {
            return;
        }
        data = fresh;
        fill();
    }};
}

void SpaceWeatherViewer::fill() {
    const auto& d = *data;
    // the scales: now, then the forecast days
    QString html = "<table cellspacing='6'><tr><td><b>Now</b></td>";
    if (!d.scales.empty()) {
        html += tile("R", d.scales[0].r) + tile("S", d.scales[0].s) + tile("G", d.scales[0].g);
    }
    html += "<td>&nbsp;&nbsp;R radio blackouts, S radiation storms, G geomagnetic storms (0 to 5)</td></tr>";
    for (size_t i = 1; i < d.scales.size(); i++) {
        const auto& day = d.scales[i];
        html += "<tr><td>" + dayName(day.date) + "</td><td align='center' style='color:#777'>R: " + (day.rMinor >= 0 ? QString::number(day.rMinor) + "%" : QString{"-"}) +
            (day.rMajor >= 0 ? " / " + QString::number(day.rMajor) + "%" : QString{}) + "</td><td align='center' style='color:#777'>S: " + (day.sProb >= 0 ? QString::number(day.sProb) + "%" : QString{"-"}) +
            "</td>" + tile("G", day.g) + "<td style='color:#777'>chance of R1-R2 / R3+, chance of S1+, expected geomagnetic scale</td></tr>";
    }
    scalesLabel->setText(html + "</table>");
    // the sun and the wind now
    QString now;
    if (!d.kp.empty()) {
        const auto lastKp = std::find_if(d.kp.rbegin(), d.kp.rend(), [] (const UtilitySpace::Point& p) { return p.kind != 2; });
        if (lastKp != d.kp.rend()) {
            now += "<b>Kp</b> " + QString::number(lastKp->value, 'f', 2) + (lastKp->kind == 1 ? " (estimated)" : "") + " &nbsp; ";
        }
        double peak = 0.0;
        long when = 0;
        for (const auto& p : d.kp) {
            if (p.kind == 2 && p.value > peak) {
                peak = p.value;
                when = p.seconds;
            }
        }
        if (peak > 0.0) {
            now += "<b>forecast peak</b> Kp " + QString::number(peak, 'f', 2) + " (" + QString::fromStdString(UtilityHdob::timeText(when)) + ") &nbsp; ";
        }
    }
    if (!d.wind.empty()) {
        const auto& w = d.wind.back();
        now += "<b>Solar wind</b> " + QString::number(std::lround(w.value)) + " km/s" + (UtilitySpace::has(w.second) ? ", " + QString::number(w.second, 'f', 1) + " /cm3" : QString{}) + " &nbsp; ";
    }
    if (!d.mag.empty()) {
        const auto& m = d.mag.back();
        now += "<b>Field</b> Bt " + QString::number(m.value, 'f', 1) + " nT" + (UtilitySpace::has(m.second) ? ", Bz " + QString::number(m.second, 'f', 1) + " nT" + (m.second < -5.0 ? " <span style='color:#d33'>(southward)</span>" : QString{}) : QString{}) + " &nbsp; ";
    }
    if (!d.flare.current.empty()) {
        now += "<b>X-ray</b> " + QString::fromStdString(d.flare.current) + (d.flare.maxClass.empty() ? QString{} : ", the day's last flare " + QString::fromStdString(d.flare.maxClass) +
            " at " + QString::fromStdString(d.flare.maxTime).mid(11, 5) + "Z");
    }
    nowLabel->setText(now.isEmpty() ? QString{"No current values."} : now);
    for (auto * chart : charts) {
        chart->setData(data);
    }
    QString alerts = "<b>Newest alerts, watches and warnings</b><br>";
    for (const auto& line : d.alerts) {
        alerts += QString::fromStdString(line).toHtmlEscaped() + "<br>";
    }
    alertsLabel->setText(d.alerts.empty() ? QString{"<b>Alerts</b>: none in the newest messages."} : alerts);
    textStatus.setText(d.problems.empty() ? std::string{"Right-click a chart to save it. Pictures and text products from NOAA SWPC."} : "Some feeds could not be read: " + d.problems);
}
