// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SoundingViewer.h"
#include "util/Utility.h"
#include <algorithm>
#include <cmath>
#include <QBuffer>
#include <QDateTime>
#include <vector>
#include <QPainter>
#include <QPainterPath>
#include <QTimeZone>
#include <QWidget>
#include "models/UtilityGrib.h"
#include "models/UtilityModelSounding.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "settings/Location.h"
#include "util/SoundingSites.h"
#include "util/UtilityIO.h"
#include "sounding/SoundingAdvection.h"
#include "sounding/SoundingPrecip.h"
#include "sounding/SoundingThermo.h"
#include "util/To.h"

namespace {
    constexpr double pi = 3.14159265358979323846;   // M_PI is not defined by MSVC without _USE_MATH_DEFINES
    bool have(double v) { return v > -9998.0; }
    constexpr double pBottom = 1050.0;
    constexpr double pTop = 100.0;
    // SPC's sounding graphic: 100 - 1050 mb, -40 to +60 C along the bottom edge, isotherms leaning at 45 degrees of the
    // picture (measured from SPC's own image: one pixel to the right per pixel up). The plot is clipped, so isotherms
    // and traces that lean out of it are simply cut at its border.
    constexpr double tLeft = -40.0;    // temperature at the bottom-left corner of the Skew-T
    constexpr double tSpan = 100.0;    // degrees C shown across the bottom

    QString num(double v, int decimals = 0, const QString& unit = QString{}) {
        return have(v) ? QString::number(v, 'f', decimals) + unit : QStringLiteral("--");
    }

    // wind barb, northern-hemisphere convention: staff points into the wind, feathers on the clockwise side
    void drawBarb(QPainter& painter, const QPointF& at, double dirDeg, double speedKt, double length) {
        if (!have(dirDeg) || !have(speedKt)) return;
        if (speedKt < 2.5) {
            painter.drawEllipse(at, 3.0, 3.0);
            return;
        }
        const double rad = dirDeg * pi / 180.0;
        const QPointF staff{std::sin(rad), -std::cos(rad)};
        const QPointF perp{-staff.y(), staff.x()};
        const QPointF end = at + staff * length;
        painter.drawLine(at, end);
        int remaining = static_cast<int>(std::lround(speedKt / 5.0)) * 5;
        double along = length;
        const double step = length * 0.14;
        const double feather = length * 0.42;
        while (remaining >= 50) {
            const QPointF a = at + staff * along, b = at + staff * (along - step * 1.4);
            QPolygonF flag;
            flag << a << a + perp * feather + staff * (-step * 0.4) << b;
            painter.drawPolygon(flag);
            along -= step * 1.6;
            remaining -= 50;
        }
        while (remaining >= 10) {
            const QPointF a = at + staff * along;
            painter.drawLine(a, a + perp * feather - staff * (step * 0.6));
            along -= step;
            remaining -= 10;
        }
        if (remaining >= 5) {
            if (along >= length - 1e-6) along -= step;   // a lone half barb sits one step in from the end
            const QPointF a = at + staff * along;
            painter.drawLine(a, a + perp * feather * 0.5 - staff * (step * 0.3));
        }
    }
}

class SoundingCanvas : public QWidget {
public:
    explicit SoundingCanvas(QWidget * parent) : QWidget{parent} { setMinimumSize(780, 520); }

    void setMessage(const QString& text) {
        message = text;
        profile = nullptr;
        analysis = nullptr;
        update();
    }

    void setData(const SoundingProfile * newProfile, const SoundingAnalysis * newAnalysis, const QString& newTitle) {
        profile = newProfile;
        analysis = newAnalysis;
        title = newTitle;
        message.clear();
        update();
    }

    void setParcel(int index) {
        parcelIndex = index;
        update();
    }

    void setSpcLayout(bool spc) {
        spcLayout = spc;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor{12, 12, 16});
        if (profile == nullptr || analysis == nullptr) {
            painter.setPen(QColor{200, 200, 200});
            painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, message);
            return;
        }
        if (!spcLayout) {
            // dynamic layout: the plots and tables stretch to fill the window
            painter.setPen(QColor{235, 235, 235});
            QFont titleFont = painter.font();
            titleFont.setBold(true);
            painter.setFont(titleFont);
            painter.drawText(QRect{10, 4, width() - 20, 20}, Qt::AlignLeft | Qt::AlignVCenter, title);
            const int skewWidth = static_cast<int>(width() * 0.56);
            const QRect skew{46, 30, skewWidth - 46 - 8, height() - 30 - 22};
            const int rightX = skewWidth + 6;
            const int rightWidth = width() - rightX - 8;
            const int insetWidth = 76;   // the 1 km / 6 km wind barbs beside the hodograph
            const int hodoSize = std::min(rightWidth - insetWidth, static_cast<int>(height() * 0.34));
            drawSkewT(painter, skew);
            drawHodograph(painter, QRect{rightX + (rightWidth - insetWidth - hodoSize) / 2, 30, hodoSize, hodoSize});
            drawWindInset(painter, QRect{rightX + rightWidth - insetWidth, 34, insetWidth, 92});
            drawTable(painter, QRect{rightX, 30 + hodoSize + 22, rightWidth, height() - 30 - hodoSize - 26}, -1);
            return;
        }

        // SPC's graphic is 1180 x 826: everything is laid out in those units and scaled to fit the window (centred)
        constexpr double canvasWidth = 1180.0;
        constexpr double canvasHeight = 826.0;
        const double scale = std::min(width() / canvasWidth, height() / canvasHeight);
        painter.translate((width() - canvasWidth * scale) / 2.0, (height() - canvasHeight * scale) / 2.0);
        painter.scale(scale, scale);
        painter.setClipRect(QRectF{0, 0, canvasWidth, canvasHeight});
        painter.fillRect(QRectF{0, 0, canvasWidth, canvasHeight}, QColor{12, 12, 16});
        QFont base = painter.font();
        base.setPixelSize(12);
        painter.setFont(base);

        painter.setPen(QColor{235, 235, 235});
        QFont titleFont = base;
        titleFont.setBold(true);
        titleFont.setPixelSize(18);
        painter.setFont(titleFont);
        painter.drawText(QRect{30, 2, 760, 22}, Qt::AlignLeft | Qt::AlignVCenter, title);
        painter.setFont(base);

        // panel rectangles, measured from SPC's picture
        drawSkewT(painter, skewRect());
        drawWindSpeed(painter, QRect{595, 25, 93, 565});
        drawTempAdvection(painter, QRect{688, 25, 67, 565});
        drawHodograph(painter, QRect{755, 25, 415, 445});
        drawThetaE(painter, QRect{775, 492, 120, 108});
        drawStormRelativeWinds(painter, QRect{940, 492, 120, 108});
        // SHARPpy's effective-layer STP and SHIP box-and-whisker insets (Thompson et al. 2012; SPC): the day's value is the
        // coloured line across the plot
        {
            static const double efBoxes[6][5] = {{1.2, 2.6, 5.3, 8.3, 11.0}, {0.2, 1.0, 2.4, 4.5, 8.4}, {0.0, 0.6, 1.7, 3.7, 5.6},
                                                 {0.0, 0.3, 1.2, 2.6, 4.5}, {0.0, 0.1, 0.8, 2.0, 3.7}, {0.0, 0.0, 0.2, 0.7, 1.7}};
            static const double shipBoxes[2][5] = {{0.2, 0.3, 0.2, 0.9, 1.2}, {1.1, 1.4, 0.8, 2.8, 4.0}};
            const double stp = analysis->stpEffective;
            const double ship = analysis->hail;
            const auto stpColor = [] (double v) {
                if (v < 0.1) return QColor{0x77, 0x50, 0x00};
                if (v < 1.0) return QColor{0x99, 0x66, 0x00};
                if (v < 2.0) return QColor{255, 255, 255};
                if (v < 4.0) return QColor{255, 255, 0};
                if (v < 8.0) return QColor{255, 0, 0};
                return QColor{0xe7, 0x00, 0xdf};
            };
            const auto shipColor = [] (double v) {
                if (v >= 5.0) return QColor{0xe7, 0x00, 0xdf};
                if (v >= 2.0) return QColor{255, 0, 0};
                if (v >= 1.0) return QColor{255, 255, 0};
                if (v >= 0.5) return QColor{255, 255, 255};
                return QColor{0x77, 0x50, 0x00};
            };
            drawBoxPlot(painter, QRect{872, 640, 190, 150}, "Effective-Layer STP", 11.0, 1.0, {"EF4+", "EF3", "EF2", "EF1", "EF0", "NONT"},
                        &efBoxes[0][0], 6, true, stp, have(stp) ? stpColor(stp) : QColor{});
            drawBoxPlot(painter, QRect{1072, 640, 100, 150}, "SHIP", 5.0, 1.0, {"<=1.5\"", ">=2.5\""},
                        &shipBoxes[0][0], 2, false, ship, have(ship) ? shipColor(ship) : QColor{});
        }
        drawWindInset(painter, QRect{545, 742, 100, 78});
        // the table band under the plots: parcels and thermodynamics | kinematics | indices and precipitation type
        drawTable(painter, bottomLeft(), 0);
        drawTable(painter, bottomMiddle(), 1);
        drawTable(painter, bottomRight(), 2);
    }

    static QRect skewRect() { return QRect{30, 25, 565, 565}; }
    static QRect bottomLeft() { return QRect{10, 609, 355, 208}; }
    static QRect bottomMiddle() { return QRect{373, 609, 168, 130}; }
    static QRect bottomRight() { return QRect{665, 609, 190, 208}; }

private:
    const SoundingProfile * profile{nullptr};
    const SoundingAnalysis * analysis{nullptr};
    QString title;
    QString message;
    int parcelIndex{1};
    bool spcLayout{true};

    // ---- Skew-T geometry ----
    struct Geometry {
        QRect plot;
        double sx;
        double k;
        double yOf(double p) const { return plot.bottom() - std::log(pBottom / p) / std::log(pBottom / pTop) * plot.height(); }
        double xOf(double t, double p) const { return plot.left() + (t - tLeft) * sx + (plot.bottom() - yOf(p)) * k; }
        QPointF at(double t, double p) const { return {xOf(t, p), yOf(p)}; }
    };

    Geometry geometry(const QRect& plot) const {
        Geometry g;
        g.plot = plot;
        g.sx = plot.width() / tSpan;
        g.k = 1.0;   // 45 degrees: as many pixels right as up
        return g;
    }

    const SoundingParcel::Parcel& selectedParcel() const {
        return parcelIndex == 0 ? analysis->sb : parcelIndex == 2 ? analysis->mu : analysis->ml;
    }

    void strokePolyline(QPainter& painter, const Geometry& g, const std::vector<double>& temps, const std::vector<double>& pres) const {
        QPainterPath path;
        bool started = false;
        for (size_t i = 0; i < temps.size() && i < pres.size(); i += 1) {
            // levels above 100 mb (and below the plot) are kept: the clip rectangle crops the line exactly at the border,
            // as SPC's graphic does, instead of the trace stopping at the last level inside the plot
            if (!have(temps[i]) || !have(pres[i]) || pres[i] <= 0.0) {
                continue;
            }
            const auto point = g.at(temps[i], pres[i]);
            if (!started) {
                path.moveTo(point);
                started = true;
            } else {
                path.lineTo(point);
            }
        }
        painter.drawPath(path);
    }

    // pressure where the temperature profile first crosses `tC` going up (linear in log p between levels); -9999 if it never does
    double crossingPressure(double tC) const {
        const auto& p = *profile;
        for (size_t i = static_cast<size_t>(p.sfc); i + 1 < p.size(); i += 1) {
            const double t1 = p.tmpc[i], t2 = p.tmpc[i + 1];
            if (!have(t1) || !have(t2)) continue;
            if ((t1 - tC) * (t2 - tC) <= 0.0 && t1 != t2) {
                const double f = (tC - t1) / (t2 - t1);
                return std::exp(std::log(p.pres[i]) + f * (std::log(p.pres[i + 1]) - std::log(p.pres[i])));
            }
        }
        return -9999.0;
    }

    // SPC's / SHARPpy's extra marks on the Skew-T: height labels along the left edge (km above ground, SFC with the station
    // elevation), the freezing level and the -20 / -30 C levels (feet above ground), the layer of steepest 2 km lapse rate between
    // 2 and 6 km (when it is at least 4.5 C/km) and the effective inflow layer with its effective SRH
    void drawAnnotations(QPainter& painter, const Geometry& g, const QRect& plot) const {
        const auto& prof = *profile;
        painter.save();
        QFont font = painter.font();
        font.setPixelSize(11);
        painter.setFont(font);
        const auto clipped = [&] (auto draw) {
            painter.save();
            painter.setClipRect(plot);
            draw();
            painter.restore();
        };

        // heights above ground
        const QColor heightColor{255, 120, 120};
        for (const double h : {0.0, 1000.0, 3000.0, 6000.0, 9000.0, 12000.0, 15000.0}) {
            const double pressure = prof.interpPresAtHght(prof.toMsl(h));
            if (!have(pressure) || pressure < pTop || pressure > pBottom) continue;
            const double y = g.yOf(pressure);
            painter.setPen(heightColor);
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.left() + 10, y));
            const QString text = h == 0.0 ? QString("SFC (%1m)").arg(prof.sfcHght(), 0, 'f', 0) : QString("%1 km").arg(h / 1000.0, 0, 'f', 0);
            painter.drawText(QRectF(plot.left() + 13, y - 8, 90, 16), Qt::AlignLeft | Qt::AlignVCenter, text);
        }

        // freezing level, -20 C, -30 C: heights in feet above ground
        const QColor levelColor{120, 190, 255};
        const struct { double t; const char * name; } levels[] = {{0.0, "FZL"}, {-20.0, "-20C"}, {-30.0, "-30C"}};
        for (const auto& level : levels) {
            const double pressure = crossingPressure(level.t);
            if (!have(pressure) || pressure < pTop || pressure > pBottom) continue;
            const double heightFt = prof.toAgl(prof.interpHght(pressure)) * 3.28084;
            const double y = g.yOf(pressure);
            painter.setPen(QPen{levelColor, 1.5});
            painter.drawLine(QPointF(plot.right() - 92, y), QPointF(plot.right() - 62, y));
            painter.drawText(QRectF(plot.right() - 170, y - 14, 106, 14), Qt::AlignRight | Qt::AlignVCenter,
                             QString("%1 = %2'").arg(level.name).arg(heightFt, 0, 'f', 0));
        }

        // steepest 2 km lapse rate between 2 and 6 km AGL (SHARPpy max_lapse_rate: 250 m steps, virtual temperature)
        double bestRate = -1e9, bestBottom = -9999.0, bestTop = -9999.0;
        for (double bottomAgl = 2000.0; bottomAgl <= 4000.0 + 1e-6; bottomAgl += 250.0) {
            const double pBot = prof.interpPresAtHght(prof.toMsl(bottomAgl));
            const double pUp = prof.interpPresAtHght(prof.toMsl(bottomAgl + 2000.0));
            if (!have(pBot) || !have(pUp)) continue;
            const double tBot = prof.interpVtmp(pBot), tUp = prof.interpVtmp(pUp);
            if (!have(tBot) || !have(tUp)) continue;
            const double rate = (tUp - tBot) * -1000.0 / 2000.0;
            if (rate > bestRate) {
                bestRate = rate;
                bestBottom = pBot;
                bestTop = pUp;
            }
        }
        if (have(bestBottom) && bestRate >= 4.5) {
            const QColor color = bestRate >= 8.0 ? QColor{190, 100, 240} : bestRate >= 7.0 ? QColor{255, 90, 90}
                                : bestRate >= 6.0 ? QColor{210, 150, 70} : QColor{210, 210, 120};
            const double x = g.xOf(prof.interpVtmp(bestBottom) + 5.0, bestBottom);
            const double y1 = g.yOf(bestBottom), y2 = g.yOf(bestTop);
            clipped([&] {
                painter.setPen(QPen{color, 1.6});
                painter.drawLine(QPointF(x - 10, y1), QPointF(x + 10, y1));
                painter.drawLine(QPointF(x - 10, y2), QPointF(x + 10, y2));
                painter.drawLine(QPointF(x, y1), QPointF(x, y2));
                painter.drawText(QRectF(x - 15, y2 - 15, 70, 14), Qt::AlignLeft | Qt::AlignVCenter, QString("%1 C/km").arg(bestRate, 0, 'f', 1));
            });
        }

        // effective inflow layer: bracket with its base and top (m above ground) and the effective SRH
        const auto& layer = analysis->effective;
        if (layer.valid && have(layer.pBot) && have(layer.pTop)) {
            const QColor color{200, 110, 230};
            const double x1 = g.xOf(-20.0, pBottom), x2 = g.xOf(-33.0, pBottom);
            const double y1 = g.yOf(layer.pBot), y2 = g.yOf(layer.pTop);
            clipped([&] {
                painter.setPen(QPen{color, 2.0});
                painter.drawLine(QPointF(x1 - 15, y1), QPointF(x1 + 15, y1));
                painter.drawLine(QPointF(x1 - 15, y2), QPointF(x1 + 15, y2));
                painter.drawLine(QPointF(x1, y1), QPointF(x1, y2));
                const QString bottom = layer.pBot >= prof.sfcPres() - 0.5 ? QString{"SFC"} : QString("%1m").arg(layer.botAgl, 0, 'f', 0);
                painter.drawText(QRectF(x2, y1 + 3, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, bottom);
                painter.drawText(QRectF(x2, y2 - 15, 60, 14), Qt::AlignLeft | Qt::AlignVCenter, QString("%1m").arg(layer.topAgl, 0, 'f', 0));
                if (have(analysis->effectiveSrh)) {
                    painter.drawText(QRectF(x1 - 15, y2 - 15, 90, 14), Qt::AlignLeft | Qt::AlignVCenter,
                                     QString("%1 m2s2").arg(analysis->effectiveSrh, 0, 'f', 0));
                }
            });
        }
        painter.restore();
    }

    void drawSkewT(QPainter& painter, const QRect& plot) {
        const auto g = geometry(plot);
        painter.save();
        painter.setClipRect(plot);
        painter.fillRect(plot, QColor{0, 0, 0});

        // isotherms
        for (int t = -170; t <= 60; t += 10) {
            painter.setPen(QPen(t == 0 ? QColor{90, 160, 230} : QColor{70, 70, 80}, t == 0 ? 1.5 : 1.0));
            painter.drawLine(g.at(t, pBottom), g.at(t, pTop));
        }
        // dry adiabats
        painter.setPen(QPen(QColor{90, 70, 40}, 1.0));
        for (int thetaK = 220; thetaK <= 620; thetaK += 10) {
            QPainterPath path;
            bool started = false;
            for (double p = pBottom; p >= pTop - 1e-6; p -= 25.0) {
                const double t = thetaK * std::pow(p / 1000.0, SoundingThermo::rocp) - SoundingThermo::zeroCelsiusK;
                const auto point = g.at(t, p);
                if (!started) { path.moveTo(point); started = true; } else { path.lineTo(point); }
            }
            painter.drawPath(path);
        }
        // moist adiabats
        painter.setPen(QPen(QColor{40, 90, 50}, 1.0, Qt::DashLine));
        for (int t0 = -10; t0 <= 40; t0 += 5) {
            QPainterPath path;
            bool started = false;
            for (double p = 1000.0; p >= 200.0 - 1e-6; p -= 25.0) {
                const double t = p >= 1000.0 ? t0 : SoundingThermo::wetLift(1000.0, t0, p);
                const auto point = g.at(t, p);
                if (!started) { path.moveTo(point); started = true; } else { path.lineTo(point); }
            }
            painter.drawPath(path);
        }
        // isobars
        painter.setPen(QPen(QColor{90, 90, 100}, 1.0));
        for (int p = 1000; p >= 100; p -= 100) {
            painter.drawLine(QPointF(plot.left(), g.yOf(p)), QPointF(plot.right(), g.yOf(p)));
        }

        // CAPE / CIN shading for the selected parcel
        const auto& pcl = selectedParcel();
        if (pcl.valid && pcl.tracePres.size() >= 2) {
            auto parcelAt = [&] (double p) {
                for (size_t i = 0; i + 1 < pcl.tracePres.size(); i += 1) {
                    const double p1 = pcl.tracePres[i], p2 = pcl.tracePres[i + 1];
                    if (p <= p1 && p >= p2 && p1 != p2) {
                        const double f = (std::log(p1) - std::log(p)) / (std::log(p1) - std::log(p2));
                        return pcl.traceTemp[i] + f * (pcl.traceTemp[i + 1] - pcl.traceTemp[i]);
                    }
                }
                return SoundingThermo::missing;
            };
            const double pStart = pcl.lplPres;
            for (double p = std::min(pStart, pBottom); p > pTop + 5.0; p -= 5.0) {
                const double envT = profile->interpTemp(p), envT2 = profile->interpTemp(p - 5.0);
                const double parT = parcelAt(p), parT2 = parcelAt(p - 5.0);
                if (!have(envT) || !have(envT2) || !have(parT) || !have(parT2)) continue;
                if (have(pcl.elPres) && p < pcl.elPres) break;
                const bool positive = (parT + parT2) > (envT + envT2);
                if (!positive && !(have(pcl.lfcPres) ? p > pcl.lfcPres : p > pcl.lclPres)) continue;   // colder-than-environment air above the LFC is not CIN
                QPolygonF quad;
                quad << g.at(parT, p) << g.at(parT2, p - 5.0) << g.at(envT2, p - 5.0) << g.at(envT, p);
                painter.setPen(Qt::NoPen);
                painter.setBrush(positive ? QColor{230, 60, 60, 90} : QColor{70, 110, 230, 90});
                painter.drawPolygon(quad);
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(QColor{255, 255, 255}, 1.6, Qt::DashLine));
            strokePolyline(painter, g, pcl.traceTemp, pcl.tracePres);
        }

        // environment
        painter.setPen(QPen(QColor{80, 200, 220}, 1.0));
        strokePolyline(painter, g, profile->wetbulb, profile->pres);
        painter.setPen(QPen(QColor{60, 220, 80}, 2.2));
        strokePolyline(painter, g, profile->dwpc, profile->pres);
        painter.setPen(QPen(QColor{240, 60, 60}, 2.2));
        strokePolyline(painter, g, profile->tmpc, profile->pres);
        painter.restore();

        // frame, pressure and temperature labels
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(plot);
        for (int p = 1000; p >= 100; p -= 100) {
            painter.drawText(QRectF(plot.left() - 28, g.yOf(p) - 8, 26, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(p));
        }
        for (int t = -30; t <= 50; t += 10) {
            const double x = g.xOf(t, pBottom);
            if (x >= plot.left() && x <= plot.right()) {
                painter.drawText(QRectF(x - 16, plot.bottom() + 2, 32, 16), Qt::AlignCenter, QString::number(t));
            }
        }

        // level markers for the selected parcel
        painter.setPen(QColor{255, 220, 120});
        auto mark = [&] (const QString& name, double p) {
            if (!have(p) || p < pTop || p > pBottom) return;
            const double y = g.yOf(p);
            painter.drawLine(QPointF(plot.right() - 92, y), QPointF(plot.right() - 62, y));
            painter.drawText(QRectF(plot.right() - 150, y - 8, 54, 16), Qt::AlignRight | Qt::AlignVCenter, name);
        };
        mark("LCL", pcl.lclPres);
        mark("LFC", pcl.lfcPres);
        mark("EL", pcl.elPres);
        drawAnnotations(painter, g, plot);

        // wind barbs in their own column to the right of the plot
        painter.setPen(QPen(QColor{220, 220, 220}, 1.2));
        painter.setBrush(QColor{220, 220, 220});
        const double barbX = plot.right() - 34;
        double lastY = -1e9;
        for (size_t i = 0; i < profile->size(); i += 1) {
            const double p = profile->pres[i];
            if (p < pTop || p > pBottom || !have(profile->wdir[i]) || !have(profile->wspd[i])) continue;
            const double y = g.yOf(p);
            if (std::abs(y - lastY) < 16.0) continue;
            lastY = y;
            drawBarb(painter, QPointF(barbX, y), profile->wdir[i], profile->wspd[i], 26.0);
        }
        painter.setBrush(Qt::NoBrush);
    }

    void drawHodograph(QPainter& painter, const QRect& area) {
        using namespace SoundingIndices;
        painter.save();
        painter.fillRect(area, QColor{0, 0, 0});
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        // scale from the winds in the lowest 10 km
        double maxKt = 40.0;
        for (double h = 0; h <= 10000.0; h += 250.0) {
            const auto w = windAtAgl(*profile, h);
            if (w.valid()) maxKt = std::max(maxKt, w.speed() + 5.0);
        }
        const double limit = std::min(100.0, std::ceil(maxKt / 10.0) * 10.0);
        const QPointF centre = area.center();
        const double scale = (area.width() / 2.0 - 4.0) / limit;
        painter.setClipRect(area);
        painter.setPen(QPen(QColor{70, 70, 80}, 1.0));
        painter.drawLine(QPointF(area.left(), centre.y()), QPointF(area.right(), centre.y()));
        painter.drawLine(QPointF(centre.x(), area.top()), QPointF(centre.x(), area.bottom()));
        const double ringStep = limit > 60.0 ? 20.0 : 10.0;
        for (double r = ringStep; r <= limit + 1e-6; r += ringStep) {
            painter.drawEllipse(centre, r * scale, r * scale);
            painter.drawText(QPointF(centre.x() + 2, centre.y() - r * scale + 11), QString::number(static_cast<int>(r)));
        }
        auto toPoint = [&] (double u, double v) { return QPointF(centre.x() + u * scale, centre.y() - v * scale); };

        // trace, coloured by height band
        struct Band { double top; QColor color; };
        const Band bands[] = {{1000.0, QColor{240, 60, 60}}, {3000.0, QColor{60, 220, 80}}, {6000.0, QColor{240, 220, 60}},
                              {9000.0, QColor{80, 200, 240}}, {10000.0, QColor{190, 120, 240}}};
        QPointF previous;
        bool havePrevious = false;
        for (double h = 0; h <= 10000.0 + 1e-6; h += 100.0) {
            const auto w = windAtAgl(*profile, h);
            if (!w.valid()) continue;
            const auto point = toPoint(w.u, w.v);
            if (havePrevious) {
                QColor color = bands[4].color;
                for (const auto& band : bands) {
                    if (h <= band.top + 1e-6) { color = band.color; break; }
                }
                painter.setPen(QPen(color, 2.4));
                painter.drawLine(previous, point);
            }
            previous = point;
            havePrevious = true;
        }
        // storm motions and the mean wind
        painter.setPen(QPen(QColor{255, 255, 255}, 1.4));
        auto marker = [&] (const Wind& w, const QString& name, bool square) {
            if (!w.valid()) return;
            const auto point = toPoint(w.u, w.v);
            if (square) painter.drawRect(QRectF(point.x() - 3, point.y() - 3, 6, 6));
            else painter.drawEllipse(point, 4.0, 4.0);
            painter.drawText(point + QPointF(6, -4), name);
        };
        marker(analysis->rightMover, "RM", false);
        marker(analysis->leftMover, "LM", false);
        marker(analysis->meanWind06, "MW", true);
        painter.setClipping(false);
        painter.setPen(QColor{200, 200, 200});
        painter.drawText(QRectF(area.left(), area.bottom() + 1, area.width(), 14), Qt::AlignCenter,
                         "kt: red 0-1  grn 1-3  yel 3-6  cyan 6-9 km");
        painter.restore();
    }

    // two wind barbs, at 1 km and 6 km above ground (SPC's small inset)
    // wind speed against pressure beside the skew-T (same vertical scale), one bar per level coloured by height above
    // ground: under 3 km red, 3-6 bright green, 6-9 dark green, 9-12 purple, above 12 km cyan; 0-140 kt, dashed every 20 kt
    void drawWindSpeed(QPainter& painter, const QRect& area) {
        const auto& p = *profile;
        const auto g = geometry(skewRect());
        constexpr double maxSpeed = 140.0;
        painter.save();
        painter.setClipRect(area);
        painter.fillRect(area, QColor{0, 0, 0});
        painter.setPen(QPen{QColor{110, 110, 110}, 1.0, Qt::DashLine});
        for (int speed = 20; speed < static_cast<int>(maxSpeed); speed += 20) {
            const double x = area.left() + area.width() * speed / maxSpeed;
            painter.drawLine(QPointF{x, static_cast<double>(area.top())}, QPointF{x, static_cast<double>(area.bottom())});
        }
        QFont font = painter.font();
        font.setPixelSize(9);
        painter.setFont(font);
        painter.setPen(QColor{170, 170, 170});
        for (int speed = 20; speed < static_cast<int>(maxSpeed); speed += 40) {
            const double x = area.left() + area.width() * speed / maxSpeed;
            painter.drawText(QRectF(x - 14, area.bottom() - 14, 28, 12), Qt::AlignCenter, QString::number(speed));
        }
        const auto colorAt = [] (double agl) {
            if (agl < 3000.0) return QColor{255, 0, 0};
            if (agl < 6000.0) return QColor{0, 255, 0};
            if (agl < 9000.0) return QColor{0, 139, 0};
            if (agl < 12000.0) return QColor{145, 44, 238};
            return QColor{0, 255, 255};
        };
        std::vector<size_t> levels;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (have(p.pres[i]) && have(p.wspd[i]) && have(p.hght[i]) && p.pres[i] >= pTop && p.pres[i] <= pBottom) {
                levels.push_back(i);
            }
        }
        for (size_t n = 0; n < levels.size(); n += 1) {
            const auto i = levels[n];
            const double y = g.yOf(p.pres[i]);
            // the bar covers half the distance to each neighbouring level
            const double above = n + 1 < levels.size() ? (y + g.yOf(p.pres[levels[n + 1]])) / 2.0 : y - 1.0;
            const double below = n > 0 ? (y + g.yOf(p.pres[levels[n - 1]])) / 2.0 : y + 1.0;
            const double length = area.width() * std::min(p.wspd[i], maxSpeed) / maxSpeed;
            painter.fillRect(QRectF{static_cast<double>(area.left()), above, length, std::max(1.0, below - above)}, colorAt(p.toAgl(p.hght[i])));
        }
        painter.setClipping(false);
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        painter.drawText(QRectF(area.left(), area.top() + 2, area.width(), 12), Qt::AlignCenter, "Wind (kt)");
        painter.restore();
    }

    // inferred temperature advection (SHARPpy's panel): one box per 100 mb layer from the surface, from the centre line out
    // to the value (-13 .. +13 C/hr across the width), red warm, blue cold, the value printed in the layer
    void drawTempAdvection(QPainter& painter, const QRect& area) {
        const auto g = geometry(skewRect());
        const auto layers = SoundingAdvection::inferred(*profile, profile->latitude);
        painter.save();
        painter.setClipRect(area.adjusted(-1, -16, 1, 1));
        painter.fillRect(area, QColor{0, 0, 0});
        const double center = area.left() + area.width() / 2.0;
        const auto xOf = [&] (double value) { return center + value / 26.0 * area.width(); };
        painter.setPen(QPen{QColor{200, 200, 200}, 1.0, Qt::DashLine});
        painter.drawLine(QPointF{center, static_cast<double>(area.top())}, QPointF{center, static_cast<double>(area.bottom())});
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        for (const auto& layer : layers) {
            if (std::isnan(layer.advection)) {
                continue;
            }
            const double yBottom = g.yOf(layer.pBottom);
            const double yTop = g.yOf(layer.pTop);
            const double x = xOf(std::clamp(layer.advection, -13.0, 13.0));
            const QColor color = layer.advection > 0 ? QColor{255, 0, 0} : (layer.advection < 0 ? QColor{0x33, 0x99, 0xCC} : QColor{235, 235, 235});
            painter.setPen(QPen{color, 1.0});
            painter.drawRect(QRectF{QPointF{std::min(center, x), yTop}, QPointF{std::max(center, x), yBottom}});
            const double labelX = layer.advection < 0 ? xOf(-8.0) : xOf(8.0);
            painter.drawText(QRectF{labelX - 15.0, (yTop + yBottom) / 2.0 - 6.0, 30.0, 12.0}, Qt::AlignCenter, QString::number(layer.advection, 'f', 1));
        }
        painter.setClipping(false);
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QRectF(area.left() + 2, area.top() + 2, area.width() - 4, 26), Qt::AlignCenter | Qt::TextWordWrap, "Inf. Temp. Adv. (C/hr)");
        painter.restore();
    }

    // theta-e against pressure, 1025-400 mb (SHARPpy's thetae panel); the x range is the data's own +-10 K; TEI beneath
    void drawThetaE(QPainter& painter, const QRect& area) {
        const auto& p = *profile;
        double low = 1e9;
        double high = -1e9;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (p.pres[i] > 400.0 && !have(p.thetae[i])) continue;
            if (p.pres[i] > 400.0) {
                low = std::min(low, p.thetae[i] + 273.15);
                high = std::max(high, p.thetae[i] + 273.15);
            }
        }
        if (high < low) return;
        const double tMin = low - 10.0;
        const double tMax = high + 10.0;
        constexpr double pMax = 1025.0;
        constexpr double pMin = 400.0;
        const auto yOf = [&] (double pr) { return area.bottom() - (pMax - pr) / (pMax - pMin) * area.height(); };
        const auto xOf = [&] (double t) { return area.left() + (t - tMin) / (tMax - tMin) * area.width(); };
        painter.save();
        painter.fillRect(area, QColor{0, 0, 0});
        QFont font = painter.font();
        font.setPixelSize(9);
        painter.setFont(font);
        painter.setPen(QColor{200, 200, 200});
        for (int pr : {1000, 900, 800, 700, 600, 500}) {
            const double y = yOf(pr);
            painter.drawLine(QPointF{area.left() + 0.0, y}, QPointF{area.left() + 5.0, y});
            painter.drawLine(QPointF{area.right() - 5.0, y}, QPointF{static_cast<double>(area.right()), y});
            painter.drawText(QRectF{area.left() - 24.0, y - 6.0, 22.0, 12.0}, Qt::AlignRight | Qt::AlignVCenter, QString::number(pr));
        }
        for (int t = 200; t < 400; t += 10) {
            if (t < tMin || t > tMax) continue;
            const double x = xOf(t);
            painter.drawLine(QPointF{x, static_cast<double>(area.top())}, QPointF{x, area.top() + 5.0});
            painter.drawLine(QPointF{x, area.bottom() - 5.0}, QPointF{x, static_cast<double>(area.bottom())});
            painter.drawText(QRectF{x - 10.0, area.bottom() + 1.0, 20.0, 11.0}, Qt::AlignCenter, QString::number(t));
        }
        painter.setClipRect(area);
        painter.setPen(QPen{QColor{255, 0, 0}, 2.0});
        double lastX = 0.0;
        double lastY = 0.0;
        bool have2 = false;
        for (size_t i = 0; i < p.size(); i += 1) {
            if (!have(p.pres[i]) || !have(p.thetae[i]) || p.pres[i] <= 400.0) {
                have2 = false;
                continue;
            }
            const double x = xOf(p.thetae[i] + 273.15);
            const double y = yOf(p.pres[i]);
            if (have2) painter.drawLine(QPointF{lastX, lastY}, QPointF{x, y});
            lastX = x;
            lastY = y;
            have2 = true;
        }
        painter.setClipping(false);
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QRectF{area.left() + 4.0, area.top() + 2.0, area.width() - 8.0, 24.0}, Qt::AlignLeft | Qt::TextWordWrap, "Theta-E\nv. Pres");
        const double tei = SoundingIndices::thetaEIndex(p);
        if (have(tei)) {
            painter.drawText(QRectF{area.left() + 4.0, area.top() + 26.0, area.width() - 8.0, 12.0}, Qt::AlignLeft, QString{"TEI: %1 K"}.arg(tei, 0, 'f', 0));
        }
        painter.restore();
    }

    // storm-relative wind speed (right-mover storm motion) against height above ground, 0-16 km and 0-80 kt, as SHARPpy's
    // srwinds panel: the trace, the 0-2 / 4-6 / 9-11 km mean storm-relative winds as short bars, and the 40-70 kt
    // classic-supercell envelope above 8 km
    void drawStormRelativeWinds(QPainter& painter, const QRect& area) {
        const auto& p = *profile;
        const auto storm = analysis->rightMover;
        if (!storm.valid() || p.size() == 0) return;
        constexpr double hMax = 16.0;
        constexpr double sMax = 80.0;
        const auto yOf = [&] (double km) { return (area.bottom() - 2.0) - km / hMax * (area.height() - 2.0); };
        const auto xOf = [&] (double kt) { return area.left() + kt / sMax * area.width(); };
        painter.save();
        painter.fillRect(area, QColor{0, 0, 0});
        QFont font = painter.font();
        font.setPixelSize(9);
        painter.setFont(font);
        painter.setPen(QColor{200, 200, 200});
        for (int km : {2, 4, 6, 8, 10, 12, 14}) {
            const double y = yOf(km);
            painter.drawLine(QPointF{static_cast<double>(area.left()), y}, QPointF{area.left() + 5.0, y});
            painter.drawLine(QPointF{area.right() - 5.0, y}, QPointF{static_cast<double>(area.right()), y});
            painter.drawText(QRectF{area.left() - 18.0, y - 6.0, 16.0, 12.0}, Qt::AlignRight | Qt::AlignVCenter, QString::number(km));
        }
        for (int kt = 0; kt < 100; kt += 10) {
            if (kt > sMax) continue;
            const double x = xOf(kt);
            painter.drawLine(QPointF{x, static_cast<double>(area.top())}, QPointF{x, area.top() + 5.0});
            painter.drawLine(QPointF{x, area.bottom() - 5.0}, QPointF{x, static_cast<double>(area.bottom())});
            painter.drawText(QRectF{x - 10.0, area.bottom() + 1.0, 20.0, 11.0}, Qt::AlignCenter, QString::number(kt));
        }
        painter.setPen(QPen{QColor{200, 200, 200}, 1.0, Qt::DashLine});
        painter.drawLine(QPointF{xOf(0), static_cast<double>(area.top())}, QPointF{xOf(0), static_cast<double>(area.bottom())});
        const QColor classic{0xb1, 0x01, 0x9a};
        painter.setPen(QPen{classic, 1.0, Qt::DashLine});
        painter.drawLine(QPointF{xOf(40.0), yOf(8.0)}, QPointF{xOf(40.0), yOf(16.0)});
        painter.drawLine(QPointF{xOf(70.0), yOf(8.0)}, QPointF{xOf(70.0), yOf(16.0)});
        painter.setPen(classic);
        painter.drawText(QRectF{xOf(40.0) - 5.0, area.top() + 2.0, 50.0, 24.0}, Qt::AlignCenter, "Classic\nSupercell");
        painter.setClipRect(area);
        // the trace: storm-relative speed every 10 m, from the surface to 16 km (or the top of the data)
        painter.setPen(QPen{QColor{255, 0, 0}, 1.0});
        const double sfc = p.sfcHght();
        double lastX = 0.0;
        double lastY = 0.0;
        bool haveLast = false;
        for (double h = 0.0; h < hMax * 1000.0; h += 10.0) {
            const double pr = h <= 0.0 ? p.sfcPres() : p.interpPresAtHght(sfc + h);
            double u;
            double v;
            if (!have(pr) || !p.interpComponents(pr, u, v)) {
                haveLast = false;
                if (have(pr)) continue;
                break;
            }
            const double x = xOf(std::hypot(u - storm.u, v - storm.v));
            const double y = yOf(h / 1000.0);
            if (haveLast) painter.drawLine(QPointF{lastX, lastY}, QPointF{x, y});
            lastX = x;
            lastY = y;
            haveLast = true;
        }
        const auto meanBar = [&] (double fromKm, double toKm, const QColor& color) {
            const auto mean = SoundingIndices::meanWind(p, fromKm * 1000.0, toKm * 1000.0);
            if (!mean.valid()) return;
            const double x = xOf(std::hypot(mean.u - storm.u, mean.v - storm.v));
            painter.setPen(QPen{color, 2.0});
            painter.drawLine(QPointF{x, yOf(fromKm)}, QPointF{x, yOf(toKm)});
        };
        meanBar(0.0, 2.0, QColor{0x8b, 0x00, 0x00});
        meanBar(4.0, 6.0, QColor{0x64, 0x95, 0xed});
        meanBar(9.0, 11.0, QColor{0x94, 0x00, 0xd3});
        painter.setClipping(false);
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QRectF{area.left() + 3.0, area.bottom() - 40.0, 60.0, 34.0}, Qt::AlignLeft | Qt::TextWordWrap, "SR Winds\nv. Height");
        painter.restore();
    }

    // a box-and-whisker inset: each row of `boxes` is {low whisker end, box bottom, median, box top, high whisker end};
    // the y axis runs 0..yMax with a dashed line at every `yStep`; `value` is drawn as a line across the plot in `valueColor`
    void drawBoxPlot(QPainter& painter, const QRect& area, const QString& title, double yMax, double yStep, const std::vector<QString>& names,
                     const double * boxes, int count, bool median, double value, const QColor& valueColor) {
        painter.save();
        painter.fillRect(area, QColor{0, 0, 0});
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        painter.setPen(QColor{235, 235, 235});
        painter.drawText(QRectF{area.left() + 0.0, area.top() + 1.0, static_cast<double>(area.width()), 14.0}, Qt::AlignCenter, title);
        const QRectF plot{area.left() + 18.0, area.top() + 18.0, area.width() - 22.0, area.height() - 36.0};
        const auto yOf = [&] (double v) { return plot.bottom() - std::clamp(v, 0.0, yMax) / yMax * plot.height(); };
        font.setPixelSize(9);
        painter.setFont(font);
        for (double y = 0.0; y <= yMax + 1e-9; y += yStep) {
            painter.setPen(QPen{QColor{0x00, 0x80, 0xff}, 1.0, Qt::DashLine});
            painter.drawLine(QPointF{plot.left(), yOf(y)}, QPointF{plot.right(), yOf(y)});
            painter.setPen(QColor{235, 235, 235});
            painter.drawText(QRectF{area.left() + 0.0, yOf(y) - 6.0, 16.0, 12.0}, Qt::AlignRight | Qt::AlignVCenter, QString::number(y, 'f', 0));
        }
        const double spacing = plot.width() / (count + 1);
        const double width = plot.width() / (count * 2.2);
        for (int i = 0; i < count; i += 1) {
            const double cx = plot.left() + spacing * (i + 1);
            const double* b = boxes + i * 5;
            painter.setPen(QPen{QColor{0, 255, 0}, 2.0});
            painter.drawLine(QPointF{cx, yOf(b[0])}, QPointF{cx, yOf(b[1])});
            painter.drawRect(QRectF{QPointF{cx - width / 2.0, yOf(b[3])}, QPointF{cx + width / 2.0, yOf(b[1])}});
            if (median) painter.drawLine(QPointF{cx - width / 2.0, yOf(b[2])}, QPointF{cx + width / 2.0, yOf(b[2])});
            painter.drawLine(QPointF{cx, yOf(b[3])}, QPointF{cx, yOf(b[4])});
            painter.setPen(QColor{235, 235, 235});
            QFont small = painter.font();
            small.setPixelSize(8);
            painter.setFont(small);
            painter.drawText(QRectF{cx - spacing / 2.0 - 2.0, plot.bottom() + 3.0, spacing + 4.0, 12.0}, Qt::AlignCenter, names[static_cast<size_t>(i)]);
            small.setPixelSize(9);
            painter.setFont(small);
        }
        if (have(value) && valueColor.isValid()) {
            painter.setPen(QPen{valueColor, 1.5});
            painter.drawLine(QPointF{plot.left(), yOf(value)}, QPointF{plot.right(), yOf(value)});
            painter.drawText(QRectF{plot.left() + 2.0, yOf(value) - 12.0, 60.0, 11.0}, Qt::AlignLeft | Qt::AlignVCenter, QString::number(value, 'f', 1));
        }
        painter.setPen(QColor{200, 200, 200});
        painter.drawRect(area);
        painter.restore();
    }

    void drawWindInset(QPainter& painter, const QRect& area) {
        const auto& p = *profile;
        painter.save();
        const QColor colors[2] = {QColor{255, 110, 110}, QColor{120, 190, 255}};
        const double heights[2] = {1000.0, 6000.0};
        const char * labels[2] = {"1 km", "6 km"};
        QFont font = painter.font();
        font.setPixelSize(10);
        painter.setFont(font);
        for (int i = 0; i < 2; i += 1) {
            double u = 0.0;
            double v = 0.0;
            const double pressure = p.interpPresAtHght(p.toMsl(heights[i]));
            const bool ok = have(pressure) && p.interpComponents(pressure, u, v);
            const QPointF center{area.left() + area.width() * (0.25 + 0.5 * i), area.top() + 34.0};
            painter.setPen(QPen{colors[i], 1.6});
            painter.setBrush(colors[i]);
            if (ok) {
                const SoundingIndices::Wind wind{u, v};
                // the staff points into the wind: start half a staff down-wind so the barb is centred on the cell
                const double rad = wind.direction() * pi / 180.0;
                const QPointF toward{std::sin(rad), -std::cos(rad)};
                drawBarb(painter, center - toward * 17.0, wind.direction(), wind.speed(), 34.0);
            }
            painter.setPen(QColor{225, 225, 225});
            painter.drawText(QRectF{center.x() - 24, area.top() + 54.0, 48, 12}, Qt::AlignCenter, labels[i]);
        }
        painter.setPen(QColor{200, 200, 200});
        painter.drawText(QRectF{static_cast<double>(area.left()), area.top() + 68.0, static_cast<double>(area.width()), 24}, Qt::AlignHCenter | Qt::AlignTop, "Wind barbs\n(above ground)");
        painter.restore();
    }

    // The parameters as grids: one small table per group (parcels, winds, storm motion, indices, thermodynamics, lapse
    // rates, mixing ratio) with a header row, shaded alternate rows and aligned columns.
    struct GridSection {
        vector<QString> header;
        vector<vector<QString>> rows;
        bool labelColumn;   // first column holds row names (left-aligned) rather than values
    };

    // `group` picks the part of the table band (-1: everything): 0 parcels / thermodynamics / lapse rates, 1 winds and storm motion, 2 indices and precip type
    void drawTable(QPainter& painter, const QRect& area, int group) {
        using namespace SoundingIndices;
        const auto& a = *analysis;
        vector<GridSection> sections;

        GridSection parcels{{"", "SB", "ML", "MU"}, {}, true};
        auto parcelRow = [&] (const QString& name, auto pick) {
            parcels.rows.push_back({name, pick(a.sb), pick(a.ml), pick(a.mu)});
        };
        parcelRow("CAPE", [&] (const auto& p) { return num(p.cape, 0); });
        parcelRow("CINH", [&] (const auto& p) { return num(p.cin, 0); });
        parcelRow("LI 500", [&] (const auto& p) { return num(p.liftedIndex500, 1); });
        parcelRow("LCL m", [&] (const auto& p) { return num(p.lclHght, 0); });
        parcelRow("LFC m", [&] (const auto& p) { return num(p.lfcHght, 0); });
        parcelRow("EL m", [&] (const auto& p) { return num(p.elHght, 0); });
        parcelRow("CAPE 0-3", [&] (const auto& p) { return num(p.cape3km, 0); });
        sections.push_back(parcels);

        sections.push_back({{"", "0-1 km", "0-3 km", "0-6 km", "Eff"},
                            {{"Shear kt", num(a.shear01.speed(), 0), num(a.shear03.speed(), 0), num(a.shear06.speed(), 0), num(a.effectiveShearKt, 0)},
                             {"SRH", num(a.srh01, 0), num(a.srh03, 0), "", num(a.effectiveSrh, 0)}},
                            true});

        sections.push_back({{"Bunkers", "Right", "Left", "Eff inflow"},
                            {{"dir / kt", num(a.rightMover.direction(), 0) + "/" + num(a.rightMover.speed(), 0),
                              num(a.leftMover.direction(), 0) + "/" + num(a.leftMover.speed(), 0),
                              a.effective.valid ? num(a.effective.botAgl, 0) + " - " + num(a.effective.topAgl, 0) + " m" : QString{"--"}}},
                            true});

        sections.push_back({{"STP fix", "STP eff", "SCP", "SHIP"},
                            {{num(a.stpFixed, 1), num(a.stpEffective, 1), num(a.supercell, 1), num(a.hail, 2)}},
                            false});

        sections.push_back({{"PW in", "DCAPE", "0C m", "WBZ m", "Conv T C", "RH sfc %"},
                            {{num(a.precipitableWaterIn, 2), num(a.dcape, 0), num(a.freezingLevelAgl, 0), num(a.wetBulbZeroAgl, 0),
                              num(a.convectiveTemp, 1), num(a.surfaceRh, 0)}},
                            false});

        sections.push_back({{"", "0-3", "3-6 MSL", "700-500"},
                            {{"Lapse C/km", num(a.lapse03, 1), num(a.lapse36, 1), num(a.lapse700500, 1)}},
                            true});

        sections.push_back({{"", "low 100 mb", "0-3 km"},
                            {{"Mean w g/kg", num(a.meanMixingLow100, 1), num(a.meanMixing03, 1)}},
                            true});

        // SPC's "best guess precip type", from the profile (SoundingPrecip, after SHARPpy)
        {
            const auto guess = SoundingPrecip::bestGuess(*profile);
            QString text = QString::fromStdString(guess.type.empty() ? std::string{"--"} : guess.type);
            if (have(guess.surfaceTempC)) {
                text += QString("  -  sfc temp %1 F").arg(guess.surfaceTempC * 1.8 + 32.0, 0, 'f', 1);
            }
            if (guess.phase >= 0 && have(guess.sourcePressure)) {
                text += QString(", source ~%1 mb (%2 C)").arg(guess.sourcePressure, 0, 'f', 0).arg(guess.sourceTemp, 0, 'f', 0);
            } else if (guess.phase < 0) {
                text += ", no saturated layer below 5 km";
            }
            sections.push_back({{"Best guess precip type"}, {{text}}, false});
        }

        if (group >= 0) {
            static const std::vector<std::vector<size_t>> groups{{0, 4, 5, 6}, {1, 2}, {3, 7}};
            std::vector<GridSection> chosen;
            for (const auto index : groups[static_cast<size_t>(group)]) {
                chosen.push_back(sections[index]);
            }
            sections = chosen;
        }

        painter.save();
        QFont font{painter.font()};
        // the largest size (14 px down to 7) at which every table fits the area, in height and (for the widest cell) width
        int rowsTotal = 0;
        for (const auto& section : sections) {
            rowsTotal += static_cast<int>(section.rows.size()) + 1;
        }
        const int gap = 3;
        const int gaps = static_cast<int>(sections.size()) - 1;
        int pixelSize = 14;
        for (; pixelSize > 7; pixelSize -= 1) {
            font.setPixelSize(pixelSize);
            const QFontMetrics metrics{font};
            const int rowHeight = metrics.height() + 2;
            bool fits = rowHeight * rowsTotal + gap * gaps <= area.height();
            for (const auto& section : sections) {
                const int columns = static_cast<int>(section.header.size());
                const double labelShare = section.labelColumn ? 1.45 : 1.0;
                const double unit = area.width() / (columns - 1 + labelShare);
                auto widest = [&] (const vector<QString>& cells, bool header) {
                    for (size_t c = 0; c < cells.size(); c += 1) {
                        const double cellWidth = (c == 0 ? labelShare : 1.0) * unit - 8;
                        if (metrics.horizontalAdvance(cells[c]) > cellWidth) {
                            return false;
                        }
                    }
                    (void) header;
                    return true;
                };
                fits = fits && widest(section.header, true);
                for (const auto& row : section.rows) {
                    fits = fits && widest(row, false);
                }
            }
            if (fits) {
                break;
            }
        }
        font.setPixelSize(pixelSize);
        const QFontMetrics metrics{font};
        const int rowHeight = metrics.height() + 2;
        QFont bold = font;
        bold.setBold(true);
        const QColor headerFill{58, 70, 96};
        const QColor stripeA{34, 37, 44};
        const QColor stripeB{43, 47, 56};
        const QColor lines{86, 92, 104};

        int y = area.top();
        for (const auto& section : sections) {
            const int columns = static_cast<int>(section.header.size());
            const double labelShare = section.labelColumn ? 1.45 : 1.0;
            const double unit = area.width() / (columns - 1 + labelShare);
            vector<double> edges{static_cast<double>(area.left())};
            for (int c = 0; c < columns; c += 1) {
                edges.push_back(edges.back() + (c == 0 ? labelShare : 1.0) * unit);
            }
            const int rowCount = static_cast<int>(section.rows.size()) + 1;
            for (int r = 0; r < rowCount; r += 1) {
                const bool header = r == 0;
                const auto& cells = header ? section.header : section.rows[static_cast<size_t>(r - 1)];
                const QRectF rowRect{static_cast<double>(area.left()), static_cast<double>(y), static_cast<double>(area.width()), static_cast<double>(rowHeight)};
                painter.fillRect(rowRect, header ? headerFill : (r % 2 == 1 ? stripeA : stripeB));
                for (int c = 0; c < columns; c += 1) {
                    const QRectF cell{edges[static_cast<size_t>(c)], static_cast<double>(y), edges[static_cast<size_t>(c) + 1] - edges[static_cast<size_t>(c)], static_cast<double>(rowHeight)};
                    painter.setFont(header || (c == 0 && section.labelColumn) ? bold : font);
                    painter.setPen(header ? QColor{235, 240, 255} : QColor{230, 230, 230});
                    const bool left = c == 0 && section.labelColumn;
                    painter.drawText(cell.adjusted(left ? 4 : 0, 0, left ? 0 : 0, 0), (left ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignVCenter,
                                     static_cast<size_t>(c) < cells.size() ? cells[static_cast<size_t>(c)] : QString{});
                    if (c > 0) {
                        painter.setPen(lines);
                        painter.drawLine(QPointF{cell.left(), cell.top()}, QPointF{cell.left(), cell.bottom()});
                    }
                }
                painter.setPen(lines);
                painter.drawLine(QPointF{rowRect.left(), rowRect.bottom()}, QPointF{rowRect.right(), rowRect.bottom()});
                y += rowHeight;
            }
            painter.setPen(lines);
            painter.drawRect(QRectF{static_cast<double>(area.left()), static_cast<double>(y - rowHeight * rowCount), static_cast<double>(area.width()), static_cast<double>(rowHeight * rowCount)});
            y += gap;
        }
        painter.restore();
    }
};

namespace {
    struct Result {
        bool ok{false};
        string status;
        SoundingProfile profile;
        SoundingAnalysis analysis;
        QDateTime validTime;
    };

    // "Latest" plus the last week of 00z/12z launches, newest first
    std::vector<std::pair<string, string>> observedTimes() {
        std::vector<std::pair<string, string>> out{{"Latest", ""}};
        auto when = QDateTime::currentDateTimeUtc();
        when = QDateTime{when.date(), QTime{when.time().hour() >= 12 ? 12 : 0, 0}, QTimeZone::utc()};
        for (int i = 0; i < 14; i += 1) {
            out.emplace_back(when.toString("yyyy-MM-dd HH").toStdString() + "z", when.toString("yyMMddHH").toStdString());
            when = when.addSecs(-12 * 3600);
        }
        return out;
    }

    std::vector<string> labelsOf(const std::vector<std::pair<string, string>>& items) {
        std::vector<string> out;
        for (const auto& item : items) out.push_back(item.first);
        return out;
    }
}

SoundingViewer::SoundingViewer(Window * parent, double lon, double lat, const string& runId, const string& forecastHour)
    : Window{parent}
    , lon{lon}
    , lat{lat}
    , runId{runId}
    , forecastHour{forecastHour}
    , textInfo{this}
    , comboSite{this, {"-"}}
    , comboTime{this, {"-"}}
    , comboArea{this, {"Point", "15 km mean", "30 km mean", "60 km mean"}}
    , comboParcel{this, {"Surface-based parcel", "Mixed-layer parcel", "Most-unstable parcel"}}
    , comboLayout{this, {"SPC layout", "Dynamic layout"}}
    , buttonSave{new QPushButton{"Save", this}}
    , canvas{new SoundingCanvas{this}}
{
    setTitle("RRFS Sounding");
    build();
    start();
}

SoundingViewer::SoundingViewer(Window * parent, double lon, double lat, const QDateTime& validUtc)
    : SoundingViewer{parent, lon, lat, string{}, string{"0"}}
{
    // the delegated constructor already started a fetch for hour "0"; restart for the valid time
    wantedValid = validUtc;
    start();
}

SoundingViewer::SoundingViewer(Window * parent, const string& site)
    : Window{parent}
    , lon{0.0}
    , lat{0.0}
    , textInfo{this}
    , comboSite{this, SoundingSites::sites->nameList}
    , comboTime{this, labelsOf(observedTimes())}
    , comboArea{this, {"Point"}}
    , comboParcel{this, {"Surface-based parcel", "Mixed-layer parcel", "Most-unstable parcel"}}
    , comboLayout{this, {"SPC layout", "Dynamic layout"}}
    , buttonSave{new QPushButton{"Save", this}}
    , canvas{new SoundingCanvas{this}}
    , observed{true}
{
    setTitle("SPC Observed Sounding");
    for (const auto& item : observedTimes()) {
        timeCodes.push_back(item.second);
    }
    const auto code = site.empty() ? SoundingSites::sites->getNearest(Location::getLatLonCurrent()) : site;
    const auto& codes = SoundingSites::sites->codeList;
    const auto found = std::find(codes.begin(), codes.end(), code);
    comboSite.setIndex(found == codes.end() ? 0 : static_cast<size_t>(found - codes.begin()));
    comboSite.connect([this] { startObserved(); });
    comboTime.setIndex(0);
    comboTime.connect([this] { startObserved(); });
    build();
    startObserved();
}

void SoundingViewer::build() {
    comboParcel.setIndex(1);
    comboParcel.connect([this] { canvas->setParcel(comboParcel.getIndex()); });
    comboLayout.getView()->setToolTip("SPC layout: SPC's graphic, scaled to fit. Dynamic layout: the skew-T, hodograph and tables stretch to fill the window.");
    comboLayout.setIndex(static_cast<size_t>(Utility::readPref("SOUNDING_LAYOUT", "spc") == "dynamic" ? 1 : 0));
    canvas->setSpcLayout(comboLayout.getIndex() == 0);
    comboLayout.connect([this] {
        Utility::writePref("SOUNDING_LAYOUT", comboLayout.getIndex() == 0 ? "spc" : "dynamic");
        canvas->setSpcLayout(comboLayout.getIndex() == 0);
    });
    QObject::connect(buttonSave, &QPushButton::clicked, this, [this] { onSave(); });
    rowTop.addWidget(textInfo, 1);
    // each mode shows only its own pickers; the others exist (members) but stay hidden
    if (observed) {
        rowTop.addWidget(comboSite);
        rowTop.addWidget(comboTime);
        comboArea.setVisible(false);
    } else {
        comboArea.connect([this] { start(); });
        rowTop.addWidget(comboArea);
        comboSite.setVisible(false);
        comboTime.setVisible(false);
    }
    rowTop.addWidget(comboParcel);
    rowTop.addWidget(comboLayout);
    rowTop.addWidgetReal(buttonSave);
    box.addLayout(rowTop);
    box.addWidgetReal(canvas, 1, Qt::Alignment{});
    box.getAndShow(this);
    setSize(1000, 640);
}

void SoundingViewer::start() {
    generation += 1;
    const int thisGeneration = generation;
    textInfo.setText(QString{"%1 N, %2 W  -  fetching the model column (about 265 MB for a new run/hour, cached afterwards)..."}
                         .arg(lat, 0, 'f', 2).arg(-lon, 0, 'f', 2));
    canvas->setMessage("Fetching the RRFS column...\n\nA run/hour not seen before downloads about 265 MB (all 25 mb levels);\nafter that, every other point at the same run and hour is instant.");
    auto result = std::make_shared<Result>();
    const auto lonNow = lon, latNow = lat;
    const auto runNow = runId;
    const auto validNow = wantedValid;
    auto hourNow = forecastHour;
    static const double radii[] = {0.0, 15.0, 30.0, 60.0};
    const double radiusNow = radii[std::clamp(comboArea.getIndex(), 0, 3)];
    new FutureVoid{this,
        [result, lonNow, latNow, runNow, validNow, hourNow, radiusNow]() mutable {
            string date;
            string cycle;
            if (!UtilityGrib::resolveSynopticRun(runNow, date, cycle)) {
                result->status = "Model sounding: could not resolve an RRFS run";
                return;
            }
            if (validNow.isValid()) {
                const QDateTime runUtc{QDate{To::Int(date.substr(0, 4)), To::Int(date.substr(4, 2)), To::Int(date.substr(6, 2))},
                                       QTime{To::Int(cycle), 0}, QTimeZone::utc()};
                const auto lead = static_cast<int>(std::llround(runUtc.secsTo(validNow) / 3600.0));
                if (lead < 1 || lead > 84) {
                    result->status = "Model sounding: the product's valid time (" + validNow.toString("yyyy-MM-dd HH").toStdString() +
                        "z) is " + (lead < 1 ? "before the latest synoptic RRFS run (" : "beyond the forecast of the latest synoptic RRFS run (") +
                        date + " " + cycle + "z, F1-F84), so there is no matching RRFS hour";
                    return;
                }
                hourNow = To::string(lead);
            }
            string detail;
            result->ok = UtilityModelSounding::buildProfile(date, cycle, hourNow, lonNow, latNow, result->profile, detail, radiusNow);
            if (!result->ok) {
                result->status = detail;
                return;
            }
            result->profile.latitude = latNow;   // for the inferred temperature advection
            result->analysis = SoundingAnalysis::compute(result->profile);
            const QDateTime runUtc{QDate{To::Int(date.substr(0, 4)), To::Int(date.substr(4, 2)), To::Int(date.substr(6, 2))},
                                   QTime{To::Int(cycle), 0}, QTimeZone::utc()};
            const auto validLocal = runUtc.addSecs(3600 * To::Int(hourNow)).toLocalTime();
            result->status = "RRFS " + date.substr(0, 4) + "-" + date.substr(4, 2) + "-" + date.substr(6, 2) + " " + cycle + "z    F" +
                (hourNow.size() < 2 ? "0" + hourNow : hourNow) + " valid " +
                validLocal.toString("ddd h:mm AP").toStdString() + " " +
                QTimeZone::systemTimeZone().abbreviation(validLocal).toStdString();
        },
        [this, result, thisGeneration, radiusNow] {
            if (closed || thisGeneration != generation) return;
            status = result->status;
            loaded = result->ok;
            if (!loaded) {
                textInfo.setText(QString::fromStdString(status));
                canvas->setMessage(QString::fromStdString(status) + "\n\nClose this window and try again, or pick another point or hour.");
                return;
            }
            profile = result->profile;
            analysis = result->analysis;
            const auto point = QString{"%1 N, %2 W"}.arg(lat, 0, 'f', 2).arg(-lon, 0, 'f', 2) +
                (radiusNow > 0.0 ? QString{"  (%1 km mean)"}.arg(radiusNow, 0, 'f', 0) : QString{});
            textInfo.setText(QString::fromStdString(status) + "    " + point);
            canvas->setData(&profile, &analysis, QString::fromStdString(status) + "    Sounding " + point);
        }};
}

void SoundingViewer::startObserved() {
    generation += 1;
    const int thisGeneration = generation;
    const auto index = static_cast<size_t>(std::max(0, comboTime.getIndex()));
    const string timeCode = index < timeCodes.size() ? timeCodes[index] : string{};
    const string siteCode = SoundingSites::sites->codeList[static_cast<size_t>(std::max(0, comboSite.getIndex()))];
    const string siteName = SoundingSites::sites->byCode[siteCode]->fullName;
    const string url = timeCode.empty() ? "https://www.spc.noaa.gov/exper/soundings/LATEST/" + siteCode + ".txt"
                                        : "https://www.spc.noaa.gov/exper/soundings/" + timeCode + "_OBS/" + siteCode + ".txt";
    textInfo.setText(QString::fromStdString("SPC sounding " + siteCode + " " + siteName + " - fetching..."));
    canvas->setMessage(QString::fromStdString("Fetching the " + siteCode + " sounding from SPC..."));
    auto result = std::make_shared<Result>();
    new FutureVoid{this,
        [result, url, siteCode, siteName, timeCode] {
            const auto text = UtilityIO::getHtml(url);
            string error;
            if (!SoundingProfile::parseSpcText(text, result->profile, error)) {
                result->status = "No sounding for " + siteCode + " " + siteName + (timeCode.empty() ? " (latest)" : " at 20" + timeCode.substr(0, 2) + "-" + timeCode.substr(2, 2) + "-" +
                    timeCode.substr(4, 2) + " " + timeCode.substr(6, 2) + "z") +
                    ". SPC publishes 00z and 12z launches (some sites also 06z/18z); the site may not have launched, or the archive may not hold that time.";
                return;
            }
            result->profile.latitude = SoundingSites::sites->byCode[siteCode]->latLon.lat();
            result->analysis = SoundingAnalysis::compute(result->profile);
            // "260930/1200" -> UTC time
            const auto& v = result->profile.validTime;
            if (v.size() >= 11) {
                result->validTime = QDateTime{QDate{2000 + To::Int(v.substr(0, 2)), To::Int(v.substr(2, 2)), To::Int(v.substr(4, 2))},
                                              QTime{To::Int(v.substr(7, 2)), To::Int(v.substr(9, 2))}, QTimeZone::utc()};
            }
            result->status = "SPC observed sounding  " + siteCode + " " + siteName + "  " +
                (result->validTime.isValid() ? result->validTime.toString("yyyy-MM-dd HH:mm").toStdString() + "z" : v);
            result->ok = true;
        },
        [this, result, thisGeneration] {
            if (closed || thisGeneration != generation) return;
            status = result->status;
            loaded = result->ok;
            if (!loaded) {
                textInfo.setText(QString{"No sounding found"});
                canvas->setMessage(QString::fromStdString(status));
                return;
            }
            profile = result->profile;
            analysis = result->analysis;
            observedTime = result->validTime;
            textInfo.setText(QString::fromStdString(status));
            canvas->setData(&profile, &analysis, QString::fromStdString(status));
        }};
}

void SoundingViewer::onSave() {
    if (!loaded) {
        return;
    }
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    canvas->grab().save(&buffer, "PNG");
    QString suggested;
    if (observed) {
        const auto code = QString::fromStdString(SoundingSites::sites->codeList[static_cast<size_t>(std::max(0, comboSite.getIndex()))]);
        suggested = UtilityAnimationExport::validName(observedTime, QDateTime{}, "sounding_" + code);
    } else {
        suggested = UtilityAnimationExport::modelName(QString::fromStdString(status), QString::fromStdString(status),
            QString{"sounding_%1_%2"}.arg(lat, 0, 'f', 2).arg(lon, 0, 'f', 2));
    }
    UtilityAnimationExport::saveWithDialog(this, {}, 0, bytes, suggested, QByteArray{}, false);
}

void SoundingViewer::closeEventCustom() {
    closed = true;
}
