// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/ShipsViewer.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include "hurricane/ChartKit.h"
#include "hurricane/Coast.h"

namespace {
    constexpr double none = UtilityShips::missing;

    bool have(double v) {
        return UtilityShips::has(v);
    }

    QColor categoryColor(double wind) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 200, 200}, QColor{235, 235, 120}, QColor{255, 220, 90}, QColor{255, 175, 50},
                                        QColor{255, 130, 20}, QColor{255, 80, 80}};
        return colors[UtilityAtcf::categoryOf(static_cast<int>(wind))];
    }

    // the last forecast hour any of the rows has a value for
    int lastHour(const UtilityShips::Ships& s) {
        int last = 48;
        for (const char * label : {"V (KT) LAND", "V (KT) NO LAND", "SHEAR (KT)", "SST (C)"}) {
            if (const auto * row = s.row(label)) {
                for (size_t i = 0; i < row->size() && i < s.hours.size(); i++) {
                    if (have((*row)[i])) {
                        last = std::max(last, s.hours[i]);
                    }
                }
            }
        }
        return last;
    }

    // peak of a row: value and the hour
    std::pair<double, int> peak(const UtilityShips::Ships& s, const char * label) {
        std::pair<double, int> best{none, 0};
        if (const auto * row = s.row(label)) {
            for (size_t i = 0; i < row->size() && i < s.hours.size(); i++) {
                if (have((*row)[i]) && (!have(best.first) || (*row)[i] > best.first)) {
                    best = {(*row)[i], s.hours[i]};
                }
            }
        }
        return best;
    }
}

QString ShipsChart::summary(const UtilityShips::Ships& s) {
    QString text = "SHIPS " + QString::fromStdString(UtilityAtcf::formatTime(s.cycle)) + ":";
    const auto land = peak(s, "V (KT) LAND");
    const auto lgem = peak(s, "V (KT) LGEM");
    if (have(land.first)) {
        text += " peak " + QString::fromStdString(UtilityAtcf::windLabel(static_cast<int>(land.first))) + " at " + QString::number(land.second) + " h";
    }
    if (have(lgem.first)) {
        text += " (LGEM " + QString::fromStdString(UtilityAtcf::windLabel(static_cast<int>(lgem.first))) + ")";
    }
    const auto first = [&] (const char * label) {
        const auto * row = s.row(label);
        return row != nullptr && !row->empty() ? row->front() : none;
    };
    if (have(first("SHEAR (KT)"))) {
        text += ", shear " + QString::number(static_cast<int>(first("SHEAR (KT)"))) + " kt";
    }
    if (have(first("SST (C)"))) {
        text += ", SST " + QString::number(first("SST (C)"), 'f', 1) + " C";
    }
    for (const auto& p : s.riLines) {
        if (p.knots == 30 && p.hours == 24) {
            text += ", RI 30 kt / 24 h: " + QString::number(static_cast<int>(p.percent)) + " % (" + QString::number(p.times, 'f', 1) + " x normal)";
        }
    }
    return text;
}

void ShipsChart::setData(const std::shared_ptr<HurricaneData::ShipsData>& newShips, const std::shared_ptr<HurricaneData::StormData>& newStorm) {
    ships = newShips;
    storm = newStorm;
    update();
}

void ShipsChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{250, 250, 250});
    if (!ships || !ships->ships.ok) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, ships ? QString::fromStdString(ships->error) : QString{"No SHIPS forecast"});
        return;
    }
    const auto& s = ships->ships;
    const double leftWidth = std::max(330.0, width() * 0.40);
    const double rightLeft = leftWidth + 40.0;
    const int hourMax = lastHour(s);
    const double xStep = hourMax > 120 ? 24.0 : 12.0;

    // ---- left: title, the storm now, the RI table, the locator map
    QFont title{p.font()};
    title.setPixelSize(18);
    title.setBold(true);
    p.setFont(title);
    p.setPen(QColor{20, 20, 20});
    p.drawText(QRectF{0, 6, leftWidth, 26}, Qt::AlignHCenter, QString::fromStdString("SHIPS forecast for " + s.id + " " + s.name));
    QFont normal{p.font()};
    normal.setPixelSize(12);
    normal.setBold(false);
    p.setFont(normal);
    p.setPen(QColor{60, 60, 60});
    const auto cycle = QString::fromStdString(s.cycle);
    p.drawText(QRectF{0, 34, leftWidth, 18}, Qt::AlignHCenter, "Initialized " + QString::fromStdString(UtilityAtcf::formatTime(s.cycle)) + " " + cycle.left(4));
    const auto * lat = s.row("LAT (DEG N)");
    const auto * lon = s.row("LONG(DEG W)");
    const auto * vNow = s.row("V (KT) LAND");
    if (lat != nullptr && lon != nullptr && vNow != nullptr && !lat->empty()) {
        p.setPen(QColor{40, 40, 120});
        p.drawText(QRectF{0, 54, leftWidth, 18}, Qt::AlignHCenter, "Now: " + QString::number((*lat)[0], 'f', 1) + " N, " + QString::number((*lon)[0], 'f', 1) + " W, " +
                                                                  QString::fromStdString(UtilityAtcf::windLabel(static_cast<int>((*vNow)[0]))));
    }
    p.setPen(QColor{20, 20, 20});
    QFont bold = normal;
    bold.setBold(true);
    p.setFont(bold);
    p.drawText(QRectF{0, 84, leftWidth, 18}, Qt::AlignHCenter, "Rapid intensification (RI) probabilities");
    // the table: threshold, SHIPS-RII percent shaded by size, how many times the usual chance
    if (!s.riLines.empty()) {
        const double cell = (leftWidth - 16.0) / static_cast<double>(s.riLines.size());
        const double top = 106.0;
        QFont small = normal;
        small.setPixelSize(10);
        for (size_t i = 0; i < s.riLines.size(); i++) {
            const auto& r = s.riLines[i];
            const double x = 8.0 + static_cast<double>(i) * cell;
            const QRectF head{x, top, cell, 34.0};
            const QRectF value{x, top + 34.0, cell, 28.0};
            const QRectF ratio{x, top + 62.0, cell, 34.0};
            p.setPen(QColor{30, 30, 30});
            p.setBrush(QColor{140, 185, 225});
            p.drawRect(head);
            // shade by how likely: white to red up to 40 %
            const double f = std::clamp(r.percent / 40.0, 0.0, 1.0);
            p.setBrush(QColor{255, static_cast<int>(255 - 130 * f), static_cast<int>(255 - 160 * f)});
            p.drawRect(value);
            p.setBrush(r.times >= 2.0 ? QColor{255, 235, 190} : QColor{255, 255, 255});
            p.drawRect(ratio);
            p.setFont(small);
            p.drawText(head, Qt::AlignCenter, QString::number(r.knots) + " kt/\n" + QString::number(r.hours) + " h");
            p.setFont(bold);
            p.drawText(value, Qt::AlignCenter, QString::number(static_cast<int>(r.percent)) + "%");
            p.setFont(small);
            p.drawText(ratio, Qt::AlignCenter, QString::number(r.times, 'f', 1) + "x\nnormal");
        }
        p.setFont(small);
        p.setPen(QColor{80, 80, 80});
        double y = top + 112.0;
        if (have(s.preliminaryRi)) {
            p.drawText(QPointF{8.0, y}, "Preliminary RI probability (35 kt in 36 h): " + QString::number(s.preliminaryRi, 'f', 1) + " %");
            y += 14.0;
        }
        // the other methods' numbers for a 30 kt / 24 h rise
        for (size_t t = 0; t < s.riThresholds.size(); t++) {
            if (s.riThresholds[t] == "30/24") {
                QString line = "30 kt / 24 h by method:";
                for (const auto& [name, values] : s.riMatrix) {
                    if (t < values.size() && have(values[t])) {
                        line += "  " + QString::fromStdString(name) + " " + QString::number(values[t], 'f', 0) + "%";
                    }
                }
                p.drawText(QRectF{8.0, y - 10.0, leftWidth - 8.0, 32.0}, Qt::TextWordWrap, line);
                y += 34.0;
            }
        }
        // the locator map: the SHIPS forecast positions on the coastlines
        if (lat != nullptr && lon != nullptr) {
            double minLat = 90, maxLat = -90, minLon = 180, maxLon = -180;
            for (size_t i = 0; i < lat->size(); i++) {
                if (have((*lat)[i]) && have((*lon)[i])) {
                    minLat = std::min(minLat, (*lat)[i]);
                    maxLat = std::max(maxLat, (*lat)[i]);
                    minLon = std::min(minLon, -(*lon)[i]);
                    maxLon = std::max(maxLon, -(*lon)[i]);
                }
            }
            if (minLat <= maxLat) {
                const double pad = std::max(2.0, std::max(maxLat - minLat, maxLon - minLon) * 0.2);
                minLat -= pad;
                maxLat += pad;
                minLon -= pad;
                maxLon += pad;
                const QRectF map{8.0, y + 6.0, leftWidth - 16.0, std::max(120.0, height() - y - 24.0)};
                // keep degrees square on the screen (east-west shrunk by the cosine of the middle latitude)
                const double kx = std::cos((minLat + maxLat) / 2.0 * 3.14159265 / 180.0);
                const double spanX = (maxLon - minLon) * kx;
                const double spanY = maxLat - minLat;
                const double scale = std::min(map.width() / spanX, map.height() / spanY);
                const QPointF center{(minLon + maxLon) / 2.0, (minLat + maxLat) / 2.0};
                const auto at = [&] (double la, double lo) {
                    return QPointF{map.center().x() + (lo - center.x()) * kx * scale, map.center().y() - (la - center.y()) * scale};
                };
                p.save();
                p.setClipRect(map);
                p.fillRect(map, QColor{228, 238, 248});
                p.setPen(QPen{QColor{120, 120, 120}, 0.8});
                p.setBrush(Qt::NoBrush);
                for (const auto& line : Coast::lines()) {
                    QPainterPath path;
                    bool started = false;
                    for (const auto& [lo, la] : line) {
                        if (lo < minLon - 5 || lo > maxLon + 5 || la < minLat - 5 || la > maxLat + 5) {
                            started = false;
                            continue;
                        }
                        const auto pt = at(la, lo);
                        started ? path.lineTo(pt) : path.moveTo(pt);
                        started = true;
                    }
                    p.drawPath(path);
                }
                // the track, a dot every 12 hours in the intensity colour, hour labels
                p.setPen(QPen{QColor{40, 40, 40}, 1.2});
                QPainterPath track;
                bool started = false;
                for (size_t i = 0; i < lat->size(); i++) {
                    if (!have((*lat)[i]) || !have((*lon)[i])) {
                        continue;
                    }
                    const auto pt = at((*lat)[i], -(*lon)[i]);
                    started ? track.lineTo(pt) : track.moveTo(pt);
                    started = true;
                }
                p.drawPath(track);
                QFont tiny = normal;
                tiny.setPixelSize(9);
                p.setFont(tiny);
                for (size_t i = 0; i < lat->size(); i++) {
                    if (!have((*lat)[i]) || !have((*lon)[i]) || (s.hours[i] % 12) != 0) {
                        continue;
                    }
                    const auto pt = at((*lat)[i], -(*lon)[i]);
                    const double wind = vNow != nullptr && have((*vNow)[i]) ? (*vNow)[i] : 0.0;
                    p.setPen(QPen{QColor{20, 20, 20}, 1.0});
                    p.setBrush(categoryColor(wind));
                    p.drawEllipse(pt, 5.0, 5.0);
                    p.setPen(QColor{20, 20, 20});
                    p.drawText(pt + QPointF{7.0, 3.0}, QString::number(s.hours[i]) + " h");
                }
                p.restore();
                p.setPen(QColor{30, 30, 30});
                p.setBrush(Qt::NoBrush);
                p.drawRect(map);
            }
        }
    }

    // ---- right: strips against forecast hour
    struct Strip {
        const char * label;
        const char * unit;
        QColor color;
    };
    const double top = 8.0;
    const double bottom = height() - 40.0;
    const int strips = 6;
    const double gap = 6.0;
    const double each = (bottom - top - gap * (strips - 1)) / strips;
    int index = 0;
    const auto stripAxes = [&] (double lo, double hi) {
        const QRectF area{rightLeft, top + index * (each + gap), width() - rightLeft - 12.0, each};
        index++;
        return ChartKit::Axes{area, 0.0, static_cast<double>(hourMax), lo, hi};
    };
    // a series of a row, drawn as a line (gaps end it)
    const auto drawRow = [&] (const ChartKit::Axes& a, const char * label, const QPen& pen) {
        const auto * row = s.row(label);
        if (row == nullptr) {
            return;
        }
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        QPainterPath path;
        bool started = false;
        for (size_t i = 0; i < row->size() && i < s.hours.size(); i++) {
            if (!have((*row)[i]) || s.hours[i] > hourMax) {
                started = false;
                continue;
            }
            const auto pt = a.at(s.hours[i], (*row)[i]);
            started ? path.lineTo(pt) : path.moveTo(pt);
            started = true;
        }
        p.drawPath(path);
    };
    const auto range = [&] (const char * label, double padFraction, double floorLow, double minSpan) {
        double lo = 1e9;
        double hi = -1e9;
        if (const auto * row = s.row(label)) {
            for (size_t i = 0; i < row->size() && i < s.hours.size(); i++) {
                if (have((*row)[i]) && s.hours[i] <= hourMax) {
                    lo = std::min(lo, (*row)[i]);
                    hi = std::max(hi, (*row)[i]);
                }
            }
        }
        if (lo > hi) {
            lo = 0;
            hi = minSpan;
        }
        const double span = std::max(hi - lo, minSpan);
        return std::pair<double, double>{std::max(floorLow, lo - span * padFraction), hi + span * padFraction};
    };
    const auto strip = [&] (const char * title, const char * unit, const ChartKit::Axes& a, double yStep, bool lastStrip) {
        p.setPen(QColor{210, 210, 210});
        p.setBrush(QColor{255, 255, 255});
        p.drawRect(a.area);
        QFont small = normal;
        small.setPixelSize(10);
        p.setFont(small);
        for (double y = std::ceil(a.yMin / yStep) * yStep; y <= a.yMax + 1e-9; y += yStep) {
            const auto at = a.at(0, y);
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{a.area.left(), at.y()}, QPointF{a.area.right(), at.y()});
            p.setPen(QColor{80, 80, 80});
            p.drawText(QRectF{a.area.left() - 38, at.y() - 7, 34, 14}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', yStep < 1.0 ? 1 : 0));
        }
        for (double x = 0; x <= a.xMax + 1e-9; x += xStep) {
            const auto at = a.at(x, a.yMin);
            p.setPen(QColor{232, 232, 232});
            p.drawLine(QPointF{at.x(), a.area.top()}, QPointF{at.x(), a.area.bottom()});
            if (lastStrip) {
                p.setPen(QColor{80, 80, 80});
                p.drawText(QRectF{at.x() - 20, a.area.bottom() + 2, 40, 14}, Qt::AlignHCenter, QString::number(static_cast<int>(x)));
            }
        }
        p.setPen(QColor{50, 50, 50});
        p.drawText(QRectF{a.area.left() + 6, a.area.top() + 2, a.area.width() - 12, 14}, Qt::AlignRight, QString{title} + (unit[0] != 0 ? QString{" ("} + unit + ")" : QString{}));
    };

    // 1: maximum wind
    {
        const double top1 = 10.0;
        auto [lo, hi] = range("V (KT) LAND", 0.1, 10.0, 30.0);
        double high = hi;
        for (const char * l : {"V (KT) NO LAND", "V (KT) LGEM"}) {
            high = std::max(high, range(l, 0.1, 0.0, 30.0).second);
        }
        if (storm && !storm->official.fixes.empty()) {
            for (const auto& f : storm->official.fixes) {
                high = std::max(high, static_cast<double>(f.wind));
            }
        }
        const auto a = stripAxes(std::max(10.0, std::floor(lo / 10.0) * 10.0 - 10.0), std::ceil((std::max(high, top1) + 5.0) / 10.0) * 10.0);
        strip("Max wind", "kt", a, (a.yMax - a.yMin) > 80 ? 20.0 : 10.0, false);
        ChartKit::categoryLines(p, a);
        drawRow(a, "V (KT) NO LAND", QPen{QColor{150, 150, 150}, 2.0});
        drawRow(a, "V (KT) LGEM", QPen{QColor{40, 130, 200}, 1.6, Qt::DashLine});
        drawRow(a, "V (KT) LAND", QPen{QColor{20, 20, 20}, 2.2});
        if (storm && !storm->official.fixes.empty()) {
            const double offset = UtilityAtcf::hoursBetween(s.cycle, storm->official.cycle);
            p.setPen(QPen{QColor{20, 20, 20}, 1.4});
            p.setBrush(QColor{255, 255, 255});
            for (const auto& f : storm->official.fixes) {
                if (f.wind >= 0 && offset + f.tau >= 0 && offset + f.tau <= hourMax) {
                    p.drawEllipse(a.at(offset + f.tau, f.wind), 3.2, 3.2);
                }
            }
        }
        const auto best = peak(s, "V (KT) LAND");
        if (have(best.first)) {
            const auto pt = a.at(best.second, best.first);
            p.setPen(QPen{QColor{200, 40, 40}, 1.5});
            p.setBrush(QColor{255, 200, 200});
            p.drawEllipse(pt, 4.5, 4.5);
            QFont small = normal;
            small.setPixelSize(10);
            p.setFont(small);
            p.setPen(QColor{160, 20, 20});
            p.drawText(pt + QPointF{8.0, -6.0}, QString::fromStdString(UtilityAtcf::windLabel(static_cast<int>(best.first))) + ", " + QString::number(best.second) + " h");
        }
        // the key
        p.setFont(normal);
        QFont small = normal;
        small.setPixelSize(10);
        p.setFont(small);
        double kx = a.area.left() + 6;
        const double ky = a.area.top() + 12;
        const auto key = [&] (const QColor& c, const QString& text, Qt::PenStyle style) {
            p.setPen(QPen{c, 2.2, style});
            p.drawLine(QPointF{kx, ky - 3}, QPointF{kx + 14, ky - 3});
            p.setPen(QColor{50, 50, 50});
            p.drawText(QPointF{kx + 18, ky}, text);
            kx += 28 + QFontMetricsF{small}.horizontalAdvance(text);
        };
        key(QColor{20, 20, 20}, "SHIPS", Qt::SolidLine);
        key(QColor{150, 150, 150}, "no land", Qt::SolidLine);
        key(QColor{40, 130, 200}, "LGEM", Qt::DashLine);
        p.setPen(QColor{50, 50, 50});
        p.drawText(QPointF{kx, ky}, "o NHC");
    }
    // 2: potential intensity
    {
        const auto r = range("POT. INT. (KT)", 0.1, 0.0, 40.0);
        const auto a = stripAxes(std::floor(r.first / 20.0) * 20.0, std::ceil(r.second / 20.0) * 20.0);
        strip("Max potential intensity", "kt", a, 20.0, false);
        drawRow(a, "POT. INT. (KT)", QPen{QColor{210, 40, 40}, 2.0});
    }
    // 3: shear
    {
        const auto r = range("SHEAR (KT)", 0.1, 0.0, 20.0);
        const auto a = stripAxes(0.0, std::max(30.0, std::ceil(r.second / 10.0) * 10.0));
        strip("Vertical wind shear", "kt", a, 10.0, false);
        // above 20 kt shear is usually hostile: a light band
        p.fillRect(QRectF{a.area.left(), a.at(0, a.yMax).y(), a.area.width(), a.at(0, 20.0).y() - a.at(0, a.yMax).y()}, QColor{255, 230, 230, 140});
        p.setPen(QPen{QColor{190, 90, 90}, 1.0, Qt::DotLine});
        p.drawLine(a.at(0, 20.0), a.at(hourMax, 20.0));
        drawRow(a, "SHEAR (KT)", QPen{QColor{30, 60, 220}, 2.0});
    }
    // 4: sea surface temperature
    {
        const auto r = range("SST (C)", 0.1, 10.0, 4.0);
        const auto a = stripAxes(std::floor(std::min(r.first, 24.0)), std::ceil(r.second));
        strip("Sea surface temperature", "C", a, 2.0, false);
        if (26.5 > a.yMin && 26.5 < a.yMax) {
            p.setPen(QPen{QColor{190, 120, 20}, 1.0, Qt::DotLine});
            p.drawLine(a.at(0, 26.5), a.at(hourMax, 26.5));
        }
        drawRow(a, "SST (C)", QPen{QColor{240, 150, 0}, 2.0});
    }
    // 5: humidity
    {
        const auto r = range("700-500 MB RH", 0.1, 0.0, 20.0);
        const auto a = stripAxes(std::max(0.0, std::floor(r.first / 10.0) * 10.0), std::min(100.0, std::ceil(r.second / 10.0) * 10.0));
        strip("700-500 mb relative humidity", "%", a, 10.0, false);
        drawRow(a, "700-500 MB RH", QPen{QColor{20, 140, 40}, 2.0});
    }
    // 6: ocean heat content
    {
        const auto r = range("HEAT CONTENT", 0.1, 0.0, 40.0);
        const auto a = stripAxes(0.0, std::ceil(r.second / 20.0) * 20.0);
        strip("Ocean heat content", "kJ/cm2", a, 20.0, true);
        drawRow(a, "HEAT CONTENT", QPen{QColor{130, 40, 160}, 2.0});
    }
    p.setPen(QColor{90, 90, 90});
    QFont small = normal;
    small.setPixelSize(10);
    p.setFont(small);
    p.drawText(QPointF{rightLeft, height() - 6.0}, "Forecast hour.  SHIPS (NHC statistical-dynamical intensity model, atcf/stext): guidance, not an official forecast.");
}

ShipsViewer::ShipsViewer(Window * parent, const std::shared_ptr<HurricaneData::ShipsData>& ships, const std::shared_ptr<HurricaneData::StormData>& storm)
    : Window{parent}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("SHIPS - intensity forecast, environment and rapid intensification - " + HurricaneData::idLabel(storm->id) + " " + storm->name);
    textStatus.setWordWrap(false);
    chart = new ShipsChart{this};
    box.addWidget(textStatus);
    box.addWidgetReal(chart, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1180, 760);
    chart->setData(ships, storm);
    textStatus.setText(ships->ships.ok ? ShipsChart::summary(ships->ships) : QString::fromStdString(ships->error));
}
