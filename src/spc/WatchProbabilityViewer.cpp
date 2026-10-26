// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spc/WatchProbabilityViewer.h"
#include <algorithm>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QPushButton>
#include <QTextStream>
#include <QTimeZone>
#include "objects/FutureVoid.h"
#include "ui/ChartExport.h"
#include "ui/UiStandards.h"

namespace {
    QString zulu(long seconds) {
        return seconds == 0 ? QString{} : QDateTime::fromSecsSinceEpoch(seconds, QTimeZone::utc()).toString("ddd d HH:mm") + "Z";
    }
}

WatchProbabilityViewer::WatchProbabilityViewer(Window * parent, const std::vector<Watch>& watches)
    : Window{parent}
    , watches{watches}
    , rows{std::make_shared<std::vector<Row>>()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Watch probabilities - SPC");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    auto * refresh = UiStandards::button(top, "Refresh", "Read the SPC's pages of the watches again");
    auto * save = UiStandards::button(top, "Save CSV...", "Save the table as a CSV file for a spreadsheet");
    auto * copy = UiStandards::button(top, "Copy", "Copy the table for a spreadsheet");
    status = new QLabel{"Reading the watches...", top};
    UiStandards::muted(status);
    topLayout->addWidget(refresh);
    topLayout->addWidget(save);
    topLayout->addWidget(copy);
    topLayout->addWidget(status, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    table = new QTableWidget{0, 0, this};
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    box.addWidgetReal(table, 1, Qt::Alignment{});
    auto * credit = new QLabel{"NOAA / NWS Storm Prediction Center, the Watch Hazard Probabilities of each watch: the forecaster's chance of the events inside the watch. Low covers under 2 % up to 20 % for "
                               "strong (EF2-EF5) tornadoes and under 5 % up to 20 % for the others, Mod 30 to 60 %, High 70 % to over 95 %. A severe thunderstorm watch has no tornado rows of the strong kind.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1150, 420);
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(save, &QPushButton::clicked, this, [this] { saveCsv(); });
    QObject::connect(copy, &QPushButton::clicked, this, [this] { copyText(); });
    load();
}

void WatchProbabilityViewer::load() {
    const int mine = ++generation;
    status->setText("Reading the watches...");
    auto fresh = std::make_shared<std::vector<Row>>();
    for (const auto& watch : watches) {
        fresh->push_back({watch, {}, false});
    }
    new FutureVoid{this, [fresh] {
        for (auto& row : *fresh) {
            row.hazards = UtilityWatchProbabilities::fetch(row.watch.number);
            row.read = true;
        }
    }, [this, mine, fresh] {
        if (closed || mine != generation) {
            return;
        }
        rows = fresh;
        fill();
    }};
}

// the hazards' names in the order the first watch that has them lists them
void WatchProbabilityViewer::fill() {
    QStringList names;
    for (const auto& row : *rows) {
        for (const auto& h : row.hazards) {
            if (!names.contains(h.event)) {
                names << h.event;
            }
        }
    }
    QStringList headers{"Watch", "Until"};
    headers << names;
    table->clear();
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(static_cast<int>(rows->size()));
    int row = 0, none = 0;
    for (const auto& r : *rows) {
        table->setItem(row, 0, new QTableWidgetItem{r.watch.title});
        table->setItem(row, 1, new QTableWidgetItem{zulu(r.watch.end)});
        if (r.hazards.empty()) {
            none++;
        }
        for (const auto& h : r.hazards) {
            auto * item = new QTableWidgetItem{h.percent + " %"};
            item->setToolTip(h.event + ": " + h.level);
            item->setTextAlignment(Qt::AlignCenter);
            if (h.level == "High") {
                item->setBackground(QColor{255, 120, 100});
                item->setForeground(QColor{20, 20, 20});
            } else if (h.level == "Mod") {
                item->setBackground(QColor{255, 215, 110});
                item->setForeground(QColor{20, 20, 20});
            }
            table->setItem(row, headers.indexOf(h.event), item);
        }
        row++;
    }
    table->resizeColumnsToContents();
    status->setText(rows->empty() ? QString{"No watch in force."} : QString::number(rows->size()) + (rows->size() == 1 ? " watch" : " watches") + (none > 0 ? ", " + QString::number(none) + " without a table" : QString{}) + ".");
}

// the table as text: tab separated for a spreadsheet, or CSV with the quotes it needs
QString WatchProbabilityViewer::build(bool csv) const {
    QStringList names;
    for (const auto& row : *rows) {
        for (const auto& h : row.hazards) {
            if (!names.contains(h.event)) {
                names << h.event;
            }
        }
    }
    const auto cell = [csv] (const QString& text) { return csv ? "\"" + QString{text}.replace("\"", "\"\"") + "\"" : text; };
    const QString sep = csv ? "," : "\t";
    QStringList header{cell("Watch"), cell("Until (UTC)")};
    for (const auto& name : names) {
        header << cell("Probability of " + name + " (%)") << cell(name + " (level)");
    }
    QStringList lines{header.join(sep)};
    for (const auto& row : *rows) {
        QStringList fields{cell(row.watch.title), cell(row.watch.end == 0 ? QString{} : QDateTime::fromSecsSinceEpoch(row.watch.end, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm"))};
        for (const auto& name : names) {
            const auto found = std::find_if(row.hazards.begin(), row.hazards.end(), [&name] (const auto& h) { return h.event == name; });
            fields << (found == row.hazards.end() ? QString{} : cell(found->percent)) << (found == row.hazards.end() ? QString{} : cell(found->level));
        }
        lines << fields.join(sep);
    }
    return lines.join("\n") + "\n";
}

void WatchProbabilityViewer::saveCsv() {
    if (rows->empty()) {
        return;
    }
    const auto target = ChartExport::askSave(this, "Save the watch probabilities", "watch probabilities", "csv", "CSV files (*.csv)");
    if (target.isEmpty()) {
        return;
    }
    QFile file{target};
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Export", "The file could not be written.");
        return;
    }
    QTextStream out{&file};
    out << build(true);
    status->setText("Saved " + target);
}

void WatchProbabilityViewer::copyText() {
    if (!rows->empty()) {
        QApplication::clipboard()->setText(build(false));
        status->setText("Copied: paste it into a spreadsheet.");
    }
}
