// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "rivers/RiverForecastList.h"
#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <QDateTime>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QTextStream>
#include <QTimeZone>
#include "objects/FutureVoid.h"
#include "util/Utility.h"
#include "rivers/RiverGaugeViewer.h"
#include "ui/TableExportDialog.h"
#include "ui/UiStandards.h"

namespace {
    int rank(const string& status) {
        return status == "action" ? 1 : status == "minor" ? 2 : status == "moderate" ? 3 : status == "major" ? 4 : 0;
    }

    QString nameOf(const string& status) {
        static const std::map<string, QString> names{{"action", "Action"}, {"minor", "Minor flooding"}, {"moderate", "Moderate flooding"}, {"major", "Major flooding"}, {"no_flooding", "No flooding"},
            {"low_threshold", "Low water"}, {"obs_not_current", "Not current"}, {"out_of_service", "Out of service"}, {"not_defined", "No flood stages"}};
        const auto found = names.find(status);
        return found == names.end() ? QString::fromStdString(status) : found->second;
    }

    QColor colorOf(const string& status) {
        return status == "major" ? QColor{190, 90, 250} : status == "moderate" ? QColor{245, 90, 90} : status == "minor" ? QColor{255, 170, 60} : status == "action" ? QColor{245, 225, 80} : QColor{};
    }

    // sorts by a number kept with the cell, not by its text
    class Cell : public QTableWidgetItem {
    public:
        Cell(const QString& text, double key) : QTableWidgetItem{text} { setData(Qt::UserRole, key); }
        bool operator<(const QTableWidgetItem& other) const override {
            return data(Qt::UserRole).toDouble() < other.data(Qt::UserRole).toDouble();
        }
    };

    QString stage(double value, const string& units) {
        return UtilityRivers::has(value) ? QString::number(value, 'f', 2) + " " + QString::fromStdString(units) : QString{};
    }
}

RiverForecastList::RiverForecastList(Window * parent)
    : Window{parent}
    , rows{std::make_shared<vector<Row>>()}
    , lows{std::make_shared<vector<Row>>()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("River gauges - forecast to reach action stage");
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    minimum = UiStandards::combo(top);
    minimum->addItem("Action stage or higher", 1);
    minimum->addItem("Minor flooding or higher", 2);
    minimum->addItem("Moderate flooding or higher", 3);
    minimum->addItem("Major flooding", 4);
    minimum->addItem("Low water (forecast below the low-water level)", 0);
    onlyRising = new QCheckBox{"Only those not there now", top};
    onlyRising->setToolTip("The gauges now below the action stage that the forecast takes up to it (or above)");
    pin = new QCheckBox{"Pin the crest to now", top};
    pin->setToolTip("A gauge that is higher now than its forecast ever is (a river falling) is shown at its level now, in its category now, and the gauges at action stage or higher now are all listed; off: the forecast as the service gives it");
    pin->setChecked(Utility::readPref("RIVERFCST_PIN", "true") == "true");
    auto * refresh = UiStandards::button(top, "Refresh", "Read the river forecasts again");
    auto * exportButton = UiStandards::button(top, "Export...", "Choose the gauges and columns, and save the table as CSV or a picture, or copy it for a spreadsheet");
    status = new QLabel{"Reading the river forecasts...", top};
    UiStandards::muted(status);
    topLayout->addWidget(minimum);
    topLayout->addWidget(onlyRising);
    topLayout->addWidget(pin);
    topLayout->addWidget(refresh);
    topLayout->addWidget(exportButton);
    topLayout->addWidget(status, 1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    table = new QTableWidget{0, 0, this};
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    table->setSortingEnabled(true);
    box.addWidgetReal(table, 1, Qt::Alignment{});
    auto * credit = new QLabel{"NOAA / NWS river forecasts (the National Weather Service's river gauge map service, the highest category of each gauge's whole forecast period, 5 to 14 days by gauge); "
                               "tide gauges are in feet above MLLW. Double-click a row for the gauge's hydrograph. A forecast can change with the next rain.", this};
    credit->setWordWrap(true);
    UiStandards::muted(credit);
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1250, 640);
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { load(); });
    QObject::connect(exportButton, &QPushButton::clicked, this, [this] { exportTable(); });
    QObject::connect(minimum, &QComboBox::currentIndexChanged, this, [this] (int) { fill(); });
    QObject::connect(onlyRising, &QCheckBox::toggled, this, [this] (bool) { fill(); });
    QObject::connect(pin, &QCheckBox::toggled, this, [this] (bool on) { Utility::writePref("RIVERFCST_PIN", on ? "true" : "false"); fill(); });
    QObject::connect(table, &QTableWidget::cellDoubleClicked, this, [this] (int row, int) {
        const auto * item = table->item(row, 0);
        if (item != nullptr) {
            new RiverGaugeViewer{this, item->text().toStdString()};
        }
    });
    load();
}

void RiverForecastList::load() {
    const int mine = ++generation;
    status->setText("Reading the river forecasts...");
    auto fresh = std::make_shared<vector<Row>>();
    auto freshLows = std::make_shared<vector<Row>>();
    auto problem = std::make_shared<string>();
    new FutureVoid{this, [fresh, freshLows, problem] {
        vector<UtilityRivers::Peak> peaks, lowPeaks;
        vector<UtilityRivers::Gauge> gauges;
        std::map<string, UtilityRivers::FirstReach> reach;
        if (!UtilityRivers::loadPeaks(peaks, *problem)) {
            return;
        }
        string ignored;
        UtilityRivers::loadLows(lowPeaks, ignored);
        UtilityRivers::loadGauges(gauges, ignored);        // (the state now: the list is useful without it)
        UtilityRivers::loadFirstReach(reach, ignored);     // (and when each is first forecast at a category)
        std::map<string, const UtilityRivers::Gauge *> byId;
        for (const auto& g : gauges) {
            byId[g.lid] = &g;
        }
        const auto clock = [] (const string& time, int hours) {
            auto t = QDateTime::fromString(QString::fromStdString(time), "yyyy-MM-dd HH:mm:ss");
            t.setTimeZone(QTimeZone::utc());
            return t.isValid() ? (t.toString("ddd d MMM HH:mm") + "Z (+" + QString::number(hours) + " h)").toStdString() : string{};
        };
        const auto fillRow = [&] (Row& row) {
            const auto found = byId.find(row.peak.lid);
            if (found != byId.end()) {
                row.nowStatus = found->second->status;
                row.observed = found->second->observed;
                row.obsTime = found->second->obsTime;
            }
            const auto first = reach.find(row.peak.lid);
            if (first != reach.end()) {
                row.firstAction = clock(first->second.time[1], first->second.hours[1]);
                row.firstMinor = clock(first->second.time[2], first->second.hours[2]);
            }
        };
        std::set<string> listed;
        for (const auto& k : peaks) {
            Row row;
            row.peak = k;
            listed.insert(k.lid);
            fillRow(row);
            fresh->push_back(std::move(row));
        }
        for (const auto& g : gauges) {   // at action stage or higher now, and not in the forecast layer: its forecast falls below the stage (or it has none)
            if (rank(g.status) >= 1 && !listed.count(g.lid)) {
                Row row;
                row.onlyNow = true;
                row.nowStatus = g.status;
                row.observed = g.observed;
                row.obsTime = g.obsTime;
                row.peak.lid = g.lid;
                row.peak.name = g.name;
                row.peak.waterbody = g.waterbody;
                row.peak.state = g.state;
                row.peak.wfo = g.wfo;
                row.peak.units = g.units;
                row.peak.status = g.status;
                row.peak.peak = g.observed;
                row.peak.peakTime = g.obsTime;
                row.peak.action = g.action;
                row.peak.minor = g.minor;
                row.peak.moderate = g.moderate;
                row.peak.major = g.major;
                row.peak.lat = g.lat;
                row.peak.lon = g.lon;
                fresh->push_back(std::move(row));
            }
        }
        for (const auto& k : lowPeaks) {
            Row row;
            row.peak = k;
            fillRow(row);
            freshLows->push_back(std::move(row));
        }
    }, [this, mine, fresh, freshLows, problem] {
        if (closed || mine != generation) {
            return;
        }
        if (!problem->empty()) {
            status->setText(QString::fromStdString(*problem));
            return;
        }
        rows = fresh;
        lows = freshLows;
        fill();
    }};
}

void RiverForecastList::fill() {
    const int least = minimum->currentData().toInt();
    const bool lowWater = least == 0;
    onlyRising->setEnabled(!lowWater);
    pin->setEnabled(!lowWater);
    shown.clear();
    for (auto row : lowWater ? *lows : *rows) {
        if (lowWater) {
            row.trend = "forecast below the low-water level";
            shown.push_back(row);
            continue;
        }
        row.trend = row.onlyNow ? "at its level now (forecast stays below)" : "rising to the crest";
        if (pin->isChecked()) {
            // the crest is never below the level now: a river that is falling is at its level now, in its category now
            if (UtilityRivers::has(row.observed) && (!UtilityRivers::has(row.peak.peak) || row.observed > row.peak.peak)) {
                if (!row.onlyNow) {
                    row.trend = "falling (pinned to now)";
                }
                row.peak.peak = row.observed;
                row.peak.peakTime = row.obsTime;
                if (rank(row.nowStatus) > rank(row.peak.status)) {
                    row.peak.status = row.nowStatus;
                }
            }
        } else if (row.onlyNow) {
            continue;
        } else if (UtilityRivers::has(row.observed) && UtilityRivers::has(row.peak.peak) && row.observed > row.peak.peak) {
            row.trend = "falling";
        }
        if (rank(row.peak.status) < least) {
            continue;
        }
        if (onlyRising->isChecked() && rank(row.nowStatus) >= 1) {
            continue;
        }
        shown.push_back(row);
    }
    table->setSortingEnabled(false);
    table->clear();
    const QStringList headers{"Gauge", "Location", "River", "State", "Now", "Stage now", lowWater ? "Forecast low" : "Forecast crest", lowWater ? "Category" : "Crest category", lowWater ? "Low time (UTC)" : "Crest time (UTC)",
                              "Hours ahead", "First at action+", "First at minor+", "Crest vs record", lowWater ? "Low-water level" : "Action", "Minor", "Moderate", "Major", "Note"};
    table->setColumnCount(headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->setRowCount(static_cast<int>(shown.size()));
    const auto now = QDateTime::currentDateTimeUtc();
    int row = 0, rising = 0;
    for (const auto& r : shown) {
        const auto when = QDateTime::fromString(QString::fromStdString(r.peak.peakTime), "yyyy-MM-dd HH:mm:ss");
        auto utc = when;
        utc.setTimeZone(QTimeZone::utc());
        const double hours = utc.isValid() ? std::max(0.0, now.secsTo(utc) / 3600.0) : 0.0;   // (a crest pinned to now is 0 hours ahead, not minus a few)
        rising += rank(r.nowStatus) < 1 ? 1 : 0;
        const auto add = [&] (int column, QTableWidgetItem * item) { table->setItem(row, column, item); };
        const auto number = [] (double v) { return UtilityRivers::has(v) ? v : -999.0; };
        add(0, new Cell{QString::fromStdString(r.peak.lid), 0.0});
        add(1, new Cell{QString::fromStdString(r.peak.name), 0.0});
        add(2, new Cell{QString::fromStdString(r.peak.waterbody), 0.0});
        add(3, new Cell{QString::fromStdString(r.peak.state), 0.0});
        auto * nowItem = new Cell{nameOf(r.nowStatus), double(rank(r.nowStatus))};
        if (colorOf(r.nowStatus).isValid()) {
            nowItem->setBackground(colorOf(r.nowStatus));
            nowItem->setForeground(QColor{20, 20, 20});
        }
        add(4, nowItem);
        add(5, new Cell{stage(r.observed, r.peak.units), number(r.observed)});
        add(6, new Cell{stage(r.peak.peak, r.peak.units), number(r.peak.peak)});
        auto * category = new Cell{nameOf(r.peak.status), lowWater ? 0.0 : double(rank(r.peak.status))};
        category->setBackground(lowWater ? QColor{200, 170, 130} : colorOf(r.peak.status));
        category->setForeground(QColor{20, 20, 20});
        add(7, category);
        add(8, new Cell{utc.isValid() ? utc.toString("ddd d MMM HH:mm") + "Z" : QString{}, utc.isValid() ? static_cast<double>(utc.toSecsSinceEpoch()) : 0.0});
        add(9, new Cell{utc.isValid() ? QString::number(hours, 'f', 0) : QString{}, hours});
        const auto hoursOf = [] (const string& text) {   // "... (+48 h)" for the sort
            const auto at = text.find("(+");
            return at == string::npos ? 9999.0 : std::atof(text.c_str() + at + 2);
        };
        add(10, new Cell{QString::fromStdString(r.firstAction), hoursOf(r.firstAction)});
        add(11, new Cell{QString::fromStdString(r.firstMinor), hoursOf(r.firstMinor)});
        add(12, new Cell{QString{}, 0.0});   // the record: filled after the table is up
        add(13, new Cell{stage(lowWater ? r.peak.action : r.peak.action, r.peak.units), number(r.peak.action)});
        add(14, new Cell{stage(r.peak.minor, r.peak.units), number(r.peak.minor)});
        add(15, new Cell{stage(r.peak.moderate, r.peak.units), number(r.peak.moderate)});
        add(16, new Cell{stage(r.peak.major, r.peak.units), number(r.peak.major)});
        add(17, new Cell{QString::fromStdString(r.trend), 0.0});
        row++;
    }
    table->setSortingEnabled(true);
    table->sortByColumn(9, Qt::AscendingOrder);   // the soonest crest first
    table->resizeColumnsToContents();
    status->setText(lowWater ? QString::number(shown.size()) + " gauges forecast to fall below their low-water level."
                             : QString::number(shown.size()) + " gauges forecast to reach " + minimum->currentText().toLower() + "; " + QString::number(rising) + " of them are below the action stage now.");
    if (!lowWater) {
        showRecords();
        startRecords();
    }
}

// how the crest ranks among the gauge's record crests: "3rd highest of 10 on record (record 12.4 ft, 1949)"
QString RiverForecastList::recordText(const Row& r) const {
    const auto found = records.find(r.peak.lid);
    if (found == records.end() || !UtilityRivers::has(r.peak.peak)) {
        return {};
    }
    const auto& crests = found->second;
    if (crests.empty()) {
        return "no record crests";
    }
    int above = 0;
    for (const auto& c : crests) {
        above += c.first > r.peak.peak + 1e-9 ? 1 : 0;
    }
    const auto year = [] (const string& time) { return QString::fromStdString(time.substr(0, 4)); };
    const QString top = QString{"record %1 ft, %2"}.arg(crests.front().first, 0, 'f', 2).arg(year(crests.front().second));
    if (above == 0) {
        return r.peak.peak > crests.front().first + 1e-9 ? "a new record (" + top + ")" : "ties the record (" + top + ")";
    }
    if (above >= static_cast<int>(crests.size())) {
        return "below the " + QString::number(crests.size()) + " record crests listed (" + top + ")";
    }
    const int place = above + 1;
    const QString suffix = place % 100 >= 11 && place % 100 <= 13 ? "th" : place % 10 == 1 ? "st" : place % 10 == 2 ? "nd" : place % 10 == 3 ? "rd" : "th";
    return QString::number(place) + suffix + " highest of " + QString::number(crests.size()) + " on record (" + top + ")";
}

// the cells of the column from the record crests read so far
void RiverForecastList::showRecords() {
    std::map<string, const Row *> byId;
    for (const auto& r : shown) {
        byId[r.peak.lid] = &r;
    }
    const bool sorting = table->isSortingEnabled();
    table->setSortingEnabled(false);
    for (int i = 0; i < table->rowCount(); i++) {
        const auto * id = table->item(i, 0);
        if (id == nullptr) {
            continue;
        }
        const auto found = byId.find(id->text().toStdString());
        if (found == byId.end()) {
            continue;
        }
        const auto text = recordText(*found->second);
        auto * cell = table->item(i, 12);
        if (cell != nullptr && !text.isEmpty()) {
            cell->setText(text);
            const auto crests = records.find(found->first);
            double key = 0.0;   // the sort: the nearer the record the higher
            if (crests != records.end() && !crests->second.empty() && crests->second.front().first > 0.0 && UtilityRivers::has(found->second->peak.peak)) {
                key = found->second->peak.peak / crests->second.front().first;
            }
            cell->setData(Qt::UserRole, key);
        }
    }
    table->setSortingEnabled(sorting);
}

// after the table is up: each gauge at action stage or higher (all those listed, but not the low-water ones), six at a time
void RiverForecastList::startRecords() {
    std::vector<string> wanted;
    for (const auto& r : shown) {
        if (rank(r.peak.status) >= 1 && !records.count(r.peak.lid) && !asked.count(r.peak.lid)) {
            wanted.push_back(r.peak.lid);
            asked.insert(r.peak.lid);
        }
    }
    if (wanted.empty()) {
        return;
    }
    recordsWanted += wanted.size();
    const int mine = generation;
    auto queue = std::make_shared<std::vector<string>>(wanted);
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, mine, queue, step] {
        if (closed || mine != generation || queue->empty()) {
            if (!closed && mine == generation) {
                status->setText(status->text().section("  Comparing", 0, 0));
            }
            return;
        }
        const size_t take = std::min<size_t>(6, queue->size());
        auto batch = std::make_shared<std::vector<string>>(queue->end() - static_cast<long>(take), queue->end());
        queue->resize(queue->size() - take);
        auto got = std::make_shared<std::map<string, vector<std::pair<double, string>>>>();
        new FutureVoid{this, [batch, got] {
            std::vector<std::thread> threads;
            std::mutex lock;
            for (const auto& lid : *batch) {
                threads.emplace_back([&lock, &got, lid] {
                    vector<std::pair<double, string>> crests;
                    string error;
                    if (UtilityRivers::loadRecordCrests(lid, crests, error)) {
                        std::lock_guard<std::mutex> guard{lock};
                        (*got)[lid] = std::move(crests);
                    }
                });
            }
            for (auto& t : threads) {
                t.join();
            }
        }, [this, mine, got, step, queue] {
            if (closed || mine != generation) {
                return;
            }
            for (auto& [lid, crests] : *got) {
                records[lid] = std::move(crests);
            }
            recordsDone += got->size();
            showRecords();
            status->setText(status->text().section("  Comparing", 0, 0) + (queue->empty() ? QString{} : "  Comparing the crests with the records: " + QString::number(recordsDone) + " of " + QString::number(recordsWanted) + "..."));
            (*step)();
        }};
    };
    (*step)();
}

void RiverForecastList::exportTable() {
    if (shown.empty()) {
        return;
    }
    TableExportDialog::Dataset data;
    data.prefix = "RIVERFCST";
    data.windowTitle = "Export river gauges forecast to reach action stage";
    data.title = "River gauges forecast to reach " + minimum->currentText().toLower() + ", " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + "Z";
    data.credit = "NOAA / NWS river forecasts";
    data.itemHeader = "Gauge";
    data.detailNames = {"Location", "River", "State"};
    const auto copy = std::make_shared<vector<Row>>(shown);
    const auto recordsCopy = std::make_shared<std::map<string, string>>();   // the record text of each gauge read so far
    for (const auto& r : shown) {
        const auto text = recordText(r);
        if (!text.isEmpty()) {
            (*recordsCopy)[r.peak.lid] = text.toStdString();
        }
    }
    for (const auto& r : *copy) {
        data.items.push_back({QString::fromStdString(r.peak.lid), QString::fromStdString(r.peak.lid), {QString::fromStdString(r.peak.name), QString::fromStdString(r.peak.waterbody), QString::fromStdString(r.peak.state)}});
    }
    const QStringList names{"Now", "Stage now", "Forecast crest", "Crest category", "Crest time (UTC)", "First at action+", "First at minor+", "Crest vs record", "Action stage", "Minor flood stage", "Moderate flood stage", "Major flood stage", "Units", "Note"};
    for (const auto& name : names) {
        data.fields.push_back({name, name});
    }
    data.parts = {{"value", "Value"}};
    data.cell = [copy, recordsCopy] (size_t item, size_t field, size_t) {
        const auto& r = (*copy)[item];
        const auto number = [] (double v) { return UtilityRivers::has(v) ? QString::number(v, 'f', 2) : QString{}; };
        switch (field) {
            case 0: return nameOf(r.nowStatus);
            case 1: return number(r.observed);
            case 2: return number(r.peak.peak);
            case 3: return nameOf(r.peak.status);
            case 4: return QString::fromStdString(r.peak.peakTime);
            case 5: return QString::fromStdString(r.firstAction);
            case 6: return QString::fromStdString(r.firstMinor);
            case 7: return recordsCopy->count(r.peak.lid) ? QString::fromStdString((*recordsCopy)[r.peak.lid]) : QString{};
            case 8: return number(r.peak.action);
            case 9: return number(r.peak.minor);
            case 10: return number(r.peak.moderate);
            case 11: return number(r.peak.major);
            case 12: return QString::fromStdString(r.peak.units);
            default: return QString::fromStdString(r.trend);
        }
    };
    data.tint = [copy] (size_t item, size_t field) {
        const auto& r = (*copy)[item];
        return field == 3 ? colorOf(r.peak.status) : field == 0 ? colorOf(r.nowStatus) : QColor{};
    };
    TableExportDialog dialog{this, std::move(data)};
    if (const auto out = qEnvironmentVariable("WXQT_TABLE_OUT"); !out.isEmpty()) {   // development: the picture and the CSV, without asking
        dialog.picture().save(out + ".png");
        QFile file{out + ".csv"};
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream{&file} << dialog.text(true);
        }
        return;
    }
    dialog.exec();
}
