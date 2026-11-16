// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "misc/ForecastPointExtended.h"
#include <algorithm>
#include <cmath>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QVBoxLayout>
#include "gfs/GfsRender.h"
#include "objects/FutureVoid.h"
#include "util/DisplayUnits.h"
#include "ui/Window.h"

namespace {
    constexpr int columnWidth = 46, labelWidth = 112;

    enum class Kind { Day, Hour, Title, TempGraph, WindGraph, PrecipGraph, Line };

    struct Row {
        Kind kind;
        QString label;
        int height;
        std::string code;     // Line: the bulletin's code (or a special name)
        int top{0};
    };

    struct Scale {
        double lo{0}, hi{1};
        double y(double v, int top, int height) const { return top + height - 4 - (v - lo) / (hi - lo) * (height - 8); }
    };

    struct Model {
        std::vector<NbmText::Column> columns;
        std::vector<Row> rows;
        Scale temp, wind, rain;
        int height{0};
        QString tempUnit{"F"}, windUnit{"kt"};   // what the numbers are in (the bulletins are in F and knots; the page shows the Settings' units)
        bool some(const std::function<bool(const NbmText::Column&)>& test) const { return std::any_of(columns.begin(), columns.end(), test); }
    };

    // the chance of a column: 12 hours when it has it, else 6
    int chance(const NbmText::Column& c, bool& sixHour) {
        sixHour = !c.has("P12") && c.has("P06");
        return c.has("P12") ? c.get("P12") : c.get("P06");
    }
    int rainOf(const NbmText::Column& c) {
        return c.has("Q12") ? c.get("Q12") : c.get("Q06");
    }
    int thunder(const NbmText::Column& c) {
        return c.has("T12") ? c.get("T12") : c.get("T06");
    }

    // The bulletins' numbers in the person's units (Settings): F and knots become F and mph (the US setting) or C and m/s. The spread (TSD) is a difference, so it has no offset. Rounded to whole
    // numbers like the bulletins' own.
    std::vector<NbmText::Column> inDisplayUnits(std::vector<NbmText::Column> columns, bool us) {
        for (auto& c : columns) {
            for (auto& [code, v] : c.value) {
                if (us) {
                    if (code == "WSP" || code == "GST") {
                        v = static_cast<int>(std::lround(v * 1.15078));
                    }
                } else if (code == "TMP" || code == "DPT" || code == "TXN") {
                    v = static_cast<int>(std::lround((v - 32) / 1.8));
                } else if (code == "TSD" || code == "XND") {
                    v = static_cast<int>(std::lround(v / 1.8));
                } else if (code == "WSP" || code == "GST") {
                    v = static_cast<int>(std::lround(v * 0.514444));
                }
            }
        }
        return columns;
    }

    Model build(const std::vector<NbmText::Column>& columns, bool us = true) {
        Model m;
        m.columns = columns;
        m.tempUnit = us ? "F" : "C";
        m.windUnit = us ? "mph" : "m/s";
        const auto any = [&m] (const char * code, int above) { return m.some([&] (const NbmText::Column& c) { return c.get(code, 0) > above; }); };
        m.rows = {{Kind::Day, "", 18, ""}, {Kind::Hour, "UTC", 18, ""}, {Kind::Title, "Temp, dew", 20, ""}, {Kind::TempGraph, "", 140, ""}, {Kind::Line, "Temp " + m.tempUnit, 18, "TMP"}, {Kind::Line, "+/- spread", 18, "TSD"}, {Kind::Line, "Dewpt " + m.tempUnit, 18, "DPT"}};
        if (m.some([] (const NbmText::Column& c) { return c.has("TXN"); })) {
            m.rows.push_back({Kind::Line, "Hi / Lo", 18, "TXN"});
        }
        m.rows.push_back({Kind::Line, "Sky %", 18, "SKY"});
        m.rows.push_back({Kind::Title, "Wind, gust", 20, ""});
        m.rows.push_back({Kind::WindGraph, "", 110, ""});
        m.rows.push_back({Kind::Line, "Speed " + m.windUnit, 18, "WSP"});
        m.rows.push_back({Kind::Line, "Gust " + m.windUnit, 18, "GST"});
        m.rows.push_back({Kind::Title, "Precip %", 20, ""});
        m.rows.push_back({Kind::PrecipGraph, "", 90, ""});
        m.rows.push_back({Kind::Line, "Chance %", 18, "CHANCE"});
        m.rows.push_back({Kind::Line, "Rain in", 18, "RAIN"});
        if (any("S12", 0) || any("S24", 0)) {
            m.rows.push_back({Kind::Line, "Snow in", 18, "SNOW"});
        }
        if (any("I12", 0)) {
            m.rows.push_back({Kind::Line, "Ice in", 18, "ICE"});
        }
        if (m.some([] (const NbmText::Column& c) { return thunder(c) > 0; })) {
            m.rows.push_back({Kind::Line, "T-storm %", 18, "TSTORM"});
        }
        if (any("SLV", 0)) {
            m.rows.push_back({Kind::Line, "Snow kft", 18, "SLV"});
        }
        if (m.some([] (const NbmText::Column& c) { return c.has("PM25"); })) {   // the RRFS's fine particles at the place (its 6 hourly runs reach 84 hours)
            m.rows.push_back({Kind::Title, "Air quality", 20, ""});
            m.rows.push_back({Kind::Line, "Smoke+dust ug/m3", 18, "PM25"});   // (the RRFS models the fires' smoke and the dust only: no traffic or industry, so clean air reads near 0)
        }
        int y = 0;
        for (auto& row : m.rows) {
            row.top = y;
            y += row.height;
        }
        m.height = y;
        double lo = 1e9, hi = -1e9, wmax = 10;
        for (const auto& c : columns) {
            for (const char * code : {"TMP", "DPT"}) {
                if (c.has(code)) {
                    lo = std::min<double>(lo, c.get(code) - (std::string{code} == "TMP" ? c.get("TSD", 0) : 0));
                    hi = std::max<double>(hi, c.get(code) + (std::string{code} == "TMP" ? c.get("TSD", 0) : 0));
                }
            }
            wmax = std::max<double>(wmax, std::max(c.get("WSP", 0), c.get("GST", 0)));
        }
        if (lo > hi) {
            lo = 0;
            hi = 10;
        }
        m.temp = {std::floor((lo - 2) / 10) * 10, std::ceil((hi + 2) / 10) * 10};
        m.wind = {0, std::ceil((wmax + 2) / 10) * 10};
        m.rain = {0, 100};
        return m;
    }

    QString cellText(const NbmText::Column& c, const std::string& code, const QDateTime& when) {
        const auto num = [&c] (const char * k, double div = 1.0, int dec = 0) { return c.has(k) ? QString::number(c.get(k) / div, 'f', dec) : QString{}; };
        if (code == "TXN") {
            if (!c.has("TXN")) {
                return {};
            }
            const int h = when.time().hour();
            return h == 0 ? QString{"H "} + num("TXN") : h == 12 ? QString{"L "} + num("TXN") : QString{};
        }
        if (code == "CHANCE") {
            bool six = false;
            const int v = chance(c, six);
            return v == NbmText::missing ? QString{} : QString::number(v);
        }
        if (code == "RAIN") {
            const int q = rainOf(c);
            return q == NbmText::missing ? QString{} : QString::number(q / 100.0, 'f', 2);
        }
        if (code == "SNOW") {
            const int s = c.has("S12") ? c.get("S12") : c.get("S24");
            return s == NbmText::missing || s == 0 ? QString{} : QString::number(s / 10.0, 'f', 1);
        }
        if (code == "ICE") {
            return c.get("I12", 0) > 0 ? QString::number(c.get("I12") / 100.0, 'f', 2) : QString{};
        }
        if (code == "TSTORM") {
            const int t = thunder(c);
            return t == NbmText::missing ? QString{} : QString::number(t);
        }
        if (code == "SLV") {
            return c.has("SLV") ? QString::number(c.get("SLV") / 10.0, 'f', 1) : QString{};
        }
        if (code == "PM25") {
            return c.has("PM25") ? QString::number(c.get("PM25") / 10.0, 'f', c.get("PM25") < 100 ? 1 : 0) : QString{};
        }
        if (code == "TSD") {
            return c.has("TSD") ? QString{"+/-"} + QString::number(c.get("TSD")) : QString{};
        }
        return num(code.c_str());
    }
}

// the part that scrolls: every column of the table and the graphs
class ExtendedCanvas : public QWidget {
public:
    ExtendedCanvas(ForecastPointExtended * page, QWidget * parent) : QWidget{parent}, page{page} {
        setMouseTracking(true);
    }
    void setColumns(const std::vector<NbmText::Column>& columns) {
        model = build(inDisplayUnits(columns, DisplayUnits::us()), DisplayUnits::us());
        selected = -1;
        setFixedSize(static_cast<int>(model.columns.size()) * columnWidth, model.height);
        update();
    }
    const Model& get() const { return model; }
    int selectedColumn() const { return selected; }
    void select(int column) {
        selected = column;
        update();
    }

protected:
    void mousePressEvent(QMouseEvent * event) override {
        const int c = static_cast<int>(event->position().x()) / columnWidth;
        if (c >= 0 && c < static_cast<int>(model.columns.size())) {
            page->picked(selected == c ? -1 : c);
        }
    }
    void paintEvent(QPaintEvent * event) override {
        QPainter p{this};
        p.setRenderHint(QPainter::Antialiasing);
        const auto text = palette().color(QPalette::WindowText);
        const auto muted = palette().color(QPalette::Mid);
        const bool dark = palette().color(QPalette::Window).lightness() < 128;
        p.fillRect(rect(), palette().color(QPalette::Base));
        const int from = std::max(0, event->rect().left() / columnWidth), to = std::min(static_cast<int>(model.columns.size()) - 1, event->rect().right() / columnWidth);
        QFont small = font();
        small.setPointSizeF(std::max(7.0, font().pointSizeF() - 1));
        p.setFont(small);
        for (int c = from; c <= to; c++) {
            const auto& col = model.columns[static_cast<size_t>(c)];
            const QRect cell{c * columnWidth, 0, columnWidth, model.height};
            if (col.source == "API") {
                p.fillRect(cell, dark ? QColor{70, 62, 30} : QColor{255, 247, 220});
            }
            if (c == selected) {
                p.fillRect(cell, dark ? QColor{40, 70, 110} : QColor{208, 226, 250});
            }
        }
        for (const auto& row : model.rows) {
            const QRect band{0, row.top, width(), row.height};
            if (row.kind == Kind::Line) {
                p.setPen(muted);
                p.drawLine(0, row.top, width(), row.top);
            }
            for (int c = from; c <= to; c++) {
                const auto& col = model.columns[static_cast<size_t>(c)];
                const auto when = QDateTime::fromSecsSinceEpoch(col.time, QTimeZone::utc());
                const QRect cell{c * columnWidth, row.top, columnWidth, row.height};
                if (row.kind == Kind::Day && (c == 0 || QDateTime::fromSecsSinceEpoch(model.columns[static_cast<size_t>(c) - 1].time, QTimeZone::utc()).date() != when.date())) {
                    p.setPen(text);
                    QFont bold = small;
                    bold.setBold(true);
                    p.setFont(bold);
                    p.drawText(cell.adjusted(3, 0, 200, 0), Qt::AlignLeft | Qt::AlignVCenter, when.toString("ddd d MMM"));
                    p.setFont(small);
                } else if (row.kind == Kind::Hour) {
                    p.setPen(text);
                    p.drawText(cell, Qt::AlignCenter, when.toString("HH") + "Z");
                } else if (row.kind == Kind::Line) {
                    const bool sky = row.code == "SKY" && col.has("SKY");
                    if (sky) {
                        const int v = col.get("SKY");
                        p.fillRect(cell.adjusted(1, 1, -1, -1), dark ? QColor{50 + v / 2, 55 + v / 2, 65 + v / 2} : QColor{255 - v * 100 / 100, 255 - v * 90 / 100, 255 - v * 55 / 100});
                    }
                    p.setPen(text);
                    p.drawText(cell, Qt::AlignCenter, cellText(col, row.code, when));
                }
            }
            (void) band;
        }
        // the lines between days
        p.setPen(QPen{muted, 1, Qt::DotLine});
        for (int c = std::max(1, from); c <= to; c++) {
            if (QDateTime::fromSecsSinceEpoch(model.columns[static_cast<size_t>(c) - 1].time, QTimeZone::utc()).date() != QDateTime::fromSecsSinceEpoch(model.columns[static_cast<size_t>(c)].time, QTimeZone::utc()).date()) {
                p.drawLine(c * columnWidth, 0, c * columnWidth, model.height);
            }
        }
        const auto centre = [] (int c) { return c * columnWidth + columnWidth / 2.0; };
        for (const auto& row : model.rows) {
            if (row.kind == Kind::TempGraph) {
                for (double g = model.temp.lo; g <= model.temp.hi; g += 10) {
                    const double y = model.temp.y(g, row.top, row.height);
                    p.setPen(QPen{g == 32 ? QColor{70, 130, 220} : muted, 1, g == 32 ? Qt::DashLine : Qt::DotLine});
                    p.drawLine(0, static_cast<int>(y), width(), static_cast<int>(y));
                }
                if (32 > model.temp.lo && 32 < model.temp.hi) {
                    const double y = model.temp.y(32, row.top, row.height);
                    p.setPen(QPen{QColor{70, 130, 220}, 1, Qt::DashLine});
                    p.drawLine(0, static_cast<int>(y), width(), static_cast<int>(y));
                }
                QPainterPath upper, lower;
                std::vector<QPointF> up, down;
                for (int c = from > 0 ? from - 1 : 0; c <= std::min(to + 1, static_cast<int>(model.columns.size()) - 1); c++) {
                    const auto& col = model.columns[static_cast<size_t>(c)];
                    if (col.has("TMP") && col.has("TSD")) {
                        up.push_back({centre(c), model.temp.y(col.get("TMP") + col.get("TSD"), row.top, row.height)});
                        down.push_back({centre(c), model.temp.y(col.get("TMP") - col.get("TSD"), row.top, row.height)});
                    }
                }
                if (up.size() > 1) {
                    QPolygonF band;
                    for (const auto& pt : up) {
                        band << pt;
                    }
                    for (auto it = down.rbegin(); it != down.rend(); ++it) {
                        band << *it;
                    }
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor{220, 80, 60, 55});
                    p.drawPolygon(band);
                    p.setBrush(Qt::NoBrush);
                }
                line(p, row, "DPT", model.temp, centre, QPen{QColor{40, 160, 90}, 1.6, Qt::DashLine}, from, to);
                line(p, row, "TMP", model.temp, centre, QPen{QColor{210, 60, 40}, 2.0}, from, to);
            } else if (row.kind == Kind::WindGraph) {
                for (double g = 0; g <= model.wind.hi; g += 10) {
                    const double y = model.wind.y(g, row.top, row.height - 16);
                    p.setPen(QPen{muted, 1, Qt::DotLine});
                    p.drawLine(0, static_cast<int>(y), width(), static_cast<int>(y));
                }
                line(p, row, "GST", model.wind, centre, QPen{QColor{150, 90, 190}, 1.4, Qt::DashLine}, from, to, 16);
                line(p, row, "WSP", model.wind, centre, QPen{QColor{40, 100, 200}, 2.0}, from, to, 16);
                p.setPen(QPen{text, 1.2});
                for (int c = from; c <= to; c++) {
                    const auto& col = model.columns[static_cast<size_t>(c)];
                    if (!col.has("WDR") || col.get("WSP", 0) <= 0) {
                        continue;
                    }
                    const double toward = (col.get("WDR") * 10 + 180) * M_PI / 180.0;   // (WDR is where the wind comes from)
                    const QPointF mid{centre(c), row.top + row.height - 8.0};
                    const QPointF d{std::sin(toward) * 6.5, -std::cos(toward) * 6.5};
                    p.drawLine(mid - d, mid + d);
                    const QPointF tip = mid + d;
                    for (const double a : {0.45, -0.45}) {
                        p.drawLine(tip, tip - QPointF{d.x() * 0.6 * std::cos(a) - d.y() * 0.6 * std::sin(a), d.x() * 0.6 * std::sin(a) + d.y() * 0.6 * std::cos(a)});
                    }
                }
            } else if (row.kind == Kind::PrecipGraph) {
                for (double g = 0; g <= 100; g += 25) {
                    const double y = model.rain.y(g, row.top, row.height);
                    p.setPen(QPen{muted, 1, Qt::DotLine});
                    p.drawLine(0, static_cast<int>(y), width(), static_cast<int>(y));
                }
                p.setPen(Qt::NoPen);
                for (int c = from; c <= to; c++) {
                    const auto& col = model.columns[static_cast<size_t>(c)];
                    bool six = false;
                    const int v = chance(col, six);
                    if (v == NbmText::missing) {
                        continue;
                    }
                    const bool snow = col.get("PSN", 0) > 50;
                    QColor fill = snow ? QColor{70, 110, 230} : QColor{60, 170, 90};
                    fill.setAlpha(six ? 110 : 220);
                    p.setBrush(fill);
                    const double y = model.rain.y(v, row.top, row.height);
                    p.drawRect(QRectF{c * columnWidth + 8.0, y, columnWidth - 16.0, row.top + row.height - 4 - y});
                }
                p.setBrush(Qt::NoBrush);
            }
        }
    }

private:
    void line(QPainter& p, const Row& row, const char * code, const Scale& scale, const std::function<double(int)>& centre, const QPen& pen, int from, int to, int bottomMargin = 0) const {
        p.setPen(pen);
        bool have = false;
        QPointF last;
        for (int c = from > 0 ? from - 1 : 0; c <= std::min(to + 1, static_cast<int>(model.columns.size()) - 1); c++) {
            const auto& col = model.columns[static_cast<size_t>(c)];
            if (!col.has(code)) {
                have = false;
                continue;
            }
            const QPointF pt{centre(c), scale.y(col.get(code), row.top, row.height - bottomMargin)};
            if (have) {
                p.drawLine(last, pt);
            }
            last = pt;
            have = true;
        }
    }
    ForecastPointExtended * page;
    Model model;
    int selected{-1};
};

// the fixed column of names and scales
class ExtendedLabels : public QWidget {
public:
    explicit ExtendedLabels(QWidget * parent) : QWidget{parent} {
        setFixedWidth(labelWidth);
    }
    void setCanvas(ExtendedCanvas * c) {
        canvas = c;
        setFixedHeight(c->get().height);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p{this};
        p.fillRect(rect(), palette().color(QPalette::Base));
        if (canvas == nullptr) {
            return;
        }
        const auto& m = canvas->get();
        QFont small = font();
        small.setPointSizeF(std::max(7.0, font().pointSizeF() - 1));
        p.setFont(small);
        const auto muted = palette().color(QPalette::Mid);
        for (const auto& row : m.rows) {
            if (row.kind == Kind::Line) {
                p.setPen(muted);
                p.drawLine(0, row.top, width(), row.top);
            }
            p.setPen(palette().color(QPalette::WindowText));
            if (row.kind == Kind::Line || row.kind == Kind::Hour || row.kind == Kind::Title) {
                p.drawText(QRect{4, row.top, width() - 8, row.height}, Qt::AlignLeft | Qt::AlignVCenter, row.label);
            } else if (row.kind == Kind::TempGraph) {
                for (double g = m.temp.lo; g <= m.temp.hi; g += 10) {
                    p.drawText(QRect{0, static_cast<int>(m.temp.y(g, row.top, row.height)) - 8, width() - 6, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(g)) + (g == m.temp.lo ? " " + m.tempUnit : QString{}));
                }
            } else if (row.kind == Kind::WindGraph) {
                for (double g = 0; g <= m.wind.hi; g += 10) {
                    p.drawText(QRect{0, static_cast<int>(m.wind.y(g, row.top, row.height - 16)) - 8, width() - 6, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(g)) + (g == 0 ? " " + m.windUnit : QString{}));
                }
            } else if (row.kind == Kind::PrecipGraph) {
                for (double g = 0; g <= 100; g += 25) {
                    p.drawText(QRect{0, static_cast<int>(m.rain.y(g, row.top, row.height)) - 8, width() - 6, 16}, Qt::AlignRight | Qt::AlignVCenter, QString::number(static_cast<int>(g)) + (g == 0 ? " %" : ""));
                }
            }
        }
    }

private:
    ExtendedCanvas * canvas{nullptr};
};

// The RRFS's near-surface fine particles (PM2.5, the hour's mean at 8 m; its smoke and dust only, the model has no traffic or industry) at the place, at the columns' times of its newest 6 hourly run (the runs between carry 18 hours only): a
// line under the precipitation, in micrograms a cubic meter (the US air quality index: good to 9, moderate to 35.4, unhealthy for sensitive groups to 55.4). Nothing outside the RRFS's map.
void ForecastPointExtended::addAirQuality(NbmText::Table& table, double lat, double lon) {
    std::string cycle, date;
    if (!GfsRender::latestCycle("RRFS", cycle, "", &date)) {
        return;
    }
    auto run = QDateTime{QDate::fromString(QString::fromStdString(date), "yyyyMMdd"), QTime{std::atoi(cycle.c_str()) / 6 * 6, 0}, QTimeZone::utc()};
    GfsRender::Session session;
    const GfsData::Want pm25{"", "MASSDEN", "8 m above ground", "", "aerosol=Total aerosol|aerosol_size <2.5e-06"};
    for (int attempt = 0; attempt < 2; attempt++, run = run.addSecs(-6 * 3600)) {   // the newest 6 hourly run may still be coming in: the one before
        std::vector<int> hours;
        for (const auto& c : table.columns) {
            const qint64 h = (c.time - run.toSecsSinceEpoch()) / 3600;
            if ((c.time - run.toSecsSinceEpoch()) % 3600 == 0 && h >= 1 && h <= 84 && h % 3 == 0 && (hours.empty() || hours.back() != static_cast<int>(h))) {
                hours.push_back(static_cast<int>(h));
            }
        }
        std::string error;
        const auto values = GfsRender::pointSeries(session, "RRFS", run.toString("yyyyMMddHH").toStdString(), hours, pm25, lat, lon, error);
        int found = 0;
        for (auto& c : table.columns) {
            const qint64 h = (c.time - run.toSecsSinceEpoch()) / 3600;
            if (const auto v = values.find(static_cast<int>(h)); v != values.end() && !std::isnan(v->second) && (c.time - run.toSecsSinceEpoch()) % 3600 == 0) {
                c.value["PM25"] = static_cast<int>(std::lround(v->second * 1e10));   // (tenths of a microgram: clean air is under one)
                found++;
            }
        }
        if (found > 0) {
            table.cycles["RRFS"] = run.toSecsSinceEpoch();
            return;
        }
    }
}

ForecastPointExtended::ForecastPointExtended(Window * owner, QWidget * parent) : QWidget{parent}, owner{owner} {
    auto * column = new QVBoxLayout{this};
    column->setContentsMargins(0, 4, 0, 0);
    status = new QLabel{this};
    status->setWordWrap(true);
    column->addWidget(status);
    auto * row = new QHBoxLayout;
    row->setSpacing(0);
    labels = new ExtendedLabels{this};
    scroll = new QScrollArea{this};
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    canvas = new ExtendedCanvas{this, scroll};
    scroll->setWidget(canvas);
    scroll->setWidgetResizable(false);
    auto * left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->addWidget(labels);
    left->addStretch();
    row->addLayout(left);
    row->addWidget(scroll, 1);
    column->addLayout(row, 1);
    auto * bar = new QHBoxLayout;
    pickText = new QLabel{"Click a column to pick a time.", this};
    pickButton = new QPushButton{"Heights && winds (REFS, else GFS)", this};
    pickButton->setEnabled(false);
    bar->addWidget(pickText, 1);
    bar->addWidget(pickButton);
    column->addLayout(bar);
    QObject::connect(pickButton, &QPushButton::clicked, [this] {
        const int c = canvas->selectedColumn();
        if (c >= 0 && openSynoptic) {
            openSynoptic(QDateTime::fromSecsSinceEpoch(canvas->get().columns[static_cast<size_t>(c)].time, QTimeZone::utc()));
        }
    });
    status->setText("Open this page to load the extended forecast.");
}

void ForecastPointExtended::setPoint(const std::shared_ptr<UtilityForecastPoint::Data>& point) {
    data = point;
    stale = true;
    table.reset();
    ++generation;
    canvas->setColumns({});
    labels->setCanvas(canvas);
    picked(-1);
    if (isVisible()) {
        showing();
    }
}

void ForecastPointExtended::showing() {
    if (data && (stale || (loadedAt != 0 && QDateTime::currentSecsSinceEpoch() - loadedAt > 1800))) {
        load();
    }
}

void ForecastPointExtended::load() {
    stale = false;
    const int mine = ++generation;
    status->setText("Loading the National Blend of Models bulletins for this place (the first time takes a few seconds)...");
    auto result = std::make_shared<NbmText::Table>();
    auto grid = data;
    new FutureVoid{owner, [result, grid] {
                       *result = NbmText::fetch(*grid);
                       if (result->ok) {
                           addAirQuality(*result, grid->lat, grid->lon);
                       }
                   }, [this, result, mine] {
                       if (mine != generation) {
                           return;
                       }
                       table = result;
                       loadedAt = QDateTime::currentSecsSinceEpoch();
                       fill();
                   }};
}

void ForecastPointExtended::fill() {
    if (!table || !table->ok) {
        status->setText("<b>" + QString::fromStdString(table ? table->error : std::string{"No data."}) + "</b>");
        canvas->setColumns({});
        labels->setCanvas(canvas);
        return;
    }
    const auto when = [] (qint64 t) { return QDateTime::fromSecsSinceEpoch(t, QTimeZone::utc()).toString("ddd d HH'Z'"); };
    QString from = "NWS grid";
    if (table->gridUpdated != 0) {
        qint64 lastGrid = 0;
        for (const auto& c : table->columns) {
            if (c.source == "API") {
                lastGrid = c.time;
            }
        }
        from += lastGrid != 0 ? " to " + when(lastGrid) : QString{" (no column of it)"};
    }
    for (const char * k : {"NBS", "NBE", "NBX", "RRFS"}) {
        if (table->cycles.count(k)) {
            from += QString{" - "} + k + " " + QDateTime::fromSecsSinceEpoch(table->cycles.at(k), QTimeZone::utc()).toString("HH'Z'");
        }
    }
    status->setText("<b>" + QString::fromStdString(table->station) + "</b> " + QString::fromStdString(table->name) + "  (the National Blend of Models bulletins of its nearest station)  -  " + from + "  -  times are UTC; the shaded columns are the NWS grid");
    canvas->setColumns(table->columns);
    labels->setCanvas(canvas);
    picked(-1);
}

void ForecastPointExtended::picked(int column) {
    canvas->select(column);
    const auto& columns = canvas->get().columns;
    if (column < 0 || column >= static_cast<int>(columns.size())) {
        pickText->setText("Click a column to pick a time.");
        pickButton->setEnabled(false);
        return;
    }
    const auto t = QDateTime::fromSecsSinceEpoch(columns[static_cast<size_t>(column)].time, QTimeZone::utc());
    pickText->setText("Valid " + t.toString("dddd d MMMM HH:mm 'UTC'") + " (" + t.toLocalTime().toString("ddd h ap") + " local)  -  " + QString::fromStdString(columns[static_cast<size_t>(column)].source));
    pickButton->setEnabled(true);
}
