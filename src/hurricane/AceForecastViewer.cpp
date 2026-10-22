// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AceForecastViewer.h"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>
#include "hurricane/EnsembleStyle.h"
#include "hurricane/UtilityWeatherLab.h"
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"
#include "util/UtilityIO.h"
#include "ui/UiStandards.h"

namespace {
    constexpr double W = 1300.0, H = 770.0;

    double niceTop(double v) {
        if (v <= 0.0) {
            return 1.0;
        }
        const double mag = std::pow(10.0, std::floor(std::log10(v)));
        for (const double m : {1.0, 1.5, 2.0, 2.5, 3.0, 4.0, 5.0, 6.0, 8.0, 10.0}) {
            if (v <= m * mag) {
                return m * mag;
            }
        }
        return 10.0 * mag;
    }
    QString number(double v, int digits = 4) {
        return QString::number(v, 'f', digits);
    }
}

class AceBoard : public QWidget {
public:
    explicit AceBoard(QWidget * parent) : QWidget{parent} {
        setMinimumSize(900, 540);
        setMouseTracking(false);
        ChartExport::install(this, "ACE forecast");
    }
    void set(const UtilityAceForecast::Summary& newSummary, const QString& newTitle, const QString& newCredit) {
        summary = newSummary;
        title = newTitle;
        credit = newCredit;
        step = summary.hours.size() > 20 ? 20 : summary.hours.empty() ? 0 : summary.hours.size() - 1;   // 120 hours
        update();
    }

protected:
    void mousePressEvent(QMouseEvent * event) override {
        if (summary.hours.empty()) {
            return;
        }
        const double scale = std::min(width() / W, height() / H);
        const QPointF p{(event->position().x() - (width() - W * scale) / 2.0) / scale, (event->position().y() - (height() - H * scale) / 2.0) / scale};
        for (const QRectF& area : {lineArea, barArea}) {
            if (area.contains(p) && area.width() > 0) {
                const double hourMax = std::max(1, summary.hours.back());
                const double hour = std::clamp((p.x() - area.left()) / area.width() * hourMax, 0.0, hourMax);
                step = static_cast<size_t>(std::lround(hour / 6.0));
                step = std::min(step, summary.hours.size() - 1);
                update();
                return;
            }
        }
    }

    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor{242, 243, 245});
        const double scale = std::min(width() / W, height() / H);
        p.translate((width() - W * scale) / 2.0, (height() - H * scale) / 2.0);
        p.scale(scale, scale);
        p.fillRect(QRectF{0, 0, W, H}, QColor{242, 243, 245});
        QFont base = p.font();
        base.setPixelSize(12);
        p.setFont(base);
        // the header band
        p.fillRect(QRectF{0, 0, W, 66}, QColor{22, 20, 120});
        QFont big = base;
        big.setPixelSize(26);
        big.setBold(true);
        p.setFont(big);
        p.setPen(Qt::white);
        p.drawText(QRectF{10, 2, W - 20, 40}, Qt::AlignLeft | Qt::AlignVCenter, "Accumulated Cyclone Energy (ACE) Output");
        p.setFont(base);
        p.drawText(QRectF{10, 42, W - 20, 20}, Qt::AlignLeft | Qt::AlignVCenter,
                   title + ".  Averages & percentiles are of the members that form the storm unless said otherwise; weighed averages are multiplied by the share that form it (0 to 1).");
        if (summary.hours.empty()) {
            p.setPen(QColor{60, 60, 60});
            p.drawText(QRectF{0, 90, W, 200}, Qt::AlignCenter, "No data.");
            return;
        }
        const auto panel = [&] (const QRectF& r, const QString& heading) {
            p.setPen(QPen{QColor{225, 225, 230}, 1.0});
            p.setBrush(Qt::white);
            p.drawRoundedRect(r, 6, 6);
            p.setPen(QColor{70, 70, 70});
            p.setFont(base);
            p.drawText(QRectF{r.left() + 12, r.top() + 6, r.width() - 24, 20}, Qt::AlignLeft | Qt::AlignVCenter, heading);
        };
        const int maxHour = std::max(6, summary.hours.back());
        const auto xOf = [&] (const QRectF& a, int hour) { return a.left() + a.width() * hour / maxHour; };

        // ---- 1: the running totals
        const QRectF p1{8, 76, 640, 320};
        panel(p1, "ACE Averages");
        lineArea = QRectF{p1.left() + 44, p1.top() + 34, p1.width() - 62, p1.height() - 100};
        double top = 0.0;
        for (const double v : summary.maximum) {
            top = std::max(top, v);
        }
        top = niceTop(top * 1.05);
        const auto yOf = [&] (const QRectF& a, double v, double t) { return a.bottom() - a.height() * v / t; };
        p.setPen(QPen{QColor{235, 235, 238}, 1.0});
        QFont small = base;
        small.setPixelSize(11);
        p.setFont(small);
        for (int i = 0; i <= 4; i++) {
            const double v = top * i / 4.0;
            const double y = yOf(lineArea, v, top);
            p.setPen(QPen{QColor{235, 235, 238}, 1.0});
            p.drawLine(QPointF{lineArea.left(), y}, QPointF{lineArea.right(), y});
            p.setPen(QColor{110, 110, 110});
            p.drawText(QRectF{p1.left() + 2, y - 8, 38, 16}, Qt::AlignRight | Qt::AlignVCenter, number(v, top < 10 ? 1 : 0));
        }
        for (int hour = 0; hour <= maxHour; hour += (maxHour > 240 ? 50 : 24)) {
            p.setPen(QColor{110, 110, 110});
            p.drawText(QRectF{xOf(lineArea, hour) - 20, lineArea.bottom() + 3, 40, 14}, Qt::AlignCenter, QString::number(hour));
        }
        struct Line {
            const std::vector<double> * values;
            QColor color;
            double width;
            Qt::PenStyle style;
            QString name;
        };
        const std::vector<Line> lines{
            {&summary.maximum, QColor{205, 205, 208}, 2.0, Qt::DotLine, "Max"},
            {&summary.p90, QColor{150, 150, 155}, 2.0, Qt::DashLine, "Conditional P90"},
            {&summary.p75, QColor{20, 20, 20}, 1.2, Qt::SolidLine, "Conditional P75"},
            {&summary.weightedMean, QColor{40, 40, 170}, 2.2, Qt::SolidLine, "Weighed Mean"},
            {&summary.mean, QColor{0, 0, 0}, 2.2, Qt::SolidLine, "Conditional Mean"},
            {&summary.median, QColor{60, 60, 60}, 1.2, Qt::SolidLine, "Conditional Median"},
            {&summary.weightedMedian, QColor{110, 110, 200}, 1.2, Qt::SolidLine, "Weighed Median"},
            {&summary.p25, QColor{20, 20, 20}, 1.2, Qt::SolidLine, "Conditional P25"},
            {&summary.p10, QColor{150, 150, 155}, 2.0, Qt::DashLine, "Conditional P10"},
            {&summary.minimum, QColor{205, 205, 208}, 2.0, Qt::DotLine, "Min"},
        };
        p.save();
        p.setClipRect(lineArea.adjusted(-2, -6, 2, 6));
        for (const auto& l : lines) {
            QPolygonF poly;
            for (size_t i = 0; i < l.values->size(); i++) {
                poly << QPointF{xOf(lineArea, summary.hours[i]), yOf(lineArea, (*l.values)[i], top)};
            }
            p.setPen(QPen{l.color, l.width, l.style, Qt::RoundCap, Qt::RoundJoin});
            p.drawPolyline(poly);
        }
        p.restore();
        {   // the hour chosen: a line and a dot on each
            const double x = xOf(lineArea, summary.hours[step]);
            p.setPen(QPen{QColor{40, 130, 250, 160}, 1.2, Qt::DashLine});
            p.drawLine(QPointF{x, lineArea.top()}, QPointF{x, lineArea.bottom()});
            for (const auto& l : lines) {
                p.setPen(Qt::NoPen);
                p.setBrush(l.color.darker(130));
                p.drawEllipse(QPointF{x, yOf(lineArea, (*l.values)[step], top)}, 3.2, 3.2);
            }
        }
        {   // the legend under the axis
            double x = p1.left() + 14, y = p1.bottom() - 28;
            p.setFont(small);
            for (const auto& l : lines) {
                const double w = QFontMetricsF{small}.horizontalAdvance(l.name) + 26;
                if (x + w > p1.right() - 8) {
                    x = p1.left() + 14;
                    y += 14;
                }
                p.setPen(QPen{l.color, 2.0, l.style});
                p.drawLine(QPointF{x, y + 6}, QPointF{x + 14, y + 6});
                p.setPen(QColor{90, 90, 90});
                p.drawText(QRectF{x + 17, y - 1, w, 14}, Qt::AlignLeft | Qt::AlignVCenter, l.name);
                x += w + 4;
            }
        }

        // ---- 2: the ACE made in each hour
        const QRectF p2{660, 76, 632, 320};
        panel(p2, "ACE Produced by Hour (Mean)");
        barArea = QRectF{p2.left() + 44, p2.top() + 34, p2.width() - 62, p2.height() - 64};
        double perHourTop = 0.0;
        for (const double v : summary.producedMean) {
            perHourTop = std::max(perHourTop, v / 6.0);
        }
        perHourTop = niceTop(perHourTop * 1.1);
        p.setFont(small);
        for (int i = 0; i <= 3; i++) {
            const double v = perHourTop * i / 3.0;
            const double y = yOf(barArea, v, perHourTop);
            p.setPen(QPen{QColor{235, 235, 238}, 1.0});
            p.drawLine(QPointF{barArea.left(), y}, QPointF{barArea.right(), y});
            p.setPen(QColor{110, 110, 110});
            p.drawText(QRectF{p2.left() + 2, y - 8, 38, 16}, Qt::AlignRight | Qt::AlignVCenter, number(v, 2));
        }
        const auto barX = [&] (size_t i) { return barArea.left() + (static_cast<double>(i) + 0.5) * barArea.width() / static_cast<double>(std::max<size_t>(1, summary.hours.size())); };   // (a bar of the first hour stays inside the axis)
        for (size_t i = 0; i < summary.hours.size(); i++) {
            if (summary.hours[i] % (maxHour > 240 ? 48 : 24) == 0) {
                p.setPen(QColor{110, 110, 110});
                p.drawText(QRectF{barX(i) - 20, barArea.bottom() + 3, 40, 14}, Qt::AlignCenter, QString::number(summary.hours[i]));
            }
        }
        const double barWidth = std::max(2.0, barArea.width() / std::max<size_t>(1, summary.hours.size()) * 0.62);
        for (size_t i = 0; i < summary.hours.size(); i++) {
            const double v = summary.producedMean[i] / 6.0;
            const double x = barX(i);
            p.setPen(Qt::NoPen);
            p.setBrush(i == step ? QColor{40, 130, 250} : QColor{170, 205, 250});
            p.drawRect(QRectF{x - barWidth / 2.0, yOf(barArea, v, perHourTop), barWidth, barArea.bottom() - yOf(barArea, v, perHourTop)});
        }

        // ---- the tiles at the hour chosen
        const auto tiles = UtilityAceForecast::tilesAt(summary, step);
        const struct { const char * name; double value; QColor fill; bool light; } cards[] = {
            {"Max", tiles.maximum, QColor{100, 175, 250}, false}, {"P90", tiles.p90, QColor{170, 170, 172}, false}, {"P75", tiles.p75, QColor{200, 200, 202}, false},
            {"Mean", tiles.mean, QColor{255, 255, 255}, false}, {"Median", tiles.median, QColor{255, 255, 255}, false}, {"Weighed Mean", tiles.weightedMean, QColor{232, 232, 234}, false},
            {"Weighed Median", tiles.weightedMedian, QColor{222, 222, 224}, false}, {"P25", tiles.p25, QColor{200, 200, 202}, false}, {"P10", tiles.p10, QColor{170, 170, 172}, false},
            {"Min", tiles.minimum, QColor{70, 150, 250}, false}};
        for (int i = 0; i < 10; i++) {
            const QRectF r{8.0 + i * 129.2, 408, 123, 64};
            p.setPen(QPen{QColor{225, 225, 230}, 1.0});
            p.setBrush(cards[i].fill);
            p.drawRoundedRect(r, 6, 6);
            p.setFont(small);
            p.setPen(QColor{50, 50, 60});
            p.drawText(QRectF{r.left() + 10, r.top() + 6, r.width() - 12, 18}, Qt::AlignLeft | Qt::AlignVCenter, cards[i].name);
            QFont value = base;
            value.setPixelSize(20);
            p.setFont(value);
            p.drawText(QRectF{r.left() + 10, r.top() + 26, r.width() - 12, 30}, Qt::AlignLeft | Qt::AlignVCenter, tiles.valid ? number(cards[i].value) : QString{"--"});
        }

        // ---- the spread of the members at the hour chosen, and over the whole life
        const auto histogram = [&] (const QRectF& r, const QString& heading, const std::vector<double>& values, int bins) {
            panel(r, heading);
            const QRectF area{r.left() + 44, r.top() + 34, r.width() - 62, r.height() - 64};
            if (values.empty()) {
                return;
            }
            double lo = 0.0, hi = *std::max_element(values.begin(), values.end());
            hi = std::max(hi, 0.5);
            std::vector<int> count(static_cast<size_t>(bins), 0);
            for (const double v : values) {
                const int b = std::clamp(static_cast<int>(v / hi * bins), 0, bins - 1);
                count[static_cast<size_t>(b)]++;
            }
            const int most = std::max(1, *std::max_element(count.begin(), count.end()));
            const double yTop = niceTop(most * 1.1);
            p.setFont(small);
            for (int i = 0; i <= 3; i++) {
                const double v = yTop * i / 3.0;
                const double y = area.bottom() - area.height() * v / yTop;
                p.setPen(QPen{QColor{235, 235, 238}, 1.0});
                p.drawLine(QPointF{area.left(), y}, QPointF{area.right(), y});
                p.setPen(QColor{110, 110, 110});
                p.drawText(QRectF{r.left() + 2, y - 8, 38, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(std::lround(v)));
            }
            for (int i = 0; i <= 5; i++) {
                const double v = lo + (hi - lo) * i / 5.0;
                p.setPen(QColor{110, 110, 110});
                p.drawText(QRectF{area.left() + area.width() * i / 5.0 - 20, area.bottom() + 3, 40, 14}, Qt::AlignCenter, QString::number(v, 'f', hi < 10 ? 1 : 0));
            }
            const double bw = area.width() / bins;
            p.setPen(Qt::NoPen);
            p.setBrush(QColor{232, 116, 59});
            for (int b = 0; b < bins; b++) {
                const double h = area.height() * count[static_cast<size_t>(b)] / yTop;
                p.drawRect(QRectF{area.left() + b * bw + bw * 0.18, area.bottom() - h, bw * 0.64, h});
            }
            const double median = UtilityAceForecast::percentile(values, 0.5);
            const double x = area.left() + area.width() * median / hi;
            p.setPen(QPen{QColor{20, 20, 20}, 2.0, Qt::DotLine});
            p.drawLine(QPointF{x, area.top()}, QPointF{x, area.bottom()});
            p.setPen(QColor{30, 30, 30});
            p.drawText(QRectF{x + 4, area.top(), 80, 14}, Qt::AlignLeft | Qt::AlignVCenter, "Median");
        };
        histogram(QRectF{8, 482, 640, 270}, QString::number(summary.hours[step]) + "h ACE Distribution", UtilityAceForecast::valuesAt(summary, step), 15);
        histogram(QRectF{660, 482, 632, 270}, "Lifetime ACE Distribution", summary.lifetime, 40);
        p.setFont(small);
        p.setPen(QColor{110, 110, 110});
        p.drawText(QRectF{8, 754, W - 16, 14}, Qt::AlignLeft | Qt::AlignVCenter, credit);
    }

private:
    UtilityAceForecast::Summary summary;
    QString title, credit;
    size_t step{0};
    QRectF lineArea, barArea;
};

AceForecastViewer::AceForecastViewer(Window * parent) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("ACE forecast - Weather Lab ensemble");
    auto * page = new QWidget{this};
    auto * column = new QVBoxLayout{page};
    auto * row = new QHBoxLayout;
    stormBox = UiStandards::combo(page);
    stormBox->setMinimumWidth(420);
    status = new QLabel{"Reading the Weather Lab large ensemble (about 37 MB)...", page};
    auto * reload = UiStandards::button(page, "Reload", "Read the Weather Lab file again");
    row->addWidget(stormBox);
    row->addWidget(reload);
    row->addWidget(status, 1);
    column->addLayout(row);
    board = new AceBoard{page};
    column->addWidget(board, 1);
    credit = new QLabel{QString::fromStdString(EnsembleStyle::deepMindCredit()), page};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    column->addWidget(credit);
    setCentralWidget(page);
    setSize(1180, 800);
    QObject::connect(stormBox, &QComboBox::currentIndexChanged, this, [this] (int index) { show(index); });
    QObject::connect(reload, &QPushButton::clicked, this, [this] { loaded.reset(); load(); });
    QWidget::show();
    load();
}

void AceForecastViewer::load() {
    static std::shared_ptr<Loaded> kept;   // the parsed run, for the session (the file is 37 MB)
    static std::mutex mutex;
    {
        std::lock_guard lock{mutex};
        if (kept && loaded == nullptr && !kept->storms.empty()) {
            loaded = kept;
        }
    }
    if (loaded) {
        fillList();
        return;
    }
    status->setText("Reading the Weather Lab large ensemble (about 37 MB)...");
    const int mine = ++generation;
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this, [result] {
                       const auto now = QDateTime::currentDateTimeUtc();
                       for (int back = 0; back < 5; back++) {
                           const auto time = now.addSecs(-static_cast<qint64>(back) * 6 * 3600);
                           char stamp[16];
                           std::snprintf(stamp, sizeof stamp, "%04d%02d%02d%02d", time.date().year(), time.date().month(), time.date().day(), time.time().hour() / 6 * 6);
                           const auto bytes = UtilityIO::downloadAsByteArray(UtilityWeatherLab::genesisUrl(stamp));
                           if (bytes.size() < 1000 || !bytes.left(4000).contains("init_time")) {
                               continue;
                           }
                           UtilityAceForecast::parse(std::string_view{bytes.constData(), static_cast<size_t>(bytes.size())}, result->storms, result->ensembleSize, result->cycle, result->error);
                           return;
                       }
                       result->error = "Weather Lab has no recent large-ensemble run (it is published several hours after its time).";
                   },
                   [this, result, mine] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (!result->error.empty() || result->storms.empty()) {
                           status->setText(QString::fromStdString(result->error.empty() ? "the file has no storms" : result->error));
                           return;
                       }
                       loaded = result;
                       static std::mutex keep;
                       std::lock_guard lock{keep};
                       fillList();
                   }};
}

void AceForecastViewer::fillList() {
    order.clear();
    for (size_t i = 0; i < loaded->storms.size(); i++) {
        order.push_back(i);
    }
    const auto exists = [] (const std::string& track) { return !track.empty() && std::isalpha(static_cast<unsigned char>(track[0])) != 0; };
    std::stable_sort(order.begin(), order.end(), [&] (size_t a, size_t b) {
        const auto& sa = loaded->storms[a];
        const auto& sb = loaded->storms[b];
        if (exists(sa.track) != exists(sb.track)) {
            return exists(sa.track);
        }
        return sa.members.size() > sb.members.size();
    });
    stormBox->blockSignals(true);
    stormBox->clear();
    for (const size_t i : order) {
        const auto& s = loaded->storms[i];
        const int percent = static_cast<int>(std::lround(100.0 * static_cast<double>(s.members.size()) / std::max(1, loaded->ensembleSize)));
        stormBox->addItem(exists(s.track) ? QString::fromStdString(s.track) + "  (exists)  " + QString::number(percent) + "% of the members have it"
                                          : "Possible storm " + QString::fromStdString(s.track) + "  -  " + QString::number(percent) + "% of the members form it");
    }
    stormBox->blockSignals(false);
    status->setText("Run " + QString::fromStdString(loaded->cycle) + "Z, " + QString::number(loaded->ensembleSize) + " members, " + QString::number(order.size()) + " storms.");
    show(0);
}

void AceForecastViewer::show(int index) {
    if (!loaded || index < 0 || index >= static_cast<int>(order.size())) {
        return;
    }
    const auto& storm = loaded->storms[order[static_cast<size_t>(index)]];
    const auto summary = UtilityAceForecast::summarize(storm, loaded->ensembleSize);
    const QString title = (!storm.track.empty() && std::isalpha(static_cast<unsigned char>(storm.track[0])) ? QString::fromStdString(storm.track) : "possible storm " + QString::fromStdString(storm.track)) +
                          ", run " + QString::fromStdString(loaded->cycle) + "Z, formed in " + QString::number(summary.formed) + " of " + QString::number(summary.ensembleSize) + " members";
    board->set(summary, title, "Google DeepMind Weather Lab FNV3 large ensemble (experimental; (c) 2024-6 Google LLC, not for real world use). ACE = sum of (wind kt)^2 / 10^4 at 6 hourly steps at 34 kt or more.");
}
