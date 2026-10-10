// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartPainter.h"
#include "hurricane/DropsondeViewer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/DropProfile.h"
#include "hurricane/UtilityHdob.h"
#include "models/SoundingViewer.h"
#include "ui/ChartExport.h"
#include "ui/WindBarb.h"

namespace {
    bool has(double v) { return UtilityDropsonde::has(v); }

    QColor bandColor(double pressure) {   // the hodograph's line: the lowest layers red, then orange, green, blue
        return pressure >= 925.0 ? QColor{215, 40, 40} : pressure >= 850.0 ? QColor{235, 140, 20} : pressure >= 700.0 ? QColor{40, 150, 70} : QColor{40, 90, 200};
    }
}

QString DropsondeChart::compass(double degrees) {
    static const char * names[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
    return names[(static_cast<int>(std::lround(degrees / 22.5)) % 16 + 16) % 16];
}

QString DropsondeChart::windText(double direction, double speed) {
    if (!has(direction) || !has(speed)) {
        return "N/A";
    }
    return speed < 1.0 ? QString{"calm"} : compass(direction) + " at " + QString::number(static_cast<int>(std::lround(speed))) + " kt";
}

double DropsondeChart::relativeHumidity(double temperature, double dewPoint) {
    if (!has(temperature) || !has(dewPoint)) {
        return UtilityDropsonde::missing;
    }
    const auto saturation = [] (double t) { return std::exp(17.625 * t / (243.04 + t)); };
    return std::min(100.0, 100.0 * saturation(dewPoint) / saturation(temperature));
}

bool DropsondeChart::meanWind(const UtilityDropsonde::Drop& drop, double meters, double& direction, double& speed) {
    SoundingProfile profile;
    std::string error;
    if (!DropProfile::build(drop, profile, error) || profile.size() < 2) {
        return false;
    }
    const double base = profile.sfcHght();
    // the mean of u and v over height, level to level (the trapezoid rule), up to `meters` above the surface
    double sumU = 0.0, sumV = 0.0, depth = 0.0;
    for (size_t i = 0; i + 1 < profile.size(); i++) {
        const double h0 = profile.hght[i] - base;
        const double h1 = profile.hght[i + 1] - base;
        if (h0 >= meters || profile.u[i] < -9000.0 || profile.u[i + 1] < -9000.0) {
            continue;
        }
        const double top = std::min(h1, meters);
        const double f = h1 > h0 ? (top - h0) / (h1 - h0) : 0.0;
        const double u1 = profile.u[i] + (profile.u[i + 1] - profile.u[i]) * f;
        const double v1 = profile.v[i] + (profile.v[i + 1] - profile.v[i]) * f;
        const double d = top - h0;
        sumU += 0.5 * (profile.u[i] + u1) * d;
        sumV += 0.5 * (profile.v[i] + v1) * d;
        depth += d;
    }
    if (depth < meters * 0.5) {
        return false;   // the report does not reach that high with winds
    }
    const double u = sumU / depth;
    const double v = sumV / depth;
    speed = std::hypot(u, v);
    direction = std::fmod(std::atan2(-u, -v) * 180.0 / std::numbers::pi + 360.0, 360.0);
    return true;
}

DropsondeChart::DropsondeChart(const UtilityDropsonde::Drop& d, QWidget * parent)
    : QWidget{parent}
    , drop{d}
{
    setMinimumSize(1100, 700);
    ChartExport::install(this, "Dropsonde (NHC reconnaissance)");
}

void DropsondeChart::paintEvent(QPaintEvent *) {
    ChartPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), Qt::white);
    // a saved or copied picture carries the plots only: the tables and the lines of words are for the screen
    const bool exportMode = ChartExport::exporting();
    const double w = width();
    const double h = height();
    QFont base{p.font()};
    base.setPixelSize(12);
    p.setFont(base);
    // the title
    const double lat = has(drop.splashLat) ? drop.splashLat : drop.lat;
    const double lon = has(drop.splashLon) ? drop.splashLon : drop.lon;
    p.setPen(QColor{30, 30, 30});
    QFont bold{base};
    bold.setBold(true);
    bold.setPixelSize(15);
    p.setFont(bold);
    if (has(lat) && has(lon)) {
        p.drawText(QPointF{14, 22}, "Dropsonde at " + QString::number(std::fabs(lat), 'f', 1) + (lat < 0 ? "S " : "N ") + QString::number(std::fabs(lon), 'f', 1) + (lon < 0 ? "W" : "E"));
    }
    p.drawText(QRectF{w * 0.45, 4, w * 0.54, 24}, Qt::AlignRight | Qt::AlignVCenter, "Launch time: " + QString::fromStdString(UtilityHdob::timeText(drop.seconds)));
    p.setFont(base);
    p.setPen(QColor{90, 90, 90});
    p.drawText(QPointF{14, 40}, QString::fromStdString(drop.mission));

    // the surface: the lowest level that is not below the ground (the report can carry a 1000 mb level under a 979 mb surface)
    const UtilityDropsonde::Level * surfaceLevel = nullptr;
    for (const auto& l : drop.levels) {
        if (!has(drop.surfacePressure) || l.pressure <= drop.surfacePressure + 0.5) {
            surfaceLevel = &l;
            break;
        }
    }
    // the levels with a temperature, lowest first
    std::vector<const UtilityDropsonde::Level *> temps;
    std::vector<const UtilityDropsonde::Level *> winds;
    for (const auto& l : drop.levels) {
        if (has(drop.surfacePressure) && l.pressure > drop.surfacePressure + 0.5) {
            continue;
        }
        if (has(l.temperature)) temps.push_back(&l);
        if (has(l.windSpeed) && has(l.windDirection)) winds.push_back(&l);
    }
    // ---- the skew-T ----
    const QRectF area{64, 56, w * 0.41 - 64, h - 56 - 52};
    double pTop = 700.0;
    double pBottom = 1010.0;
    for (const auto * l : temps) {
        pTop = std::min(pTop, std::floor(l->pressure / 100.0) * 100.0);
        pBottom = std::max(pBottom, std::ceil(l->pressure / 10.0) * 10.0);
    }
    pTop = std::max(pTop, 100.0);
    double tLow = 0.0, tHigh = 0.0;
    tLow = tHigh = temps.empty() ? 20.0 : temps.front()->temperature;
    for (const auto * l : temps) {
        tLow = std::min({tLow, l->temperature, has(l->dewPoint) ? l->dewPoint : 99.0});
        tHigh = std::max(tHigh, l->temperature);
    }
    const double skew = 36.0;   // degrees C of shift from the bottom to the top of the plot
    const double xLow = std::floor((tLow - 6.0) / 10.0) * 10.0;
    const double xHigh = std::ceil((tHigh + 4.0) / 10.0) * 10.0 + skew * 0.5;
    const auto yOf = [&] (double pressure) { return area.top() + area.height() * std::log(pressure / pTop) / std::log(pBottom / pTop); };
    const auto xOf = [&] (double t, double pressure) {
        const double shift = skew * std::log(pBottom / pressure) / std::log(pBottom / pTop);
        return area.left() + area.width() * (t + shift - xLow) / (xHigh - xLow);
    };
    p.setPen(QColor{40, 40, 40});
    p.setBrush(Qt::NoBrush);
    p.drawRect(area);
    p.save();
    p.setClipRect(area);
    // the isotherms, slanted, every 10 C
    p.setPen(QPen{QColor{150, 150, 150}, 1.0, Qt::DashLine});
    for (double t = xLow - skew; t <= xHigh; t += 10.0) {
        p.drawLine(QPointF{xOf(t, pBottom), yOf(pBottom)}, QPointF{xOf(t, pTop), yOf(pTop)});
    }
    // the isobars every 100 mb, and 50 mb in the lowest layers
    for (double pr = std::ceil(pTop / 50.0) * 50.0; pr <= pBottom; pr += 50.0) {
        p.setPen(QPen{pr == std::round(pr / 100.0) * 100.0 ? QColor{175, 175, 175} : QColor{222, 222, 222}, 1.0});
        p.drawLine(QPointF{area.left(), yOf(pr)}, QPointF{area.right(), yOf(pr)});
    }
    // the temperature and dew point
    const auto trace = [&] (bool dew, const QColor& color) {
        QPainterPath path;
        bool started = false;
        std::vector<QPointF> dots;
        for (const auto * l : temps) {
            const double v = dew ? l->dewPoint : l->temperature;
            if (!has(v)) {
                started = false;
                continue;
            }
            const QPointF at{xOf(v, l->pressure), yOf(l->pressure)};
            started ? path.lineTo(at) : path.moveTo(at);
            started = true;
            dots.push_back(at);
        }
        p.setPen(QPen{color, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        for (const auto& d : dots) {
            p.drawEllipse(d, 3.8, 3.8);
        }
    };
    trace(true, QColor{0, 128, 0});
    trace(false, QColor{225, 0, 0});
    p.restore();
    // the labels: pressure on the left, temperature along the foot (the isotherm at the bottom)
    p.setPen(QColor{30, 30, 30});
    for (double pr = std::ceil(pTop / 100.0) * 100.0; pr <= pBottom; pr += 100.0) {
        p.drawText(QRectF{area.left() - 46, yOf(pr) - 8, 40, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(pr)));
    }
    for (double t = std::ceil(xLow / 10.0) * 10.0; t <= xHigh - skew * 0.0; t += 10.0) {
        const double x = xOf(t, pBottom);
        if (x >= area.left() - 1 && x <= area.right() + 1) {
            p.drawText(QRectF{x - 20, area.bottom() + 3, 40, 16}, Qt::AlignHCenter, QString::number(static_cast<int>(t)));
        }
    }
    p.drawText(QRectF{area.left(), area.bottom() + 20, area.width(), 18}, Qt::AlignHCenter, "Temperature (C)");
    p.save();
    p.translate(14, area.center().y());
    p.rotate(-90);
    p.drawText(QRectF{-80, -8, 160, 16}, Qt::AlignHCenter, "Pressure (hPa)");
    p.restore();
    // the wind barbs, in a column beside the plot
    const double barbX = area.right() + 34.0;
    p.setPen(QPen{QColor{20, 20, 20}, 1.3});
    p.setBrush(QColor{20, 20, 20});
    for (const auto * l : winds) {
        if (l->pressure < pTop) {
            continue;
        }
        WindBarb::draw(p, QPointF{barbX, yOf(l->pressure)}, l->windDirection, l->windSpeed, 28.0);
    }

    // ---- the hodograph ----
    const double hodoSize = exportMode ? std::min(w * 0.40, h - 150.0) : std::min(w * 0.27, h * 0.40);
    const QRectF hodo{w * 0.545, exportMode ? 80.0 : 60.0, hodoSize, hodoSize};
    double fastest = 20.0;
    for (const auto * l : winds) {
        if (l->pressure >= 400.0) {
            fastest = std::max(fastest, l->windSpeed);
        }
    }
    const double reach = std::ceil(fastest / 10.0) * 10.0 + (fastest < 40 ? 0.0 : 10.0);
    const QPointF center = hodo.center();
    const double scale = hodo.width() / 2.0 / reach;
    p.setPen(QPen{QColor{190, 190, 190}, 1.0});
    p.setBrush(Qt::NoBrush);
    const double ringStep = reach > 60 ? 20.0 : 10.0;
    for (double r = ringStep; r <= reach + 0.01; r += ringStep) {
        p.drawEllipse(center, r * scale, r * scale);
    }
    p.setPen(QPen{QColor{110, 110, 110}, 1.0});
    p.drawLine(QPointF{hodo.left(), center.y()}, QPointF{hodo.right(), center.y()});
    p.drawLine(QPointF{center.x(), hodo.top()}, QPointF{center.x(), hodo.bottom()});
    QFont small{base};
    small.setPixelSize(10);
    p.setFont(small);
    p.setPen(QColor{110, 110, 110});
    for (double r = ringStep; r <= reach + 0.01; r += ringStep) {
        p.drawText(QPointF{center.x() + r * scale + 2, center.y() - 3}, QString::number(static_cast<int>(r)));
    }
    p.drawText(QPointF{center.x() + 3, hodo.top() + 10}, "N");
    p.setFont(base);
    // the wind vector's tip at each level, the surface first: u east, v north (the wind comes from its direction)
    QPointF previous;
    bool havePrevious = false;
    double previousPressure = 0.0;
    std::vector<std::pair<QPointF, double>> tips;
    for (const auto * l : winds) {
        if (l->pressure < 400.0) {
            continue;
        }
        const double rad = l->windDirection * std::numbers::pi / 180.0;
        const double u = -l->windSpeed * std::sin(rad);
        const double v = -l->windSpeed * std::cos(rad);
        const QPointF at{center.x() + u * scale, center.y() - v * scale};
        if (havePrevious) {
            p.setPen(QPen{bandColor((l->pressure + previousPressure) / 2.0), 3.0, Qt::SolidLine, Qt::RoundCap});
            p.drawLine(previous, at);
        }
        previous = at;
        previousPressure = l->pressure;
        havePrevious = true;
        tips.emplace_back(at, l->pressure);
    }
    p.setFont(small);
    for (const auto& [at, pressure] : tips) {
        p.setPen(QPen{QColor{30, 30, 30}, 1.0});
        p.setBrush(bandColor(pressure));
        p.drawEllipse(at, 4.0, 4.0);
    }
    // the pressure beside the mandatory levels and the surface
    for (size_t i = 0; i < winds.size(); i++) {
        const auto * l = winds[i];
        const bool mandatory = has(l->height) || i == 0;
        if (!mandatory || l->pressure < 400.0) {
            continue;
        }
        for (const auto& [at, pressure] : tips) {
            if (pressure == l->pressure) {
                p.setPen(QColor{30, 30, 30});
                p.drawText(at + QPointF{6, -5}, QString::number(static_cast<int>(l->pressure)));
            }
        }
    }
    // the mean boundary layer wind of the report, a diamond
    if (has(drop.mblSpeed) && has(drop.mblDirection)) {
        const double rad = drop.mblDirection * std::numbers::pi / 180.0;
        const QPointF at{center.x() - drop.mblSpeed * std::sin(rad) * scale, center.y() + drop.mblSpeed * std::cos(rad) * scale};
        p.setPen(QPen{QColor{30, 30, 30}, 1.2});
        p.setBrush(QColor{255, 255, 255});
        p.drawPolygon(QPolygonF{{at + QPointF{0, -6}, at + QPointF{6, 0}, at + QPointF{0, 6}, at + QPointF{-6, 0}}});
    }
    p.setFont(bold);
    p.setPen(QColor{30, 30, 30});
    p.drawText(QPointF{hodo.left(), hodo.top() - 6}, "Hodograph (kt)");
    p.setFont(small);
    double lx = hodo.left();
    const double ly = hodo.bottom() + 16;
    const auto key = [&] (const QColor& color, const QString& label) {
        p.setPen(QPen{color, 3.0});
        p.drawLine(QPointF{lx, ly}, QPointF{lx + 16, ly});
        p.setPen(QColor{60, 60, 60});
        p.drawText(QPointF{lx + 20, ly + 4}, label);
        lx += 26 + QFontMetricsF{small}.horizontalAdvance(label);
    };
    key(bandColor(1000), "surface-925");
    key(bandColor(900), "925-850");
    key(bandColor(780), "850-700");
    key(bandColor(500), "above 700");
    if (has(drop.mblSpeed) && !exportMode) {
        p.setPen(QColor{60, 60, 60});
        p.drawText(QPointF{hodo.left(), ly + 18}, "diamond: the report's mean boundary layer wind");
    }
    p.setFont(base);

    // ---- the tables ----
    if (exportMode) {
        return;
    }
    const double tableTop = hodo.bottom() + 40;
    const double col = w * 0.545;
    const auto cell = [&] (double x, double y, double width, const QString& text, const QColor& fill, bool boldText = false) {
        p.setPen(QColor{80, 80, 80});
        p.setBrush(fill);
        p.drawRect(QRectF{x, y, width, 16});
        QFont f{small};
        f.setBold(boldText);
        p.setFont(f);
        p.setPen(QColor{20, 20, 20});
        p.drawText(QRectF{x, y, width, 16}, Qt::AlignCenter, text);
    };
    // table 1: the mandatory levels (those with a height), the surface last
    {
        const double widths[] = {70, 52, 52, 40, 88};
        const char * heads[] = {"Level", "Height", "Temp", "RH", "Wind"};
        double y = tableTop;
        double x = col;
        for (int i = 0; i < 5; i++) {
            cell(x, y, widths[i], heads[i], QColor{245, 245, 245}, true);
            x += widths[i];
        }
        y += 16;
        std::vector<const UtilityDropsonde::Level *> rows;
        for (const auto& l : drop.levels) {
            if (has(l.height) && l.height >= 0.0 && l.pressure >= 400.0 && (!has(drop.surfacePressure) || l.pressure <= drop.surfacePressure + 0.5) && &l != surfaceLevel) {   // none below the ground
                rows.push_back(&l);
            }
        }
        std::reverse(rows.begin(), rows.end());
        if (surfaceLevel != nullptr) {
            rows.push_back(surfaceLevel);
        }
        for (const auto * l : rows) {
            const bool isSurface = l == surfaceLevel;
            x = col;
            const QString level = QString::number(static_cast<int>(std::lround(l->pressure))) + " mb" + (isSurface ? " (sfc)" : "");
            const double rh = relativeHumidity(l->temperature, l->dewPoint);
            const QString cells[5] = {level, has(l->height) ? QString::number(static_cast<int>(l->height)) + " m" : (isSurface ? QString{"0 m"} : QString{"N/A"}),
                                      has(l->temperature) ? QString::number(l->temperature, 'f', 1) + " C" : QString{"N/A"}, has(rh) ? QString::number(static_cast<int>(std::lround(rh))) + "%" : QString{"N/A"},
                                      windText(l->windDirection, l->windSpeed)};
            for (int i = 0; i < 5; i++) {
                QColor fill{255, 255, 255};
                if (i == 2 && has(l->temperature)) {
                    fill = QColor{255, 170, 150};
                } else if (i == 3 && has(rh)) {
                    fill = rh > 90 ? QColor{150, 190, 170} : QColor{215, 225, 215};
                }
                cell(x, y, widths[i], cells[i], fill);
                x += widths[i];
            }
            y += 16;
        }
        p.setFont(base);
        p.setPen(QColor{40, 40, 40});
        p.drawText(QPointF{col, y + 16}, "Mandatory level data");
        // table 2: the winds below 700 mb, beside the first
        const double x2 = col + 312.0;
        double y2 = tableTop;
        cell(x2, y2, 82, "Level", QColor{245, 245, 245}, true);
        cell(x2 + 82, y2, 92, "Wind below 700", QColor{245, 245, 245}, true);
        y2 += 16;
        std::vector<const UtilityDropsonde::Level *> low;
        for (const auto * l : winds) {
            if (l->pressure >= 700.0) low.push_back(l);
        }
        std::reverse(low.begin(), low.end());   // the highest level first, as the surface ends the list
        const size_t maxRows = static_cast<size_t>(std::max(0.0, (h - y2 - 110) / 16.0));
        for (size_t i = 0; i < low.size() && i < maxRows; i++) {
            const bool isSurface = low[i] == surfaceLevel;
            cell(x2, y2, 82, QString::number(static_cast<int>(std::lround(low[i]->pressure))) + " mb" + (isSurface ? " sfc" : ""), QColor{255, 255, 255});
            cell(x2 + 82, y2, 92, windText(low[i]->windDirection, low[i]->windSpeed), QColor{235, 235, 245});
            y2 += 16;
        }
        y2 = std::max(y2, y + 34.0);
        // the summary lines
        p.setFont(base);
        p.setPen(QColor{30, 30, 30});
        double y3 = y2 + 14;
        const auto line = [&] (const QString& text) {
            p.drawText(QPointF{col, y3}, text);
            y3 += 19;
        };
        double direction = 0.0, speed = 0.0;
        line("Mean wind in the lowest 500 m:  " + (meanWind(drop, 500.0, direction, speed) ? windText(direction, speed) : QString{"N/A"}));
        line("Mean wind in the lowest 150 m:  " + (meanWind(drop, 150.0, direction, speed) ? windText(direction, speed) : QString{"N/A"}));
        const UtilityDropsonde::Level * strongest = nullptr;
        for (const auto * l : winds) {
            if (strongest == nullptr || l->windSpeed > strongest->windSpeed) strongest = l;
        }
        line("Strongest wind in the sounding:  " + (strongest != nullptr ? windText(strongest->windDirection, strongest->windSpeed) + " at " + QString::number(static_cast<int>(std::lround(strongest->pressure))) + " mb" : QString{"N/A"}));
        if (has(drop.mblSpeed)) {
            line("Mean boundary layer wind (reported):  " + windText(drop.mblDirection, drop.mblSpeed));
        }
    }
    p.setPen(QColor{110, 110, 110});
    p.setFont(small);
    p.drawText(QPointF{14, h - 8}, "NHC / reconnaissance dropsonde report (WMO TEMP DROP). Winds in knots, from the direction shown; as reported.");
}

DropsondeViewer::DropsondeViewer(Window * parent, const UtilityDropsonde::Drop& d)
    : Window{parent}
    , buttonFull{this, None, "Full sounding analysis..."}
    , drop{d}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Dropsonde " + drop.mission + "  " + UtilityHdob::timeText(drop.seconds));
    chart = new DropsondeChart{drop, this};
    buttonFull.connect([this] {
        SoundingProfile profile;
        std::string error;
        if (DropProfile::build(drop, profile, error)) {
            new SoundingViewer{this, profile, DropProfile::title(drop)};
        }
    });
    row.addWidget(buttonFull);
    row.addStretch();
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.addLayout(row);
    box.getAndShow(this);
    resize(1100, 720);
}
