// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/VdmViewer.h"
#include <algorithm>
#include <cmath>
#include <QPainter>
#include <QPainterPath>
#include <QTextBrowser>
#include "hurricane/ChartKit.h"
#include "hurricane/UtilityHdob.h"

namespace {
    QString q(const std::string& s) {
        return QString::fromStdString(s).toHtmlEscaped();
    }

    QString number(double v, int digits = 0) {
        return UtilityVdm::has(v) ? QString::number(v, 'f', digits) : QString{"-"};
    }

    QString wind(const UtilityVdm::Wind& w) {
        if (!UtilityVdm::has(w.kt)) {
            return "-";
        }
        QString text = QString::number(static_cast<int>(w.kt)) + " kt";
        if (UtilityVdm::has(w.bearing)) {
            text += " at " + QString::number(static_cast<int>(w.bearing)) + " deg / " + QString::number(w.rangeNm, 'f', 0) + " nm";
        }
        return text;
    }
}

QString VdmViewer::timeText(long seconds) {
    return QString::fromStdString(UtilityHdob::timeText(seconds));
}

QString VdmViewer::summary(const HurricaneData::VdmData& data) {
    if (data.messages.empty()) {
        return "no vortex messages yet";
    }
    const auto& last = data.messages.back();
    QString text = QString::number(data.messages.size()) + " vortex messages, latest " + timeText(last.seconds);
    if (UtilityVdm::has(last.pressure)) {
        text += ": " + QString::number(static_cast<int>(last.pressure)) + " mb" + (last.extrapolated ? " (extrapolated)" : "");
    }
    if (UtilityVdm::has(last.maxFlightWind())) {
        text += ", max flight-level wind " + QString::number(static_cast<int>(last.maxFlightWind())) + " kt";
    }
    return text;
}

void VdmChart::setData(const std::shared_ptr<HurricaneData::VdmData>& newData) {
    data = newData;
    update();
}

void VdmChart::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor{245, 245, 245});
    if (!data || data->messages.empty()) {
        p.setPen(QColor{100, 100, 100});
        p.drawText(rect(), Qt::AlignCenter, "No vortex messages for this storm yet");
        return;
    }
    const auto& messages = data->messages;
    // time axis: hours from the first fix, at least 6
    const long first = messages.front().seconds;
    double span = std::max(6.0, static_cast<double>(messages.back().seconds - first) / 3600.0);
    const double marginLeft = 50.0;
    const double width = this->width() - marginLeft - 16.0;
    const double height = (this->height() - 70.0 - 28.0) / 2.0;
    double pMin = 1020.0;
    double pMax = 940.0;
    double wMax = 40.0;
    for (const auto& m : messages) {
        if (UtilityVdm::has(m.pressure)) {
            pMin = std::min(pMin, m.pressure);
            pMax = std::max(pMax, m.pressure);
        }
        if (UtilityVdm::has(m.maxFlightWind())) {
            wMax = std::max(wMax, m.maxFlightWind());
        }
    }
    pMin = std::floor((pMin - 4.0) / 10.0) * 10.0;
    pMax = std::ceil((pMax + 2.0) / 10.0) * 10.0;
    pMax = std::max(pMax, pMin + 20.0);
    wMax = std::ceil((wMax + 5.0) / 20.0) * 20.0;
    const ChartKit::Axes pressure{QRectF{marginLeft, 22.0, width, height}, 0.0, span, pMin, pMax};
    const ChartKit::Axes winds{QRectF{marginLeft, 22.0 + height + 30.0, width, height}, 0.0, span, 0.0, wMax};
    const double xStep = span > 72 ? 24.0 : span > 30 ? 12.0 : 6.0;
    ChartKit::frame(p, pressure, "Minimum sea-level pressure at each fix", "mb", (pMax - pMin) > 80 ? 20.0 : 10.0, xStep);
    ChartKit::frame(p, winds, "Strongest flight-level wind at each fix", "kt", wMax > 100 ? 40.0 : 20.0, xStep);
    const auto at = [&] (const ChartKit::Axes& a, const UtilityVdm::Vdm& m, double y) { return a.at(static_cast<double>(m.seconds - first) / 3600.0, y); };
    // a line through the fixes, a dot at each: solid for a dropsonde pressure, hollow for one extrapolated from the flight level
    const auto series = [&] (const ChartKit::Axes& a, auto value, const QColor& color, bool hollowWhenExtrapolated) {
        p.setPen(QPen{color, 1.6});
        const UtilityVdm::Vdm * previous = nullptr;
        double previousValue = 0.0;
        for (const auto& m : messages) {
            const double v = value(m);
            if (!UtilityVdm::has(v)) {
                continue;
            }
            if (previous != nullptr) {
                p.drawLine(at(a, *previous, previousValue), at(a, m, v));
            }
            previous = &m;
            previousValue = v;
        }
        for (const auto& m : messages) {
            const double v = value(m);
            if (!UtilityVdm::has(v)) {
                continue;
            }
            p.setPen(QPen{color, 1.8});
            p.setBrush(hollowWhenExtrapolated && m.extrapolated ? QColor{255, 255, 255} : color);
            p.drawEllipse(at(a, m, v), 3.8, 3.8);
        }
    };
    series(pressure, [] (const UtilityVdm::Vdm& m) { return m.pressure; }, QColor{200, 50, 50}, true);
    series(winds, [] (const UtilityVdm::Vdm& m) { return m.maxFlightWind(); }, QColor{30, 80, 200}, false);
    // the time labels: clock time of the first fix plus the hours
    QFont small{p.font()};
    small.setPixelSize(10);
    p.setFont(small);
    p.setPen(QColor{70, 70, 70});
    for (double h = 0; h <= span + 1e-9; h += xStep) {
        const auto pt = winds.at(h, 0.0);
        p.drawText(QRectF{pt.x() - 40, pt.y() + 14, 80, 12}, Qt::AlignHCenter, VdmViewer::timeText(first + static_cast<long>(h * 3600.0)));
    }
    p.drawText(QPointF{marginLeft, this->height() - 4.0}, "Filled dot: pressure from a dropsonde at the centre; hollow: extrapolated from flight level.");
}

namespace {
QString tableHtml(const HurricaneData::VdmData& data) {
    QString out = "<table border='1' cellspacing='0' cellpadding='3' style='font-size:12px'><tr style='background:#cfe2f3'><th>Time</th><th>Position</th><th>SLP (mb)</th><th>700 mb height</th>"
                  "<th>Centre dropsonde wind</th><th>Eye</th><th>Inbound flight wind</th><th>Outbound flight wind</th><th>Temp out / in eye (C)</th><th>Aircraft</th></tr>";
    for (auto it = data.messages.rbegin(); it != data.messages.rend(); ++it) {
        const auto& m = *it;
        out += "<tr><td>" + VdmViewer::timeText(m.seconds) + "</td><td>" + number(std::abs(m.lat), 2) + (m.lat >= 0 ? "N " : "S ") + number(std::abs(m.lon), 2) + (m.lon >= 0 ? "E" : "W") + "</td><td>" +
            number(m.pressure) + (m.extrapolated ? " (extrap.)" : "") + "</td><td>" + number(m.heightM) + " m</td><td>" +
            (UtilityVdm::has(m.centerWindKt) ? number(m.centerWindDir) + " deg " + number(m.centerWindKt) + " kt" : QString{"-"}) + "</td><td>" + q(m.eyeCharacter + (m.eyeShape.empty() ? "" : " " + m.eyeShape)) +
            "</td><td>" + wind(m.inboundFlight) + "</td><td>" + wind(m.outboundFlight) + "</td><td>" + number(m.tempOutsideC) + " / " + number(m.tempInsideC) + "</td><td>" + q(m.aircraft) + "</td></tr>";
    }
    return out + "</table>";
}
}

VdmViewer::VdmViewer(Window * parent, const std::shared_ptr<HurricaneData::VdmData>& data, const QString& storm)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Recon - vortex data messages " + storm.toStdString());
    auto * chart = new VdmChart{this};
    chart->setData(data);
    auto * browser = new QTextBrowser{this};
    QString html = "<p>" + summary(*data).toHtmlEscaped() + " (from the newest " + QString::number(data->filesRead) + " messages in the NHC recon archive).</p>";
    if (!data->messages.empty()) {
        html += tableHtml(*data);
        html += "<p style='color:#666'>Each row is one centre fix by the aircraft (NHC recon archive, REPNT2). Inbound / outbound: the strongest flight-level wind on the leg into / out of the centre, with where it was seen (bearing and range from the centre).</p>";
    }
    browser->setHtml(html);
    box.addWidgetReal(chart, 0, Qt::Alignment{});
    box.addWidgetReal(browser, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(1100, 700);
}
