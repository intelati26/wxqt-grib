// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AtcfFeedViewer.h"
#include <algorithm>
#include <memory>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "util/UtilityGzip.h"
#include "ui/UiStandards.h"

namespace {
    // a best track line, the fields this window works with
    struct Row {
        QString dtg, cy, type, name;
        int wind{0};
        double lat{0}, lon{0};
        bool ok{false};
    };

    double tenths(const QString& field) {
        const auto text = field.trimmed();
        if (text.size() < 2) {
            return 0.0;
        }
        const double value = text.left(text.size() - 1).toDouble() / 10.0;
        return (text.back() == 'S' || text.back() == 'W') ? -value : value;
    }

    Row parseRow(const QString& line) {
        const auto f = line.split(',');
        Row r;
        if (f.size() < 11) {
            return r;
        }
        r.cy = f[1].trimmed();
        r.dtg = f[2].trimmed();
        r.wind = f[8].trimmed().toInt();
        r.type = f[10].trimmed();
        r.name = f.size() > 27 ? f[27].trimmed() : QString{};
        r.lat = tenths(f[6]);
        r.lon = tenths(f[7]);
        r.ok = r.dtg.size() >= 10;
        return r;
    }

    QString category(int wind) {
        return wind < 34 ? "TD" : wind < 64 ? "TS" : wind < 83 ? "Cat 1" : wind < 96 ? "Cat 2" : wind < 113 ? "Cat 3" : wind < 137 ? "Cat 4" : "Cat 5";
    }

    QString shortTime(const QString& dtg) {
        const auto when = QDateTime::fromString(dtg, "yyyyMMddHH");
        return when.isValid() ? when.toString("MMM d HH'Z'") : dtg;
    }

    // what changed from one line to the next, in words ("" when nothing of the status): the type (DB, TD, TS, HU, LO, EX ...) and the name (GENESIS022, INVEST, NINE, ISAIAS)
    QString statusChange(const Row& before, const Row& now) {
        QStringList parts;
        if (before.type != now.type) {
            parts << before.type + " -> " + now.type;
        }
        if (before.name != now.name) {
            parts << (now.name.isEmpty() ? QString{"name dropped"} : (before.name.isEmpty() ? QString{} : before.name + " -> ") + now.name);
        }
        return parts.join(", ");
    }

    bool nhcBasin(const string& id) {
        const auto basin = id.substr(0, 2);
        return basin == "al" || basin == "ep" || basin == "cp";
    }

    // the deck as text (the newest part last); empty with the reason in `why`
    string fetchDeck(const string& id, int which, string& why) {
        const string year = id.size() >= 4 ? id.substr(id.size() - 4) : string{};
        string url;
        if (nhcBasin(id)) {
            url = which == 0 ? "https://ftp.nhc.noaa.gov/atcf/btk/b" + id + ".dat" : which == 1 ? "https://ftp.nhc.noaa.gov/atcf/aid_public/a" + id + ".dat.gz" : "https://ftp.nhc.noaa.gov/atcf/fix/f" + id + ".dat";
        } else if (which == 0) {
            url = "https://hurricanes.ral.ucar.edu/repository/data/bdecks_open/" + year + "/b" + id + ".dat";
        } else {
            why = "the guidance and fix files of this basin are not public in time (UCAR keeps them only as an archive of past years)";
            return {};
        }
        const auto bytes = URL::getBytes(url);
        if (bytes.size() < 20) {
            why = "no answer for " + url;
            return {};
        }
        string text{bytes.constData(), static_cast<size_t>(bytes.size())};
        if (url.size() > 3 && url.compare(url.size() - 3, 3, ".gz") == 0) {
            string plain;
            if (!UtilityGzip::gunzip(text, plain) || plain.empty()) {
                why = "the file could not be unpacked";
                return {};
            }
            text = std::move(plain);
        }
        return text;
    }

    QString nice(const QString& field) {
        return field.trimmed();
    }

    // ATCF writes a position in tenths of a degree with its hemisphere: "124N", "1486E"
    QString degrees(const QString& field) {
        const auto text = field.trimmed();
        if (text.size() < 2 || !(text.back() == 'N' || text.back() == 'S' || text.back() == 'E' || text.back() == 'W')) {
            return text;
        }
        bool ok = false;
        const double value = text.left(text.size() - 1).toDouble(&ok) / 10.0;
        return ok ? QString::number(value, 'f', 1) + text.back() : text;
    }
}

AtcfFeedViewer::AtcfFeedViewer(Window * parent, const string& id, const string& title)
    : Window{parent}
    , stormId{id}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("ATCF feed - " + title);
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    deck = UiStandards::combo(top);
    deck->addItem("Best track (b-deck)");
    deck->addItem("Guidance, every model (a-deck)");
    deck->addItem("Fixes, satellite and aircraft (f-deck)");
    view = UiStandards::combo(top);
    view->addItem("every line");
    view->addItem("status changes (invest, TD, TS, hurricane, post-tropical ...)");
    view->addItem("category changes (TD, TS, Cat 1 ... 5)");
    view->setToolTip("The best track only: keep the lines where the storm changes status or category, with what changed");
    count = UiStandards::combo(top);
    for (const int n : {30, 100, 300, 1000}) {
        count->addItem("last " + QString::number(n) + " lines", n);
    }
    filter = new QLineEdit{top};
    filter->setPlaceholderText("only lines with ...  (OFCL, GFS, HWRF ...)");
    filter->setClearButtonEnabled(true);
    refresh = new QCheckBox{"Look again every 5 minutes", top};
    refresh->setChecked(true);
    auto * reload = UiStandards::button(top, "Refresh");
    auto * copy = UiStandards::button(top, "Copy");
    topLayout->addWidget(deck);
    topLayout->addWidget(view);
    topLayout->addWidget(count);
    topLayout->addWidget(filter, 1);
    topLayout->addWidget(refresh);
    topLayout->addWidget(reload);
    topLayout->addWidget(copy);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    status = new QLabel{"Reading the deck...", this};
    status->setWordWrap(true);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    timeline = new QLabel{this};
    timeline->setWordWrap(true);
    timeline->setStyleSheet("font-weight: bold;");
    box.addWidgetReal(timeline, 0, Qt::Alignment{});
    tabs = new QTabWidget{this};
    table = new QTableWidget{this};
    table->setColumnCount(15);
    table->setHorizontalHeaderLabels({"Change", "Time (UTC)", "Tech", "Tau", "Latitude", "Longitude", "Wind kt", "MSLP mb", "Type", "Rad kt", "NE", "SE", "SW", "NW", "Name / more"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    QFont mono{"monospace"};
    mono.setStyleHint(QFont::Monospace);
    table->setFont(mono);
    raw = new QPlainTextEdit{this};
    raw->setReadOnly(true);
    raw->setLineWrapMode(QPlainTextEdit::NoWrap);
    raw->setFont(mono);
    tabs->addTab(table, "Decoded");
    tabs->addTab(raw, "Raw lines");
    box.addWidgetReal(tabs, 1, Qt::Alignment{});
    auto * credit = new QLabel{nhcBasin(id) ? "NHC / NCEP ATCF directory (ftp.nhc.noaa.gov/atcf). The a-deck holds every guidance model's forecast of each cycle as it arrives." :
                                              "Best track from UCAR's research data repository (hurricanes.ral.ucar.edu): the JTWC analyses, every 6 hours. JTWC's own decks are not public.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 640);
    QObject::connect(deck, &QComboBox::currentIndexChanged, this, [this] (int) { seen.clear(); firstLook = true; load(); });
    QObject::connect(view, &QComboBox::currentIndexChanged, this, [this] (int) { if (!lastText.empty()) showLines(lastText); });
    QObject::connect(count, &QComboBox::currentIndexChanged, this, [this] (int) { if (!lastText.empty()) showLines(lastText); });
    QObject::connect(filter, &QLineEdit::textChanged, this, [this] (const QString&) { if (!lastText.empty()) showLines(lastText); });
    QObject::connect(reload, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(raw->toPlainText()); });
    view->setCurrentIndex(std::clamp(qEnvironmentVariableIntValue("WXQT_ATCF_VIEW"), 0, 2));   // (a dev hook: the filter to start with)
    timer.setInterval(5 * 60 * 1000);
    QObject::connect(&timer, &QTimer::timeout, this, [this] { if (refresh->isChecked()) load(); });
    timer.start();
    load();
}

void AtcfFeedViewer::load() {
    const int mine = ++generation;
    auto text = std::make_shared<string>();
    auto why = std::make_shared<string>();
    const auto id = stormId;
    const int which = deck->currentIndex();
    new FutureVoid{this,
        [text, why, id, which] { *text = fetchDeck(id, which, *why); },
        [this, mine, text, why] {
            if (closed || mine != generation) {
                return;
            }
            if (text->empty()) {
                status->setText(QString::fromStdString(*why));
                table->setRowCount(0);
                raw->clear();
                lastText.clear();
                return;
            }
            lastText = *text;
            fresh.clear();   // the lines that were not in the file at the last reading (the first reading marks none)
            for (const auto& line : QString::fromStdString(*text).split('\n', Qt::SkipEmptyParts)) {
                const string trimmed = line.trimmed().toStdString();
                if (!firstLook && seen.count(trimmed) == 0) {
                    fresh.insert(trimmed);
                }
            }
            for (const auto& line : QString::fromStdString(*text).split('\n', Qt::SkipEmptyParts)) {
                seen.insert(line.trimmed().toStdString());
            }
            firstLook = false;
            showLines(*text);
            makeTimeline(*text);
            findInvest(*text);
        }};
}

void AtcfFeedViewer::showLines(const string& text) {
    // the lines (those the filter lets through), the last N of them
    QStringList lines;
    const auto filterText = filter->text().trimmed();
    for (const auto& line : QString::fromStdString(text).split('\n', Qt::SkipEmptyParts)) {
        if (line.trimmed().isEmpty() || (!filterText.isEmpty() && !line.contains(filterText, Qt::CaseInsensitive))) {
            continue;
        }
        lines << line.trimmed();
    }
    // the best track only: just the lines where the status (or the category) changes, each with what changed
    QStringList changes(lines.size());
    if (deck->currentIndex() == 0 && view->currentIndex() > 0) {
        QStringList kept;
        QStringList words;
        Row previous;
        for (int i = 0; i < lines.size(); i++) {
            const Row now = parseRow(lines[i]);
            if (!now.ok) {
                continue;
            }
            QString change;
            if (!previous.ok) {
                change = "start: " + now.type + (now.name.isEmpty() ? QString{} : " " + now.name) + ", " + category(now.wind);
            } else if (view->currentIndex() == 1) {
                change = statusChange(previous, now);
            } else if (category(previous.wind) != category(now.wind)) {
                change = category(previous.wind) + " -> " + category(now.wind);
            }
            if (!change.isEmpty() || i == lines.size() - 1) {
                kept << lines[i];
                words << (change.isEmpty() ? QString{"latest"} : change);
            }
            previous = now;
        }
        lines = kept;
        changes = words;
    }
    const int wanted = count->currentData().toInt();
    const int total = static_cast<int>(lines.size());
    if (total > wanted) {
        changes = changes.mid(total - wanted);
        lines = lines.mid(total - wanted);
    }
    std::vector<bool> freshRow;
    int newCount = 0;
    for (const auto& line : lines) {
        const bool isNew = fresh.count(line.toStdString()) > 0;
        freshRow.push_back(isNew);
        newCount += isNew ? 1 : 0;
    }
    table->setRowCount(static_cast<int>(lines.size()));
    for (int row = 0; row < lines.size(); row++) {
        const auto f = lines[row].split(',');
        const auto field = [&f] (int i) { return i < f.size() ? nice(f[i]) : QString{}; };
        const auto when = QDateTime::fromString(field(2), "yyyyMMddHH");
        const QString radii = field(11);
        QStringList more;
        const QString name = field(27);
        if (!name.isEmpty()) {
            more << name;
        }
        for (int i = 28; i < f.size() && more.size() < 6; i++) {
            if (!nice(f[i]).isEmpty()) {
                more << nice(f[i]);
            }
        }
        const QStringList cells{row < changes.size() ? changes[row] : QString{}, when.isValid() ? when.toString("yyyy-MM-dd HH") + "Z" : field(2), field(4), field(5), degrees(field(6)), degrees(field(7)), field(8), field(9), field(10), radii, field(13), field(14), field(15), field(16), more.join("  ")};
        for (int c = 0; c < cells.size(); c++) {
            auto * item = new QTableWidgetItem{cells[c]};
            if (freshRow[static_cast<size_t>(row)]) {
                item->setBackground(QColor{255, 244, 190});
                QFont bold = item->font();
                bold.setBold(true);
                item->setFont(bold);
            }
            table->setItem(row, c, item);
        }
    }
    table->resizeColumnsToContents();
    table->scrollToBottom();
    raw->setPlainText(lines.join('\n'));
    raw->moveCursor(QTextCursor::End);
    status->setText(QString{"%1 of %2 lines, the newest at the bottom%3.  Read %4 UTC."}.arg(lines.size()).arg(total).arg(newCount > 0 ? QString{"  -  %1 new since the last look (marked)"}.arg(newCount) : QString{}).arg(QDateTime::currentDateTimeUtc().toString("HH:mm")));
}

// "AL93 -> AL09: GENESIS022 (DB, Oct 4 18Z) -> INVEST (Oct 5 12Z) -> NINE TD (Oct 6 18Z) -> ISAIAS TS (Oct 7 06Z) -> HU (Oct 8 06Z) ...": the storm's life as its best track tells it
void AtcfFeedViewer::makeTimeline(const string& text) {
    timelineBase.clear();
    if (deck->currentIndex() != 0) {
        timeline->clear();
        return;
    }
    Row previous;
    QStringList events;
    for (const auto& line : QString::fromStdString(text).split('\n', Qt::SkipEmptyParts)) {
        const Row now = parseRow(line);
        if (!now.ok) {
            continue;
        }
        if (!previous.ok || previous.type != now.type || previous.name != now.name) {
            QString label;
            if (!previous.ok || previous.name != now.name) {
                label = now.name + (now.name.isEmpty() ? QString{} : " ");
            }
            label += now.type;
            events << label + " (" + shortTime(now.dtg) + ")";
        }
        previous = now;
    }
    string label = stormId;
    std::transform(label.begin(), label.end(), label.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    label = label.substr(0, 4);   // AL09
    timelineBase = label.size() >= 4 ? label : stormId;
    timeline->setText((investNote.empty() ? QString{} : QString::fromStdString(investNote) + "  \u2192  ") + QString::fromStdString(timelineBase) + ":  " + events.join("  \u2192  "));
}

// the invest the storm came from: the invests (numbers 90 to 99) of the basin and year whose best track passed through the same places at the same times as the storm's first days
void AtcfFeedViewer::findInvest(const string& text) {
    investNote.clear();
    if (deck->currentIndex() != 0 || stormId.size() < 8 || stormId.substr(2, 2) >= "90") {
        return;
    }
    std::vector<Row> early;
    for (const auto& line : QString::fromStdString(text).split('\n', Qt::SkipEmptyParts)) {
        const Row r = parseRow(line);
        if (r.ok) {
            early.push_back(r);
        }
        if (early.size() >= 12) {
            break;
        }
    }
    if (early.empty()) {
        return;
    }
    const int mine = generation;
    auto found = std::make_shared<string>();
    const auto id = stormId;
    new FutureVoid{this,
        [found, id, early] {
            const string basin = id.substr(0, 2), year = id.substr(id.size() - 4);
            const bool nhc = nhcBasin(id);
            const string folder = nhc ? "https://ftp.nhc.noaa.gov/atcf/btk/" : "https://hurricanes.ral.ucar.edu/repository/data/bdecks_open/" + year + "/";
            const auto listing = QString::fromStdString(URL::getText(folder));
            const QRegularExpression pattern{QString{"href=\"b%1(9\\d)%2\\.dat\""}.arg(QString::fromStdString(basin), QString::fromStdString(year))};
            QStringList matches;
            for (auto m = pattern.globalMatch(listing); m.hasNext();) {
                const auto number = m.next().captured(1);
                const auto bytes = URL::getBytes(folder + "b" + basin + number.toStdString() + year + ".dat");
                bool same = false;
                for (const auto& line : QString::fromUtf8(bytes).split('\n', Qt::SkipEmptyParts)) {
                    const Row r = parseRow(line);
                    for (const auto& e : early) {
                        if (r.ok && r.dtg == e.dtg && std::abs(r.lat - e.lat) < 1.0 && std::abs(r.lon - e.lon) < 1.0) {
                            same = true;
                        }
                    }
                }
                if (same) {
                    matches << QString::fromStdString(basin).toUpper() + number;
                }
            }
            matches.removeDuplicates();
            *found = matches.join(", ").toStdString();
        },
        [this, mine, found] {
            if (closed || mine != generation || found->empty()) {
                return;
            }
            investNote = *found;
            timeline->setText(QString::fromStdString(investNote) + "  \u2192  " + timeline->text());
        }};
}
