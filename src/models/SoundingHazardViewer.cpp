// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SoundingHazardViewer.h"
#include <algorithm>
#include <cmath>
#include <QDate>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <QVBoxLayout>
#include "gfs/GfsClimate.h"
#include "models/UtilityGrib.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"

class HazardChart : public QWidget {
public:
    explicit HazardChart(QWidget * parent) : QWidget{parent} {
        setMinimumSize(760, 600);
        ChartExport::install(this, "hazard flow chart");
    }
    void set(const SoundingHazard::Result& newResult, const QString& newNote, const QString& newTitle) {
        result = newResult;
        note = newNote;
        title = newTitle;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), palette().window());
        constexpr double W = 1000.0, H = 790.0;
        const double scale = std::min(width() / W, height() / H);
        p.translate((width() - W * scale) / 2.0, (height() - H * scale) / 2.0);
        p.scale(scale, scale);
        QFont base = p.font();
        base.setPixelSize(12);
        p.setFont(base);
        const QColor text = palette().text().color();
        const QFontMetrics fm{base};
        const double lineHeight = 15.0;

        // the node boxes: left column from the top, right column from the bottom
        struct Place {
            QRectF box;
            const SoundingHazard::Node * node;
        };
        std::vector<Place> left, right;
        double y = 40.0;
        for (const auto& node : result.nodes) {
            if (node.id[0] != 'L') {
                continue;
            }
            const double h = node.lines.size() * lineHeight + 14.0;
            left.push_back({QRectF{10, y, 350, h}, &node});
            y += h + 20.0;
        }
        double bottom = y - 20.0 + 10.0;   // the right column stands on the same baseline
        std::vector<const SoundingHazard::Node *> rightNodes;
        for (const auto& node : result.nodes) {
            if (node.id[0] == 'R') {
                rightNodes.push_back(&node);
            }
        }
        double yb = std::max(bottom, 560.0);
        for (const auto * node : rightNodes) {
            const double h = node->lines.size() * lineHeight + 14.0;
            yb -= h;
            right.push_back({QRectF{640, yb, 350, h}, node});
            yb -= 20.0;
        }
        const auto fill = [] (const std::string& id) {
            if (id == "L1") return QColor{190, 175, 215};
            if (id[0] == 'L') return QColor{228, 170, 168};
            if (id == "R4") return QColor{160, 214, 230};
            if (id == "R5") return QColor{170, 200, 110};
            if (id == "R6") return QColor{160, 190, 235};
            if (id == "R7") return QColor{245, 170, 90};
            return QColor{214, 204, 168};
        };
        const auto resultColor = [] (const std::string& name) {
            if (name == "PDS TOR") return QColor{255, 0, 200};
            if (name == "TOR" || name == "MRGL TOR") return QColor{220, 0, 0};
            if (name == "SVR") return QColor{190, 170, 0};
            if (name == "MRGL SVR") return QColor{80, 150, 220};
            if (name == "FLASH FLOOD") return QColor{0, 140, 0};
            if (name == "BLIZZARD") return QColor{30, 90, 220};
            if (name == "EXCESSIVE HEAT") return QColor{240, 120, 20};
            return QColor{110, 110, 110};
        };

        p.setPen(text);
        QFont big = base;
        big.setBold(true);
        big.setPixelSize(16);
        p.setFont(big);
        p.drawText(QRectF{10, 6, 700, 24}, Qt::AlignLeft | Qt::AlignVCenter, title.isEmpty() ? QString{"Possible hazard type"} : title);
        p.drawText(QRectF{640, 6, 350, 24}, Qt::AlignRight | Qt::AlignVCenter, "Possible hazard: " + QString::fromStdString(result.type));
        p.setFont(base);

        // the result boxes between the columns
        struct Result { QString name; QRectF box; };
        std::vector<Result> results;
        const auto center = [] (const std::vector<Place>& column, const char * from, const char * to) {
            double top = 1e9, bot = -1e9;
            for (const auto& c : column) {
                if (c.node->id >= from && c.node->id <= to) {
                    top = std::min(top, c.box.top());
                    bot = std::max(bot, c.box.bottom());
                }
            }
            return (top + bot) / 2.0;
        };
        const auto addResult = [&] (const QString& name, double cy, bool onLeft) { results.push_back({name, QRectF{onLeft ? 372.0 : 508.0, cy - 17, 120, 34}}); };   // (the tornado results beside the left column, the others beside the right)
        addResult("PDS TOR", center(left, "L1", "L1"), true);
        addResult("TOR", center(left, "L2", "L4"), true);
        addResult("MRGL TOR", center(left, "L5", "L6"), true);
        addResult("SVR", center(right, "R1", "R3"), false);
        addResult("MRGL SVR", center(right, "R4", "R4"), false);
        addResult("FLASH FLOOD", center(right, "R5", "R5"), false);
        addResult("BLIZZARD", center(right, "R6", "R6"), false);
        addResult("EXCESSIVE HEAT", center(right, "R7", "R7"), false);
        addResult("NONE", right.empty() ? 40.0 : right.back().box.top() - 40.0, false);
        const auto resultBox = [&] (const QString& name) -> QRectF {
            for (const auto& r : results) {
                if (r.name == name) {
                    return r.box;
                }
            }
            return {};
        };

        // the arrows: T to the result of a node that holds, F to the next node
        const auto line = [&] (QPointF a, QPointF b, bool taken) {
            p.setPen(QPen{taken ? text : QColor{150, 150, 150, 150}, taken ? 2.0 : 1.0});
            p.drawLine(a, b);
        };
        const auto label = [&] (const QString& s, QPointF at, bool taken) {
            p.setPen(taken ? text : QColor{150, 150, 150, 170});
            p.drawText(QRectF{at.x() - 8, at.y() - 8, 16, 16}, Qt::AlignCenter, s);
        };
        const auto drawColumn = [&] (const std::vector<Place>& column, bool leftSide) {
            for (size_t i = 0; i < column.size(); i++) {
                const auto& c = column[i];
                const QRectF target = resultBox(QString::fromStdString(c.node->outcome));
                const QPointF from{leftSide ? c.box.right() : c.box.left(), c.box.center().y()};
                const QPointF to{leftSide ? target.left() : target.right(), target.center().y()};
                const bool tookT = c.node->reached && c.node->passed;
                line(from, QPointF{(from.x() + to.x()) / 2.0, from.y()}, tookT);
                line(QPointF{(from.x() + to.x()) / 2.0, from.y()}, QPointF{(from.x() + to.x()) / 2.0, to.y()}, tookT);
                line(QPointF{(from.x() + to.x()) / 2.0, to.y()}, to, tookT);
                label("T", QPointF{from.x() + (leftSide ? 14 : -14), from.y() - 8}, tookT);
                if (i + 1 < column.size()) {   // F: on to the next node
                    const auto& next = column[i + 1];
                    const bool tookF = c.node->reached && !c.node->passed;
                    const QPointF a{c.box.center().x(), leftSide ? c.box.bottom() : c.box.top()};
                    const QPointF b{c.box.center().x(), leftSide ? next.box.top() : next.box.bottom()};
                    line(a, b, tookF);
                    label("F", QPointF{a.x() + 12, (a.y() + b.y()) / 2.0}, tookF);
                }
            }
        };
        drawColumn(left, true);
        drawColumn(right, false);
        if (!left.empty() && !right.empty()) {   // the last node of the tornado column leads to the first of the other
            const bool tookF = left.back().node->reached && !left.back().node->passed;
            const double yBelow = std::max(left.back().box.bottom(), right.front().box.bottom()) + 16.0;
            line(QPointF{left.back().box.center().x(), left.back().box.bottom()}, QPointF{left.back().box.center().x(), yBelow}, tookF);
            line(QPointF{left.back().box.center().x(), yBelow}, QPointF{right.front().box.center().x(), yBelow}, tookF);
            line(QPointF{right.front().box.center().x(), yBelow}, QPointF{right.front().box.center().x(), right.front().box.bottom()}, tookF);
            label("F", QPointF{left.back().box.center().x() + 12, left.back().box.bottom() + 10}, tookF);
        }
        if (!right.empty()) {   // the last node: F gives NONE
            const bool tookF = right.back().node->reached && !right.back().node->passed;
            line(QPointF{right.back().box.center().x(), right.back().box.top()}, QPointF{right.back().box.center().x(), resultBox("NONE").bottom()}, tookF);
            line(QPointF{right.back().box.center().x(), resultBox("NONE").bottom()}, QPointF{resultBox("NONE").right(), resultBox("NONE").bottom()}, tookF);
        }

        // the result boxes
        for (const auto& r : results) {
            const bool won = r.name.toStdString() == result.type;
            const auto color = resultColor(r.name.toStdString());
            p.setPen(QPen{color, won ? 4.0 : 1.5});
            p.setBrush(won ? QColor{color.red(), color.green(), color.blue(), 60} : QColor{255, 255, 255, 20});
            p.drawRect(r.box);
            QFont f = base;
            f.setBold(true);
            f.setPixelSize(won ? 14 : 12);
            p.setFont(f);
            p.setPen(won ? color : QColor{color.red(), color.green(), color.blue(), 150});
            p.drawText(r.box, Qt::AlignCenter, r.name);
            p.setFont(base);
        }

        // the nodes with their tests
        const auto drawNode = [&] (const Place& c) {
            const auto& node = *c.node;
            QColor body = fill(node.id);
            body.setAlpha(node.reached ? 150 : 45);
            p.setPen(QPen{node.reached ? text : QColor{150, 150, 150, 120}, node.reached && node.passed ? 3.0 : 1.5});
            p.setBrush(body);
            p.drawRoundedRect(c.box, 9, 9);
            double ty = c.box.top() + 7.0;
            for (const auto& l : node.lines) {
                const QString mark = l.state == SoundingHazard::State::Pass ? QString::fromUtf8("\xE2\x9C\x93 ") : l.state == SoundingHazard::State::Fail ? QString::fromUtf8("\xE2\x9C\x97 ") : QString{"? "};
                QColor ink = l.state == SoundingHazard::State::Pass ? QColor{20, 120, 40} : l.state == SoundingHazard::State::Fail ? QColor{190, 30, 30} : QColor{120, 120, 120};
                if (!node.reached) {
                    ink.setAlpha(110);
                }
                p.setPen(ink);
                const QString shown = mark + QString::fromStdString(l.text) + "   [" + QString::fromStdString(l.value) + "]";
                p.drawText(QRectF{c.box.left() + 8, ty, c.box.width() - 12, lineHeight}, Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(shown, Qt::ElideRight, static_cast<int>(c.box.width() - 14)));
                ty += lineHeight;
            }
        };
        for (const auto& c : left) {
            drawNode(c);
        }
        for (const auto& c : right) {
            drawNode(c);
        }
        p.setPen(text);
        p.drawText(QRectF{10, 770, 980, 18}, Qt::AlignLeft | Qt::AlignVCenter, note);
    }

private:
    SoundingHazard::Result result;
    QString note, title;
};

SoundingHazardViewer::SoundingHazardViewer(Window * parent, const SoundingProfile& given, const SoundingAnalysis& analysisGiven, double latitude, double longitude, const std::string& title)
    : Window{parent}
    , profile{given}
    , analysis{analysisGiven}
    , lat{latitude}
    , lon{longitude}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Hazard type - sounding");
    auto * page = new QWidget{this};
    auto * column = new QVBoxLayout{page};
    chart = new HazardChart{page};
    column->addWidget(chart, 1);
    setCentralWidget(page);
    setSize(1100, 820);
    const QString heading = QString::fromStdString(title);
    chart->set(SoundingHazard::classify(profile, analysis), "Looking for the precipitable water climatology of the place (the flash flood test)...", heading);
    QWidget::show();
    if (const auto file = qEnvironmentVariable("WXQT_HAZARD_PNG"); !file.isEmpty()) {   // (for testing: the chart as a picture after a few seconds)
        QTimer::singleShot(qEnvironmentVariableIntValue("WXQT_HAZARD_MS") > 0 ? qEnvironmentVariableIntValue("WXQT_HAZARD_MS") : 6000, this, [this, file] { chart->grab().save(file); });
    }
    // the precipitable water climatology at the point and the day, in the background: the reanalysis' daily mean and spread (the spread is built once, in Settings)
    const int mine = ++generation;
    QDate day = QDate::currentDate();
    if (const auto stamp = QString::fromStdString(profile.validTime); stamp.size() >= 6) {   // "260930/1200"
        const auto parsed = QDate::fromString(stamp.left(6), "yyMMdd");
        if (parsed.isValid()) {
            day = parsed.year() < 2000 ? parsed.addYears(100) : parsed;
        }
    }
    struct Climate {
        double mean{-9999.0}, sigma{-9999.0};
        std::string problem;
    };
    auto found = std::make_shared<Climate>();
    const double pointLat = lat, pointLon = lon;
    new FutureVoid{this, [found, day, pointLat, pointLon] {
                       if (pointLat < -90.0 || pointLat > 90.0 || pointLon < -360.0 || pointLon > 360.0 || (pointLat == 0.0 && pointLon == 0.0)) {
                           found->problem = "the place of the sounding is not known";
                           return;
                       }
                       const GfsClimate climate{UtilityGrib::gdalBinDir()};
                       const auto field = GfsClimate::precipitableWater();
                       const int index = GfsClimate::dayIndex(day.year(), day.month(), day.day());
                       GfsGrid::Grid normal, deviation;
                       if (!climate.at(field, index, normal, found->problem)) {
                           return;
                       }
                       const auto at = [&] (const GfsGrid::Grid& grid) { return grid.sample(pointLon < 0 ? pointLon + 360.0 : pointLon, pointLat); };
                       found->mean = at(normal) / 25.4;   // kg/m2 = mm: inches
                       if (!climate.deviation(field, index, deviation, found->problem)) {
                           return;
                       }
                       found->sigma = at(deviation) / 25.4;
                   },
                   [this, found, mine, heading] {
                       if (closed || mine != generation) {
                           return;
                       }
                       SoundingHazard::Input input;
                       input.pwMeanIn = found->mean;
                       input.pwSigmaIn = found->sigma;
                       const auto result = SoundingHazard::classify(profile, analysis, input);
                       chart->set(result, result.pwKnown ? QString{"Precipitable water climatology: reanalysis daily mean and spread at the place (NCEP/NCAR 1991-2020)."}
                                                         : QString::fromStdString("Flash flood test: no precipitable water climatology (" + (found->problem.empty() ? std::string{"the place is not known"} : found->problem) + "), so it cannot hold."),
                                  heading);
                   }};
}
