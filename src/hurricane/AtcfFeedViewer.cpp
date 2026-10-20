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
#include <QStringList>
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "util/UtilityGzip.h"

namespace {
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
    deck = new QComboBox{top};
    deck->addItem("Best track (b-deck)");
    deck->addItem("Guidance, every model (a-deck)");
    deck->addItem("Fixes, satellite and aircraft (f-deck)");
    count = new QComboBox{top};
    for (const int n : {30, 100, 300, 1000}) {
        count->addItem("last " + QString::number(n) + " lines", n);
    }
    filter = new QLineEdit{top};
    filter->setPlaceholderText("only lines with ...  (OFCL, GFS, HWRF ...)");
    filter->setClearButtonEnabled(true);
    refresh = new QCheckBox{"Look again every 5 minutes", top};
    refresh->setChecked(true);
    auto * reload = new QPushButton{"Refresh", top};
    auto * copy = new QPushButton{"Copy", top};
    topLayout->addWidget(deck);
    topLayout->addWidget(count);
    topLayout->addWidget(filter, 1);
    topLayout->addWidget(refresh);
    topLayout->addWidget(reload);
    topLayout->addWidget(copy);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    status = new QLabel{"Reading the deck...", this};
    status->setWordWrap(true);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    tabs = new QTabWidget{this};
    table = new QTableWidget{this};
    table->setColumnCount(14);
    table->setHorizontalHeaderLabels({"Time (UTC)", "Tech", "Tau", "Latitude", "Longitude", "Wind kt", "MSLP mb", "Type", "Rad kt", "NE", "SE", "SW", "NW", "Name / more"});
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
    credit->setStyleSheet("color: gray;");
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 640);
    QObject::connect(deck, &QComboBox::currentIndexChanged, this, [this] (int) { seen.clear(); firstLook = true; load(); });
    QObject::connect(count, &QComboBox::currentIndexChanged, this, [this] (int) { if (!lastText.empty()) showLines(lastText); });
    QObject::connect(filter, &QLineEdit::textChanged, this, [this] (const QString&) { if (!lastText.empty()) showLines(lastText); });
    QObject::connect(reload, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(raw->toPlainText()); });
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
    const int wanted = count->currentData().toInt();
    const int total = static_cast<int>(lines.size());
    if (total > wanted) {
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
        const QStringList cells{when.isValid() ? when.toString("yyyy-MM-dd HH") + "Z" : field(2), field(4), field(5), degrees(field(6)), degrees(field(7)), field(8), field(9), field(10), radii, field(13), field(14), field(15), field(16), more.join("  ")};
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
