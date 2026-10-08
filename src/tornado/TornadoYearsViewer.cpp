// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tornado/TornadoYearsViewer.h"
#include <algorithm>
#include <map>
#include <QHeaderView>
#include <QLocale>
#include <QTableWidget>
#include "tornado/UtilityTornado.h"
#include "ui/NumberItem.h"

TornadoYearsViewer::TornadoYearsViewer(Window * parent, const std::shared_ptr<const TornadoData::Database>& db, int rating, const std::string& state)
    : Window{parent}
    , textHint{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tornado years, ranked");
    struct Year {
        int tornadoes{0};
        int strong{0};          // EF3 or more
        int fatalities{0};
        int injuries{0};
        double longest{0.0};    // miles
        double total{0.0};
    };
    std::map<int, Year> years;
    for (const auto& t : db->tornadoes) {
        if (!t.counts() || !UtilityTornado::passesRating(t, rating) || (!state.empty() && t.state != state)) {
            continue;
        }
        auto& y = years[t.year];
        y.tornadoes++;
        y.strong += t.mag >= 3 ? 1 : 0;
        y.fatalities += t.fatalities;
        y.injuries += t.injuries;
        y.longest = std::max(y.longest, t.length);
        y.total += t.length;
    }
    textHint.setText("Click a heading to rank the years by it (click again to turn it round).   " + std::string{state.empty() ? "All states" : state} + (rating > 0 ? ", the rating filter of the charts applies" : "") +
                     "   -   the newest year may be incomplete" + (db->preliminaryCount > 0 ? "; * " + std::to_string(db->preliminaryFrom) + " on is SPC's preliminary point reports (no rating, fatalities or injuries, and a tornado may count twice)" : std::string{}));
    table = new QTableWidget{static_cast<int>(years.size()), 7, this};
    table->setHorizontalHeaderLabels({"Year", "Tornadoes", "EF3 or more", "Fatalities", "Injuries", "Longest track (mi)", "All tracks (mi)"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    const QLocale english{QLocale::English};
    int row = 0;
    for (const auto& [year, y] : years) {
        const auto number = [&] (double value, int digits = 0) { return new NumberItem{english.toString(value, 'f', digits), value}; };
        // SPC's preliminary point reports (the years after the last full file) carry no rating, deaths or injuries: the year is starred so its zeros are not read as fact
        const bool preliminary = db->preliminaryCount > 0 && year >= db->preliminaryFrom;
        table->setItem(row, 0, new NumberItem{QString::number(year) + (preliminary ? "*" : ""), static_cast<double>(year)});
        table->setItem(row, 1, number(y.tornadoes));
        table->setItem(row, 2, number(y.strong));
        table->setItem(row, 3, number(y.fatalities));
        table->setItem(row, 4, number(y.injuries));
        table->setItem(row, 5, number(y.longest, 1));
        table->setItem(row, 6, number(y.total));
        row++;
    }
    table->setSortingEnabled(true);
    table->sortByColumn(1, Qt::DescendingOrder);   // the busiest years first to begin with
    box.addWidget(textHint);
    box.addWidgetReal(table, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(860, 640);
}
