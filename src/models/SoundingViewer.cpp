// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SoundingViewer.h"
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
        painter.setPen(QColor{235, 235, 235});
        QFont titleFont = painter.font();
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(QRect{10, 4, width() - 20, 20}, Qt::AlignLeft | Qt::AlignVCenter, title);

        const int skewWidth = static_cast<int>(width() * 0.56);
        const QRect skew{46, 30, skewWidth - 46 - 62, height() - 30 - 22};
        const int rightX = skewWidth + 6;
        const int rightWidth = width() - rightX - 8;
        const int insetWidth = 76;   // the 1 km / 6 km wind barbs beside the hodograph
        const int hodoSize = std::min(rightWidth - insetWidth, static_cast<int>(height() * 0.34));
        drawSkewT(painter, skew);
        drawHodograph(painter, QRect{rightX + (rightWidth - insetWidth - hodoSize) / 2, 30, hodoSize, hodoSize});
        drawWindInset(painter, QRect{rightX + rightWidth - insetWidth, 34, insetWidth, 92});
        drawTable(painter, QRect{rightX, 30 + hodoSize + 22, rightWidth, height() - 30 - hodoSize - 26});
    }

private:
    const SoundingProfile * profile{nullptr};
    const SoundingAnalysis * analysis{nullptr};
    QString title;
    QString message;
    int parcelIndex{1};

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
            painter.drawText(QRectF(plot.left() - 40, g.yOf(p) - 8, 36, 16), Qt::AlignRight | Qt::AlignVCenter, QString::number(p));
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
            painter.drawLine(QPointF(plot.right() - 38, y), QPointF(plot.right(), y));
            painter.drawText(QRectF(plot.right() - 76, y - 8, 36, 16), Qt::AlignRight | Qt::AlignVCenter, name);
        };
        mark("LCL", pcl.lclPres);
        mark("LFC", pcl.lfcPres);
        mark("EL", pcl.elPres);

        // wind barbs in their own column to the right of the plot
        painter.setPen(QPen(QColor{220, 220, 220}, 1.2));
        painter.setBrush(QColor{220, 220, 220});
        const double barbX = plot.right() + 34;
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

    void drawTable(QPainter& painter, const QRect& area) {
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
