// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/PhaseViewer.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QImage>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QInputDialog>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimeZone>
#include <QPushButton>
#include "gfs/GfsModels.h"
#include "hurricane/Coast.h"
#include "hurricane/HurricaneData.h"
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "util/Utility.h"
#include "ui/UiStandards.h"

namespace {
    // bold text with a white halo, so it reads over the shading, the lines and the dots
    void haloText(QPainter& painter, const QPointF& at, const QString& text, int points = 11, const QColor& color = Qt::black) {
        QFont font = painter.font();
        font.setBold(true);
        font.setPointSize(points);
        QPainterPath path;
        path.addText(at, font, text);
        painter.save();
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{Qt::white, 4.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
        painter.drawPath(path);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(path);
        painter.restore();
    }

    // the three marks of a track: the start (a square), the end (an arrowhead along the way it was going) and now (a ring); the text beside each, away from the line
    void trackMarks(QPainter& painter, const QPointF& start, const QPointF& beforeEnd, const QPointF& end, const QPointF * now, bool single, const QColor& edge = Qt::black) {
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(Qt::white);
        painter.setPen(QPen{edge, 2.6});
        painter.drawRect(QRectF{start.x() - 7, start.y() - 7, 14, 14});
        const bool crowded = now != nullptr && std::hypot(now->x() - start.x(), now->y() - start.y()) < 40.0;   // the labels of two marks close together go to different sides
        haloText(painter, start + (crowded ? QPointF{-48, -12} : QPointF{10, -9}), "START", 10, edge);
        if (!single) {
            QPointF d = end - beforeEnd;
            const double len = std::hypot(d.x(), d.y());
            d = len > 1e-6 ? d / len : QPointF{1, 0};
            const QPointF n{-d.y(), d.x()};
            QPolygonF head;
            head << end + d * 13 << end - d * 5 + n * 9 << end - d * 5 - n * 9;
            painter.setBrush(edge);
            painter.setPen(QPen{Qt::white, 1.4});
            painter.drawPolygon(head);
            haloText(painter, end + QPointF{12, 16}, "END", 10, edge);
        }
        if (now != nullptr) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{Qt::white, 5.5});
            painter.drawEllipse(*now, 10, 10);
            painter.setPen(QPen{edge, 2.6});
            painter.drawEllipse(*now, 10, 10);
            haloText(painter, *now + QPointF{12, 18}, "NOW", 10, edge);
        }
        painter.restore();
    }
}

// the diagrams: -VTL against -VTU as the main one (the shaded quadrants: cold core, shallow and deep warm core), and B against -VTL beside it. A dot is an analysis, a crossed dot a forecast; the colour is the
// storm's central pressure
class PhasePlot : public QWidget {
public:
    explicit PhasePlot(QWidget * parent) : QWidget{parent} {
        setMinimumHeight(360);
    }
    void set(const vector<PhaseSpace::Point>& p) {
        points = p;
        update();
    }
    // the other systems of a comparison, drawn thinner in their own colours under the one shown
    struct Line {
        QString label;
        QColor colour;
        vector<PhaseSpace::Point> points;
    };
    void setOthers(const vector<Line>& lines) {
        others = lines;
        update();
    }

public:
    static QColor pressureColor(double hPa) {
        static const std::pair<double, const char *> stops[] = {{1015, "#111111"}, {1010, "#8a2be2"}, {1000, "#2a5cff"}, {990, "#00c8d2"}, {980, "#20d020"}, {970, "#e8e020"}, {960, "#ff8c1a"}, {950, "#ff1493"}};
        if (hPa <= 0) {
            return QColor{128, 128, 128};
        }
        const std::pair<double, const char *> * best = &stops[0];
        for (const auto& s : stops) {
            if (std::fabs(s.first - hPa) < std::fabs(best->first - hPa)) {
                best = &s;
            }
        }
        return QColor{best->second};
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        const auto text = palette().color(QPalette::WindowText);
        double xs = 150.0, ys = 150.0, bs = 40.0;
        const auto scan = [&] (const vector<PhaseSpace::Point>& list) {
            for (const auto& p : list) {
                if (p.ok) {
                    xs = std::max(xs, std::fabs(p.vtl));
                    ys = std::max(ys, std::fabs(p.vtu));
                    if (p.bKnown) {
                        bs = std::max(bs, p.b);
                    }
                }
            }
        };
        scan(points);
        for (const auto& line : others) {
            scan(line.points);
        }
        xs = std::ceil(xs * 1.15 / 50.0) * 50.0;
        ys = std::ceil(ys * 1.15 / 50.0) * 50.0;
        bs = std::ceil(bs * 1.1 / 10.0) * 10.0;
        const int gap = 56, legend = 130;
        const double wide = (width() - legend) * 0.62;
        const QRectF main{gap, 12.0, wide - gap - 8, height() - 12.0 - 46};
        const QRectF side{wide + gap, 12.0, width() - legend - wide - gap - 12, height() - 12.0 - 46};
        const auto at = [] (const QRectF& r, double x0, double x1, double y0, double y1, double x, double y) {
            return QPointF{r.left() + (x - x0) / (x1 - x0) * r.width(), r.bottom() - (y - y0) / (y1 - y0) * r.height()};
        };
        // the quadrants of the main diagram, shaded by how far from the middle
        {
            QImage shade{static_cast<int>(main.width()), static_cast<int>(main.height()), QImage::Format_ARGB32};
            for (int py = 0; py < shade.height(); py++) {
                for (int px = 0; px < shade.width(); px++) {
                    const double x = -xs + 2.0 * xs * (px + 0.5) / shade.width(), y = ys - 2.0 * ys * (py + 0.5) / shade.height();
                    const double a = std::min(1.0, std::fabs(x) / xs), b = std::min(1.0, std::fabs(y) / ys);
                    const double strength = std::min(1.0, std::pow(std::max(a, b), 0.8) * 0.9 + 0.1 * std::min(a, b));
                    QColor c;
                    if (x < 0 && y < 0) {
                        c = QColor::fromRgbF(1.0 - strength, 1.0 - strength, 1.0);              // cold core: blue
                    } else if (x >= 0 && y < 0) {
                        c = QColor::fromRgbF(1.0, 1.0 - 0.8 * strength * a, 1.0);                 // lower warm, upper cold: magenta
                    } else if (x >= 0) {
                        c = QColor::fromRgbF(1.0, 1.0 - strength * (0.4 + 0.6 * b), 1.0 - strength * (0.9 - 0.5 * b));   // warm core: magenta to red
                    } else {
                        c = QColor::fromRgbF(1.0 - 0.3 * strength, 1.0 - 0.6 * strength, 1.0);   // lower cold, upper warm: lilac
                    }
                    shade.setPixelColor(px, py, c);
                }
            }
            painter.drawImage(main.topLeft(), shade);
        }
        painter.setPen(QPen{text, 1});
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(main);
        painter.drawRect(side);
        painter.setPen(QPen{QColor{0, 0, 0, 120}, 1});
        painter.drawLine(at(main, -xs, xs, -ys, ys, 0, -ys), at(main, -xs, xs, -ys, ys, 0, ys));
        painter.drawLine(at(main, -xs, xs, -ys, ys, -xs, 0), at(main, -xs, xs, -ys, ys, xs, 0));
        painter.drawLine(at(side, -xs, xs, 0, bs, 0, 0), at(side, -xs, xs, 0, bs, 0, bs));
        painter.setPen(QPen{QColor{0, 0, 0, 120}, 1, Qt::DashLine});
        painter.drawLine(at(side, -xs, xs, 0, bs, -xs, 10), at(side, -xs, xs, 0, bs, xs, 10));
        painter.setPen(text);
        const auto note = [&] (const QRectF& r, double x0, double x1, double y0, double y1, double x, double y, const QString& label) {
            painter.drawText(QRectF{at(r, x0, x1, y0, y1, x, y) - QPointF{80, 9}, QSizeF{160, 18}}, Qt::AlignCenter, label);
        };
        note(main, -xs, xs, -ys, ys, xs * 0.45, ys * 0.85, "DEEP WARM CORE");
        note(main, -xs, xs, -ys, ys, xs * 0.45, -ys * 0.3, "SHALLOW WARM");
        note(main, -xs, xs, -ys, ys, -xs * 0.45, -ys * 0.85, "DEEP COLD CORE");
        const auto axes = [&] (const QRectF& r, double x0, double x1, double y0, double y1, const QString& xl, const QString& yl) {
            painter.drawText(QRectF{r.left(), r.bottom() + 4, r.width(), 18}, Qt::AlignCenter, xl);
            painter.save();
            painter.translate(r.left() - 40, r.center().y());
            painter.rotate(-90);
            painter.drawText(QRectF{-r.height() / 2, 0, r.height(), 18}, Qt::AlignCenter, yl);
            painter.restore();
            painter.drawText(QPointF{r.left(), r.bottom() + 36}, QString::number(x0, 'f', 0));
            painter.drawText(QPointF{r.right() - 26, r.bottom() + 36}, QString::number(x1, 'f', 0));
            painter.drawText(QPointF{r.left() - 34, r.bottom()}, QString::number(y0, 'f', 0));
            painter.drawText(QPointF{r.left() - 34, r.top() + 10}, QString::number(y1, 'f', 0));
        };
        axes(main, -xs, xs, -ys, ys, "-VTL  900-600 mb thermal wind   (cold core <   > warm core)", "-VTU  600-300 mb");
        axes(side, -xs, xs, 0, bs, "-VTL  900-600 mb", "B (m): symmetric <  > asymmetric");
        const auto track = [&] (const QRectF& r, double x0, double x1, double y0, double y1, bool useB) {
            QPointF last;
            bool have = false;
            const PhaseSpace::Point * first = nullptr, * end = nullptr;
            for (const auto& p : points) {
                if (!p.ok || (useB && !p.bKnown)) {
                    continue;
                }
                first = first ? first : &p;
                end = &p;
                const auto pos = at(r, x0, x1, y0, y1, p.vtl, useB ? p.b : p.vtu);
                if (have) {
                    painter.setPen(QPen{Qt::black, 1.6});
                    painter.drawLine(last, pos);
                }
                last = pos;
                have = true;
            }
            for (const auto& p : points) {
                if (!p.ok || (useB && !p.bKnown)) {
                    continue;
                }
                const auto pos = at(r, x0, x1, y0, y1, p.vtl, useB ? p.b : p.vtu);
                painter.setPen(QPen{Qt::black, 1});
                painter.setBrush(pressureColor(p.pressure));
                painter.drawEllipse(pos, 6.5, 6.5);
                if (p.hour > 0) {   // a forecast: crossed
                    painter.setPen(QPen{Qt::white, 1.4});
                    painter.drawLine(pos + QPointF{-3.5, -3.5}, pos + QPointF{3.5, 3.5});
                    painter.drawLine(pos + QPointF{-3.5, 3.5}, pos + QPointF{3.5, -3.5});
                }
            }
            const PhaseSpace::Point * nowPoint = nullptr;
            const PhaseSpace::Point * beforeEnd = nullptr;
            const long nowSeconds = QDateTime::currentSecsSinceEpoch();
            const PhaseSpace::Point * previousOk = nullptr;
            for (const auto& p : points) {
                if (!p.ok || (useB && !p.bKnown)) {
                    continue;
                }
                if (nowPoint == nullptr || std::labs(p.valid - nowSeconds) < std::labs(nowPoint->valid - nowSeconds)) {
                    nowPoint = &p;
                }
                if (&p == end) {
                    beforeEnd = previousOk;
                }
                previousOk = &p;
            }
            if (first != nullptr) {
                const auto place = [&] (const PhaseSpace::Point * p) { return at(r, x0, x1, y0, y1, p->vtl, useB ? p->b : p->vtu); };
                const QPointF nowAt = nowPoint != nullptr ? place(nowPoint) : QPointF{};
                trackMarks(painter, place(first), beforeEnd != nullptr ? place(beforeEnd) : place(first), place(end), nowPoint != nullptr && nowPoint != first && nowPoint != end ? &nowAt : nullptr, end == first);
            }
        };
        for (const auto& line : others) {   // the other systems: a line and small dots in the system's colour, its name at the last point
            for (const bool useB : {false, true}) {
                const QRectF& r = useB ? side : main;
                const double y0 = useB ? 0.0 : -ys, y1 = useB ? bs : ys;
                QPointF last, lastDrawn;
                bool have = false;
                for (const auto& p : line.points) {
                    if (!p.ok || (useB && !p.bKnown)) {
                        continue;
                    }
                    const auto pos = at(r, -xs, xs, y0, y1, p.vtl, useB ? p.b : p.vtu);
                    if (have) {
                        painter.setPen(QPen{line.colour, 1.8});
                        painter.drawLine(last, pos);
                    }
                    painter.setPen(QPen{line.colour, 1});
                    painter.setBrush(p.hour > 0 ? QBrush{Qt::white} : QBrush{line.colour});
                    painter.drawEllipse(pos, 3.5, 3.5);
                    last = pos;
                    have = true;
                }
                if (have) {
                    painter.setPen(line.colour);
                    painter.drawText(last + QPointF{6, -6}, line.label);
                }
            }
        }
        track(main, -xs, xs, -ys, ys, false);
        track(side, -xs, xs, 0, bs, true);
        // the key
        double y = 14;
        const double x = width() - legend + 14;
        painter.setPen(text);
        painter.drawText(QPointF{x, y + 10}, "Pressure (hPa)");
        y += 22;
        for (const double hPa : {1015.0, 1010.0, 1000.0, 990.0, 980.0, 970.0, 960.0, 950.0}) {
            painter.setPen(QPen{Qt::black, 1});
            painter.setBrush(pressureColor(hPa));
            painter.drawEllipse(QPointF{x + 8, y + 8}, 7, 7);
            painter.setPen(text);
            painter.drawText(QPointF{x + 22, y + 13}, QString::number(hPa, 'f', 0));
            y += 22;
        }
        y += 8;
        painter.setPen(QPen{Qt::black, 1});
        painter.setBrush(QColor{128, 128, 128});
        painter.drawEllipse(QPointF{x + 8, y + 8}, 6, 6);
        painter.setPen(text);
        painter.drawText(QPointF{x + 22, y + 13}, "analysis");
        y += 22;
        painter.setPen(QPen{Qt::black, 1});
        painter.drawEllipse(QPointF{x + 8, y + 8}, 6, 6);
        painter.setPen(QPen{Qt::white, 1.4});
        painter.drawLine(QPointF{x + 5, y + 5}, QPointF{x + 11, y + 11});
        painter.drawLine(QPointF{x + 5, y + 11}, QPointF{x + 11, y + 5});
        painter.setPen(text);
        painter.drawText(QPointF{x + 22, y + 13}, "forecast");
        y += 30;
        painter.setPen(QPen{Qt::black, 2});
        painter.setBrush(Qt::white);
        painter.drawRect(QRectF{x + 2, y + 2, 12, 12});
        painter.setPen(text);
        painter.drawText(QPointF{x + 22, y + 13}, "start");
        y += 22;
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{Qt::black, 2});
        painter.drawEllipse(QPointF{x + 8, y + 8}, 8, 8);
        painter.setPen(text);
        painter.drawText(QPointF{x + 22, y + 13}, "now");
        y += 22;
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::black);
        painter.drawPolygon(QPolygonF{{QPointF{x + 15, y + 8}, QPointF{x + 1, y + 1}, QPointF{x + 1, y + 15}}});
        painter.setPen(text);
        painter.drawText(QPointF{x + 22, y + 13}, "end");
    }

private:
    vector<PhaseSpace::Point> points;
    vector<Line> others;
};

// where the storm has been and goes: the track on the coasts, a line through the analyses (past) and the forecast, the dots coloured by pressure as on the diagrams
class PhaseMap : public QWidget {
public:
    explicit PhaseMap(QWidget * parent) : QWidget{parent} {
        setMinimumSize(260, 200);
    }
    void set(const vector<PhaseSpace::Point>& p) {
        points = p;
        update();
    }
    void setObserved(const vector<PhaseSpace::Observed>& o) {
        observed = o;
        update();
    }
    void setOthers(const vector<PhasePlot::Line>& lines) {
        others = lines;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor{200, 218, 235});
        if (points.empty()) {
            return;
        }
        double west = 1e9, east = -1e9, south = 1e9, north = -1e9;
        for (const auto& p : points) {
            west = std::min(west, p.lon); east = std::max(east, p.lon);
            south = std::min(south, p.lat); north = std::max(north, p.lat);
        }
        for (const auto& line : others) {
            for (const auto& p : line.points) {
                if (std::fabs(p.lon - (west + east) / 2.0) < 120.0) {
                    west = std::min(west, p.lon); east = std::max(east, p.lon);
                    south = std::min(south, p.lat); north = std::max(north, p.lat);
                }
            }
        }
        for (const auto& o : observed) {
            if (std::fabs(o.lon - (west + east) / 2.0) < 120.0) {
                west = std::min(west, o.lon); east = std::max(east, o.lon);
                south = std::min(south, o.lat); north = std::max(north, o.lat);
            }
        }
        const double midLat = (south + north) / 2.0, scaleX = std::cos(midLat * 3.14159265 / 180.0);
        double spanX = std::max(12.0, (east - west) * 1.5), spanY = std::max(12.0 / std::max(0.3, scaleX), (north - south) * 1.5);
        // keep the degrees' aspect: the wider of the two sets the scale
        const double pixelsPerDegree = std::min(width() / (spanX * scaleX), height() / spanY);
        spanX = width() / pixelsPerDegree / scaleX;
        spanY = height() / pixelsPerDegree;
        const double cx = (west + east) / 2.0, cy = midLat;
        const auto at = [&] (double lon, double lat) {
            return QPointF{width() / 2.0 + (lon - cx) * scaleX * pixelsPerDegree, height() / 2.0 - (lat - cy) * pixelsPerDegree};
        };
        painter.setClipRect(rect());
        painter.setPen(QPen{QColor{90, 90, 90}, 1});
        for (const auto& line : Coast::worldLines()) {
            QPointF last;
            bool have = false;
            for (const auto& [lon, lat] : line) {
                if (lat < cy - spanY || lat > cy + spanY || lon < cx - spanX || lon > cx + spanX) {
                    have = false;
                    continue;
                }
                const auto pos = at(lon, lat);
                if (have) {
                    painter.drawLine(last, pos);
                }
                last = pos;
                have = true;
            }
        }
        {   // the observed (best) track: red, a ring at each fix, a filled one where it was a named system (a wind of 34 kt or more)
            QPointF last;
            bool haveObserved = false;
            for (const auto& o : observed) {
                const auto pos = at(o.lon, o.lat);
                if (haveObserved) {
                    painter.setPen(QPen{QColor{200, 30, 30}, 2.2});
                    painter.drawLine(last, pos);
                }
                last = pos;
                haveObserved = true;
            }
            for (const auto& o : observed) {
                painter.setPen(QPen{QColor{200, 30, 30}, 1.4});
                painter.setBrush(o.wind >= 34 ? QBrush{QColor{200, 30, 30}} : QBrush{Qt::white});
                painter.drawEllipse(at(o.lon, o.lat), 3.0, 3.0);
            }
            if (!observed.empty()) {
                painter.fillRect(QRectF{4, 3, 176, 18}, QColor{255, 255, 255, 190});
                painter.setPen(QColor{200, 30, 30});
                painter.drawText(QPointF{8, 16}, "red: observed (best track)");
            }
        }
        for (const auto& line : others) {   // the other systems of a comparison
            QPointF last;
            bool haveLine = false;
            for (const auto& p : line.points) {
                const auto pos = at(p.lon, p.lat);
                if (haveLine) {
                    painter.setPen(QPen{line.colour, 2.0, p.hour > 0 ? Qt::DashLine : Qt::SolidLine});
                    painter.drawLine(last, pos);
                }
                last = pos;
                haveLine = true;
            }
            if (haveLine) {
                painter.setPen(line.colour);
                painter.drawText(last + QPointF{6, -6}, line.label);
            }
        }
        QPointF previous;
        bool have = false;
        for (const auto& p : points) {
            const auto pos = at(p.lon, p.lat);
            if (have) {
                painter.setPen(QPen{Qt::black, 1.6, p.hour > 0 ? Qt::DashLine : Qt::SolidLine});
                painter.drawLine(previous, pos);
            }
            previous = pos;
            have = true;
        }
        for (const auto& p : points) {
            const auto pos = at(p.lon, p.lat);
            painter.setPen(QPen{Qt::black, 1});
            painter.setBrush(PhasePlot::pressureColor(p.pressure));
            painter.drawEllipse(pos, 4.5, 4.5);
        }
        const long now = QDateTime::currentSecsSinceEpoch();
        const PhaseSpace::Point * current = &points.front();
        for (const auto& p : points) {
            if (std::labs(p.valid - now) < std::labs(current->valid - now)) {
                current = &p;
            }
        }
        const auto& startP = points.front();
        const auto& endP = points.back();
        const auto& beforeP = points.size() >= 2 ? points[points.size() - 2] : startP;
        const QPointF nowAt = at(current->lon, current->lat);
        trackMarks(painter, at(startP.lon, startP.lat), at(beforeP.lon, beforeP.lat), at(endP.lon, endP.lat), current != &startP && current != &endP ? &nowAt : nullptr, points.size() < 2);
    }

private:
    vector<PhaseSpace::Point> points;
    vector<PhaseSpace::Observed> observed;
    vector<PhasePlot::Line> others;
};

vector<string> PhaseViewer::models() {
    vector<string> out;
    for (const auto& def : GfsModels::all()) {
        if (!def.clone.enabled && def.id != "GFS" && def.id != "IFS" && def.id != "AIFS" && def.id != "AIGFS" && def.id != "GEOS-FP") {
            continue;
        }
        const auto group = GfsModels::pickerGroup(def.id);
        if (def.storm || (group != "Global" && group != "Regional" && group != "Convection allowing") || def.id == "NBM" || def.id == "FIREWX" || !GfsModels::draws(def.id)) {
            continue;
        }
        if (!def.clone.labelPrefix.empty()) {
            continue;
        }
        out.push_back(def.id);
    }
    if (std::find(out.begin(), out.end(), "GFS") == out.end()) {
        out.insert(out.begin(), "GFS");
    }
    return out;
}

namespace {
    QPointer<PhaseViewer> openWindow;   // the window a pick joins

    const QColor seriesColours[] = {QColor{20, 20, 20}, QColor{200, 30, 30}, QColor{20, 110, 220}, QColor{20, 150, 70}, QColor{170, 60, 200}, QColor{230, 130, 0}};

    // the systems chosen before: "label|lat|lon|id", newest first
    QStringList recents() {
        return QString::fromStdString(Utility::readPref("PHASE_RECENT", "")).split('\n', Qt::SkipEmptyParts);
    }

    void remember(const string& label, double lat, double lon, const string& id) {
        auto list = recents();
        const QString entry = QString::fromStdString(label).replace('|', ' ').replace('\n', ' ') + "|" + QString::number(lat, 'f', 2) + "|" + QString::number(lon, 'f', 2) + "|" + QString::fromStdString(id);
        list.removeAll(entry);
        list.prepend(entry);
        while (list.size() > 10) {
            list.removeLast();
        }
        Utility::writePref("PHASE_RECENT", list.join('\n').toStdString());
    }

    // "52.0 -12.0 Norwegian Sea low", "52N 12W low", or an id ("al932026"); false when it is none of these
    bool parseSystem(const QString& text, string& label, double& lat, double& lon, string& id) {
        const auto trimmed = text.trimmed();
        if (QRegularExpression{"^[a-zA-Z]{2}[0-9]{6}$"}.match(trimmed).hasMatch()) {
            id = trimmed.toLower().toStdString();
            label = HurricaneData::idLabel(id);
            lat = lon = 0.0;
            return true;
        }
        const auto m = QRegularExpression{"^\\s*(-?[0-9.]+)\\s*([NSns]?)[ ,]+(-?[0-9.]+)\\s*([EWew]?)\\s*(.*)$"}.match(trimmed);
        if (!m.hasMatch()) {
            return false;
        }
        lat = m.captured(1).toDouble() * (m.captured(2).toUpper() == "S" ? -1.0 : 1.0);
        lon = m.captured(3).toDouble() * (m.captured(4).toUpper() == "W" ? -1.0 : 1.0);
        if (std::fabs(lat) > 90.0 || std::fabs(lon) > 360.0 || (lat == 0.0 && lon == 0.0)) {
            return false;
        }
        lon = lon > 180.0 ? lon - 360.0 : lon;
        id.clear();
        label = m.captured(5).trimmed().isEmpty() ? QString{"%1%2 %3%4"}.arg(std::fabs(lat), 0, 'f', 1).arg(lat >= 0 ? "N" : "S").arg(std::fabs(lon), 0, 'f', 1).arg(lon >= 0 ? "E" : "W").toStdString() : m.captured(5).trimmed().toStdString();
        return true;
    }
}

PhaseViewer * PhaseViewer::pick(Window * parent, const string& title, double lat, double lon, const string& model, const string& atcfId) {
    if (!openWindow.isNull() && !openWindow->closed) {
        openWindow->addSystem(title, lat, lon, atcfId);
        openWindow->raise();
        openWindow->activateWindow();
        return openWindow;
    }
    auto * window = new PhaseViewer{parent, title, lat, lon, model, atcfId};
    openWindow = window;
    return window;
}

PhaseViewer::PhaseViewer(Window * parent, const string& title, double lat, double lon, const string& model, const string& atcfId)
    : Window{parent}
    , session{std::make_shared<GfsRender::Session>()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Phase space - " + title);
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    modelBox = UiStandards::combo(top);
    for (const auto& id : models()) {
        const auto * def = GfsModels::find(id);
        modelBox->addItem(QString::fromStdString(id) + (def != nullptr ? "  -  " + QString::fromStdString(def->label) : QString{}), QString::fromStdString(id));
    }
    const int at = modelBox->findData(QString::fromStdString(model));
    modelBox->setCurrentIndex(at >= 0 ? at : 0);
    systemBox = UiStandards::combo(top);
    systemBox->setToolTip("The system shown in full below; the others are the thinner lines");
    auto * add = UiStandards::button(top, "Add...", "Add another system to compare: a place (52.0 -12.0 Norwegian Sea low) or a storm's id (al932026), or one of the last ones");
    auto * remove = UiStandards::button(top, "Remove", "Take the system shown out of the comparison");
    auto * again = UiStandards::button(top, "Measure", "Read the model's newest run again and follow the systems through it");
    topLayout->addWidget(modelBox, 1);
    topLayout->addWidget(systemBox, 1);
    topLayout->addWidget(add);
    topLayout->addWidget(remove);
    topLayout->addWidget(again);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    headline = new QLabel{this};
    headline->setWordWrap(true);
    box.addWidgetReal(headline, 0, Qt::Alignment{});
    status = new QLabel{this};
    status->setWordWrap(true);
    UiStandards::muted(status);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    plot = new PhasePlot{this};
    box.addWidgetReal(plot, 1, Qt::Alignment{});
    table = new QTableWidget{0, 8, this};
    table->setHorizontalHeaderLabels({"Hour", "Valid", "Centre", "hPa", "B (m)", "-VTL", "-VTU", "State"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setMinimumHeight(180);
    auto * lower = new QWidget{this};
    auto * lowerLayout = new QHBoxLayout{lower};
    lowerLayout->setContentsMargins(0, 0, 0, 0);
    map = new PhaseMap{lower};
    lowerLayout->addWidget(table, 2);
    lowerLayout->addWidget(map, 1);
    box.addWidgetReal(lower, 1, Qt::Alignment{});
    auto * credit = new QLabel{"After Hart (2003), Monthly Weather Review 131: B is the thickness asymmetry of 900-600 mb across the storm's track (above 10 m: asymmetric, a frontal or hybrid system); -VTL and -VTU are the lapse "
                               "with ln p of the range of the model's heights within 500 km, 900-600 and 600-300 mb (above 0: a warm core). The system is followed as the lowest sea level pressure of the model near where it was "
                               "(or its strongest spin), so a weak one can be lost; read the early hours as the model's idea of it, not a measurement.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 820);
    QObject::connect(again, &QPushButton::clicked, this, [this] { measureAll(); });
    QObject::connect(modelBox, &QComboBox::currentIndexChanged, this, [this] (int) { measureAll(); });
    QObject::connect(systemBox, &QComboBox::currentIndexChanged, this, [this] (int) { refresh(); });
    QObject::connect(add, &QPushButton::clicked, this, [this] { askForSystem(); });
    QObject::connect(remove, &QPushButton::clicked, this, [this] {
        const int index = systemBox->currentIndex();
        if (systems.size() > 1 && index >= 0 && static_cast<size_t>(index) < systems.size()) {
            systems.erase(systems.begin() + index);
            QSignalBlocker block{systemBox};
            systemBox->removeItem(index);
            systemBox->setCurrentIndex(std::min(index, systemBox->count() - 1));
            refresh();
        }
    });
    addSystem(title, lat, lon, atcfId);
}

std::shared_ptr<PhaseViewer::System> PhaseViewer::shown() const {
    const int index = systemBox->currentIndex();
    return index >= 0 && static_cast<size_t>(index) < systems.size() ? systems[static_cast<size_t>(index)] : nullptr;
}

void PhaseViewer::addSystem(const string& title, double lat, double lon, const string& atcfId) {
    auto system = std::make_shared<System>();
    system->label = title;
    system->lat = lat;
    system->lon = lon;
    system->atcfId = atcfId;
    system->colour = seriesColours[systems.size() % (sizeof seriesColours / sizeof seriesColours[0])];
    systems.push_back(system);
    remember(title, lat, lon, atcfId);
    {
        QSignalBlocker block{systemBox};
        systemBox->addItem(QString::fromStdString(title));
        systemBox->setCurrentIndex(systemBox->count() - 1);
    }
    if (systems.size() > 1) {
        setTitle("Phase space - comparison of " + std::to_string(systems.size()) + " systems");
    }
    start(system);
}

void PhaseViewer::askForSystem() {
    QStringList items;
    for (const auto& entry : recents()) {
        const auto parts = entry.split('|');
        if (parts.size() >= 4) {
            items << (parts[3].isEmpty() ? parts[1] + " " + parts[2] + " " + parts[0] : parts[3]);
        }
    }
    bool accepted = false;
    const auto text = QInputDialog::getItem(this, "Add a system", "A place and a name (latitude longitude name: 52.0 -12.0 Norwegian Sea low, or 52N 12W), or a storm's id (al932026):", items, 0, true, &accepted);
    if (!accepted || text.trimmed().isEmpty()) {
        return;
    }
    string label, id;
    double lat = 0.0, lon = 0.0;
    if (!parseSystem(text, label, lat, lon, id)) {
        status->setText("Could not read that: give a latitude, a longitude and a name, or a storm's id.");
        UiStandards::error(status);
        return;
    }
    addSystem(label, lat, lon, id);
}

void PhaseViewer::start(const std::shared_ptr<System>& system) {
    if (system->atcfId.size() < 6) {
        measure(system);
        return;
    }
    status->setText("Reading the observed track of " + QString::fromStdString(system->label) + "...");
    auto track = system->observed;
    const string id = system->atcfId;
    new FutureVoid{this, [track, id] {
        const string year = id.substr(id.size() - 4);
        const string nhc = "https://ftp.nhc.noaa.gov/atcf/btk/b" + id + ".dat";
        auto text = URL::getBytes(nhc).toStdString();
        if (text.size() < 100 || text.find(',') == string::npos) {
            text = URL::getBytes("https://hurricanes.ral.ucar.edu/repository/data/bdecks_open/" + year + "/b" + id + ".dat").toStdString();
        }
        *track = PhaseSpace::parseBest(text);
    }, [this, system] {
        if (closed) {
            return;
        }
        if (!system->observed->empty()) {   // where it is now: the newest fix, when it is recent (an invest, which the lists of storms give no position for, takes the newest fix of its file up to three days old)
            const auto& last = system->observed->back();
            const bool unknown = system->lat == 0.0 && system->lon == 0.0;
            if (QDateTime::currentSecsSinceEpoch() - last.valid < (unknown ? 72 : 12) * 3600) {
                system->lat = last.lat;
                system->lon = last.lon;
            }
        }
        if (system->lat == 0.0 && system->lon == 0.0) {
            system->problem = "no position yet: its best track file has no fix";
            system->measured = true;
            refresh();
            return;
        }
        measure(system);
    }};
}

void PhaseViewer::measureAll() {
    ++generation;
    for (const auto& system : systems) {
        if (system->lat != 0.0 || system->lon != 0.0) {
            measure(system);
        }
    }
}

void PhaseViewer::measure(const std::shared_ptr<System>& system) {
    const int mine = generation;
    const string model = modelBox->currentData().toString().toStdString();
    status->setText("Reading " + QString::fromStdString(model) + "'s newest run for " + QString::fromStdString(system->label) + "...");
    UiStandards::muted(status);
    system->measured = false;
    system->problem.clear();
    auto points = std::make_shared<vector<PhaseSpace::Point>>();
    auto problem = std::make_shared<string>();
    auto used = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    const double startLat = system->lat, startLon = system->lon;
    auto keep = session;
    auto obs = system->observed;
    new FutureVoid{this, [=] {
        string cycle, date;
        if (!GfsRender::latestCycle(model, cycle, "", &date)) {
            *problem = "no run of " + model + " was found (offline?)";
            return;
        }
        const string run = date + cycle.substr(0, 2);
        *used = run;
        const auto now = QDateTime::currentDateTimeUtc();
        const string valid = now.toString("yyyyMMddHH").toStdString();
        *ok = GfsRender::phaseTrack(*keep, model, run, valid, startLat, startLon, 120, *points, *problem);
        if (*ok && !points->empty()) {   // the analyses of the runs before: where it has been (each found from where the next one was)
            const auto * def = GfsModels::find(model);
            const int step = def != nullptr && !def->hourlyRuns ? std::max(6, def->cycleStep) : 6;
            auto when = QDateTime::fromString(QString::fromStdString(run), "yyyyMMddHH");
            when.setTimeSpec(Qt::UTC);
            double seedLat = points->front().lat, seedLon = points->front().lon;
            const long firstValid = points->front().valid;
            vector<PhaseSpace::Point> past;
            for (int k = 0; k < 12; k++) {
                const auto at = when.addSecs(-static_cast<qint64>(k) * step * 3600);
                const string id = at.toString("yyyyMMddHH").toStdString();
                vector<PhaseSpace::Point> one;
                string unused;
                double useLat = seedLat, useLon = seedLon;
                PhaseSpace::observedAt(*obs, static_cast<long>(at.toSecsSinceEpoch()), 0, useLat, useLon);   // (where it was observed to be, when there is a track)
                if (!GfsRender::phaseTrack(*keep, model, id, id, useLat, useLon, 0, one, unused) || one.empty() || !one.front().ok) {
                    break;
                }
                seedLat = one.front().lat;
                seedLon = one.front().lon;
                if (one.front().valid < firstValid) {
                    past.push_back(one.front());
                }
            }
            std::reverse(past.begin(), past.end());
            points->insert(points->begin(), past.begin(), past.end());
        }
    }, [this, mine, system, points, problem, used, ok, model] {
        if (closed || mine != generation) {
            return;
        }
        system->measured = true;
        if (!*ok) {
            system->problem = "could not measure " + model + ": " + *problem;
        } else {
            system->points = *points;
            runUsed = *used;
        }
        refresh();
    }};
}

// the diagrams, the map, the headline and the table: the system picked in full, the others thinner
void PhaseViewer::refresh() {
    const auto main = shown();
    table->setRowCount(0);
    plot->set({});
    plot->setOthers({});
    map->set({});
    map->setOthers({});
    map->setObserved({});
    headline->setText({});
    if (main == nullptr) {
        return;
    }
    vector<PhasePlot::Line> others;
    QStringList lines;
    int pending = 0;
    for (const auto& system : systems) {
        if (!system->measured) {
            pending++;
        }
        if (system->points.empty()) {
            if (system->measured && !system->problem.empty()) {
                lines << "<b>" + QString::fromStdString(system->label) + "</b>: " + QString::fromStdString(system->problem);
            }
            continue;
        }
        const auto smoothed = PhaseSpace::smoothed(system->points, 12 * 3600);
        const PhaseSpace::Point * nowPoint = nullptr;
        const long now = QDateTime::currentSecsSinceEpoch();
        for (const auto& p : smoothed) {   // the state now: the point nearest the present
            if (p.ok && (nowPoint == nullptr || std::labs(p.valid - now) < std::labs(nowPoint->valid - now))) {
                nowPoint = &p;
            }
        }
        if (nowPoint != nullptr) {
            lines << QString{"<span style='color:%1'>&#9632;</span> <b>%2</b>: %3 now (%4%5, -VTL %6, -VTU %7)"}.arg(system->colour.name(), QString::fromStdString(system->label).toHtmlEscaped(),
                QString::fromStdString(PhaseSpace::wordFor(*nowPoint)), QString::fromStdString(nowPoint->state).toLower(), nowPoint->bKnown ? ", B " + QString::number(nowPoint->b, 'f', 0) + " m" : QString{},
                QString::number(nowPoint->vtl, 'f', 0), QString::number(nowPoint->vtu, 'f', 0));
        }
        if (system != main) {
            others.push_back({QString::fromStdString(system->label), system->colour, smoothed});
        }
    }
    headline->setText(lines.join("<br>"));
    status->setText((runUsed.empty() ? QString{} : "Run " + QString::fromStdString(runUsed) + "Z, " + QString::number(systems.size()) + (systems.size() == 1 ? " system" : " systems") + ", followed at six hours.") + (pending > 0 ? "  Reading..." : QString{}));
    UiStandards::muted(status);
    if (main->points.empty()) {
        return;
    }
    plot->set(PhaseSpace::smoothed(main->points, 12 * 3600));
    plot->setOthers(others);
    vector<PhasePlot::Line> mapOthers;
    for (const auto& system : systems) {
        if (system != main && !system->points.empty()) {
            mapOthers.push_back({QString::fromStdString(system->label), system->colour, system->points});
        }
    }
    map->set(main->points);
    map->setOthers(mapOthers);
    map->setObserved(*main->observed);
    table->setRowCount(static_cast<int>(main->points.size()));
    int row = 0;
    for (const auto& p : main->points) {
        const auto valid = QDateTime::fromSecsSinceEpoch(p.valid, QTimeZone::utc());
        const QStringList cells{p.hour == 0 ? QString{"analysis"} : QString::number(p.hour), valid.toString("ddd HH") + "Z", QString::number(std::fabs(p.lat), 'f', 1) + (p.lat >= 0 ? "N " : "S ") + QString::number(std::fabs(p.lon), 'f', 1) + (p.lon >= 0 ? "E" : "W"),
                                p.pressure > 0 ? QString::number(p.pressure, 'f', 0) : "-", p.ok && p.bKnown ? QString::number(p.b, 'f', 0) : "-", p.ok ? QString::number(p.vtl, 'f', 0) : "-",
                                p.ok ? QString::number(p.vtu, 'f', 0) : "-", p.ok ? QString::fromStdString(PhaseSpace::wordFor(p)) : "not measurable"};
        for (int c = 0; c < cells.size(); c++) {
            table->setItem(row, c, new QTableWidgetItem{cells[c]});
        }
        row++;
    }
}
