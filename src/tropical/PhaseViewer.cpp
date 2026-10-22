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
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QTimeZone>
#include <QPushButton>
#include "gfs/GfsModels.h"
#include "hurricane/Coast.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"

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
        for (const auto& p : points) {
            if (p.ok) {
                xs = std::max(xs, std::fabs(p.vtl));
                ys = std::max(ys, std::fabs(p.vtu));
                if (p.bKnown) {
                    bs = std::max(bs, p.b);
                }
            }
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
            QFont big = painter.font();
            big.setBold(true);
            big.setPointSizeF(big.pointSizeF() * 1.5);
            painter.setFont(big);
            painter.setPen(Qt::black);
            if (first != nullptr) {
                painter.drawText(at(r, x0, x1, y0, y1, first->vtl, useB ? first->b : first->vtu) + QPointF{-18, -9}, "A");
            }
            if (end != nullptr && end != first) {
                painter.drawText(at(r, x0, x1, y0, y1, end->vtl, useB ? end->b : end->vtu) + QPointF{9, -9}, "Z");
            }
            big.setPointSizeF(big.pointSizeF() / 1.5);
            big.setBold(false);
            painter.setFont(big);
        };
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
    }

private:
    vector<PhaseSpace::Point> points;
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
        QFont big = painter.font();
        big.setBold(true);
        painter.setFont(big);
        painter.setPen(Qt::black);
        painter.drawText(at(points.front().lon, points.front().lat) + QPointF{-14, -7}, "A");
        painter.drawText(at(points.back().lon, points.back().lat) + QPointF{7, -7}, "Z");
        // now: the point nearest the present
        const long now = QDateTime::currentSecsSinceEpoch();
        const PhaseSpace::Point * current = &points.front();
        for (const auto& p : points) {
            if (std::labs(p.valid - now) < std::labs(current->valid - now)) {
                current = &p;
            }
        }
        painter.drawText(at(current->lon, current->lat) + QPointF{7, 14}, "C");
    }

private:
    vector<PhaseSpace::Point> points;
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

PhaseViewer::PhaseViewer(Window * parent, const string& title, double lat, double lon, const string& model)
    : Window{parent}
    , session{std::make_shared<GfsRender::Session>()}
    , lat{lat}
    , lon{lon}
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
    auto * again = UiStandards::button(top, "Measure", "Read the model's newest run again and follow the storm through it");
    topLayout->addWidget(modelBox, 1);
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
                               "with ln p of the range of the model's heights within 500 km, 900-600 and 600-300 mb (above 0: a warm core). The storm is followed as the lowest sea level pressure of the model near where it was, "
                               "so a weak invest can be lost; read the early hours as the model's idea of it, not a measurement.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 820);
    QObject::connect(again, &QPushButton::clicked, this, [this] { measure(); });
    QObject::connect(modelBox, &QComboBox::currentIndexChanged, this, [this] (int) { measure(); });
    measure();
}

void PhaseViewer::measure() {
    const int mine = ++generation;
    const string model = modelBox->currentData().toString().toStdString();
    headline->setText({});
    status->setText("Reading " + QString::fromStdString(model) + "'s newest run...");
    table->setRowCount(0);
    plot->set({});
    map->set({});
    auto points = std::make_shared<vector<PhaseSpace::Point>>();
    auto problem = std::make_shared<string>();
    auto runUsed = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    const double startLat = lat, startLon = lon;
    auto keep = session;
    new FutureVoid{this, [=] {
        string cycle, date;
        if (!GfsRender::latestCycle(model, cycle, "", &date)) {
            *problem = "no run of " + model + " was found (offline?)";
            return;
        }
        const string run = date + cycle.substr(0, 2);
        *runUsed = run;
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
                if (!GfsRender::phaseTrack(*keep, model, id, id, seedLat, seedLon, 0, one, unused) || one.empty() || !one.front().ok) {
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
    }, [this, mine, points, problem, runUsed, ok, model] {
        if (closed || mine != generation) {
            return;
        }
        if (!*ok) {
            status->setText("Could not measure " + QString::fromStdString(model) + ": " + QString::fromStdString(*problem));
            UiStandards::error(status);
            return;
        }
        UiStandards::muted(status);
        status->setText("Run " + QString::fromStdString(*runUsed) + "Z, followed " + QString::number(points->size()) + " times at six hours.");
        plot->set(PhaseSpace::smoothed(*points, 12 * 3600));
        map->set(*points);
        const auto run = QDateTime::fromString(QString::fromStdString(runUsed->substr(0, 10)), "yyyyMMddHH");
        const PhaseSpace::Point * first = nullptr;
        const long now = QDateTime::currentSecsSinceEpoch();
        for (const auto& p : *points) {   // the state now: the point nearest the present
            if (p.ok && (first == nullptr || std::labs(p.valid - now) < std::labs(first->valid - now))) {
                first = &p;
            }
        }
        if (first != nullptr) {
            headline->setText("<b>" + QString::fromStdString(PhaseSpace::wordFor(*first)) + "</b> now (" + QString::fromStdString(first->state).toLower() + (first->bKnown ? ", B " + QString::number(first->b, 'f', 0) + " m" : QString{}) + ", -VTL " + QString::number(first->vtl, 'f', 0) + ", -VTU " + QString::number(first->vtu, 'f', 0) + ")");
        }
        table->setRowCount(static_cast<int>(points->size()));
        int row = 0;
        for (const auto& p : *points) {
            const auto valid = QDateTime::fromSecsSinceEpoch(p.valid, QTimeZone::utc());
            const QStringList cells{p.hour == 0 ? QString{"analysis"} : QString::number(p.hour), valid.toString("ddd HH") + "Z", QString::number(std::fabs(p.lat), 'f', 1) + (p.lat >= 0 ? "N " : "S ") + QString::number(std::fabs(p.lon), 'f', 1) + (p.lon >= 0 ? "E" : "W"),
                                    p.pressure > 0 ? QString::number(p.pressure, 'f', 0) : "-", p.ok && p.bKnown ? QString::number(p.b, 'f', 0) : "-", p.ok ? QString::number(p.vtl, 'f', 0) : "-",
                                    p.ok ? QString::number(p.vtu, 'f', 0) : "-", p.ok ? QString::fromStdString(PhaseSpace::wordFor(p)) : "not measurable"};
            for (int c = 0; c < cells.size(); c++) {
                table->setItem(row, c, new QTableWidgetItem{cells[c]});
            }
            row++;
        }
    }};
}
