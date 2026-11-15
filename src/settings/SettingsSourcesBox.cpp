// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsSourcesBox.h"
#include <future>
#include <thread>
#include <QBrush>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTimer>
#include "gfs/GfsClimate.h"
#include "gfs/GfsModels.h"
#include "gfs/Servers.h"
#include "util/Utility.h"
#include "models/UtilityGrib.h"
#include "ui/UiStandards.h"

namespace {
    // the fields the standard-deviation charts (anomalies in deviations, the chance of an anomaly of so many) use, and what for
    struct ClimateField {
        const char * name;
        GfsClimate::Field field;
        const char * usedBy;
        bool isStat{false};
        GfsClimate::Stat stat{};
    };
    std::vector<ClimateField> climateFields() {
        return {{"500 mb height", GfsClimate::height(500), "Height anomaly in std dev; chance of a 500 mb height anomaly"},
                {"700 mb height", GfsClimate::height(700), "Height anomaly in std dev; chance of a 700 mb height anomaly"},
                {"Sea level pressure", GfsClimate::seaLevelPressure(), "MSLP anomaly in std dev"},
                {"700 mb temperature", GfsClimate::temperature(700), "Chance of a 700 mb temperature anomaly"},
                {"850 mb temperature", GfsClimate::temperature(850), "Chance of an 850 mb temperature anomaly"},
                {"Precipitable water", GfsClimate::precipitableWater(), "Chance of a precipitable water anomaly"},
                {"Near-surface temperature", {}, "Chance of a 2 m temperature anomaly (daily means of the reanalysis)", true, GfsClimate::nearSurfaceTemperature()},
                {"Near-surface wind speed", {}, "Chance of a 10 m wind speed anomaly", true, GfsClimate::nearSurfaceWind()},
                {"Sea level pressure (daily)", {}, "Chance of a sea level pressure anomaly", true, GfsClimate::seaLevelPressureStat()}};
    }
    const GfsClimate& climate() {
        static const GfsClimate instance{UtilityGrib::gdalBinDir()};
        return instance;
    }
}

SettingsSourcesBox::SettingsSourcesBox(QWidget * parent)
    : QWidget{parent}
    , session{std::make_shared<GfsRender::Session>()}
    , generation{std::make_shared<std::atomic<int>>(0)}
{
    auto * layout = new QVBoxLayout{this};
    auto * about = new QLabel{"Where the Model Viewer's data comes from, and whether it is still there. Each model's server is asked for its newest run and whether that run lists the "
                              "fields of the model's first chart (nothing is downloaded). This looks when you open the tab, or press Check again.", this};
    about->setWordWrap(true);
    layout->addWidget(about);
    {   // the servers by name (section 20 of the plan): a model's url begins with {@name}; where each name points, from which layer, and the project's file that can move them without a release
        auto * heading = new QLabel{"<b>Servers</b>: the places the models read from, by name. The program has its own list; the project publishes a newer one that is read once a day "
                                    "(so a server that moves is fixed without a new version); a <code>servers.json</code> in your models folder, or a base typed here, goes over both.", this};
        heading->setWordWrap(true);
        layout->addWidget(heading);
        serversTable = new QTableWidget{0, 4, this};
        serversTable->setHorizontalHeaderLabels({"Server", "Places (the first is tried first)", "From", "What it is"});
        serversTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        serversTable->horizontalHeader()->setStretchLastSection(true);
        serversTable->verticalHeader()->setVisible(false);
        serversTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        serversTable->setToolTip("Double-click a Places cell to type a base of your own for that server (kept in the settings; empty puts the usual back).");
        layout->addWidget(serversTable);
        QObject::connect(serversTable, &QTableWidget::cellChanged, this, [this] (int r, int c) {
            if (c != 1 || serversTable->signalsBlocked()) {
                return;
            }
            const auto name = serversTable->item(r, 0)->text().toStdString();
            auto text = serversTable->item(r, 1)->text().trimmed();
            text = text.section('\n', 0, 0).section(" | ", 0, 0).trimmed();
            const auto now = Servers::table();
            const auto found = now.find(name);
            if (found != now.end() && !found->second.bases.empty() && text.toStdString() == found->second.bases.front()) {
                return;   // (unchanged)
            }
            Utility::writePref("SERVER_" + name, text.startsWith("https://") ? text.toStdString() : std::string{});
            Servers::reload();
            refreshServers();
        });
        auto * serversRow = new QHBoxLayout;
        auto * fetchBox = new QCheckBox{"Read server updates from the project (once a day)", this};
        fetchBox->setChecked(Utility::readPref("SERVERS_FETCH", "on") == "on");
        fetchBox->setToolTip("One request a day to the project's repository for a short list of server names and addresses. Nothing about you is sent. Off: only the program's list and yours.");
        QObject::connect(fetchBox, &QCheckBox::toggled, this, [] (bool on) { Utility::writePref("SERVERS_FETCH", on ? "on" : "off"); });
        auto * anyBox = new QCheckBox{"Allow other hosts in the project's file", this};
        anyBox->setChecked(Utility::readPref("SERVERS_ANY_HOST", "off") == "on");
        anyBox->setToolTip("Off: the project's file may only name hosts the program reads from today (NOAA, AWS, Google, NASA, DWD, ECCC, Open-Meteo ...). On: any https host.");
        QObject::connect(anyBox, &QCheckBox::toggled, this, [this] (bool on) { Utility::writePref("SERVERS_ANY_HOST", on ? "on" : "off"); Servers::reload(); refreshServers(); });
        auto * fetchNow = UiStandards::button(this, "Fetch now");
        QObject::connect(fetchNow, &QPushButton::clicked, this, [this, fetchNow] {
            fetchNow->setEnabled(false);
            serversStatus->setText("Fetching...");
            auto * guard = new QObject{this};
            std::thread{[this, fetchNow, guard] {
                Servers::refreshFromProject(nullptr);
                QMetaObject::invokeMethod(guard, [this, fetchNow, guard] { fetchNow->setEnabled(true); refreshServers(); guard->deleteLater(); }, Qt::QueuedConnection);
            }}.detach();
        });
        auto * openFolder = UiStandards::button(this, "Your file...");
        openFolder->setToolTip("Opens the folder of your own models: a servers.json there (the same format as the project's) goes over the project's.");
        QObject::connect(openFolder, &QPushButton::clicked, this, [] {
            const auto folder = QString::fromStdString(Servers::userFile()).section('/', 0, -2);
            QDir{}.mkpath(folder);
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        });
        serversStatus = new QLabel{this};
        serversRow->addWidget(fetchBox);
        serversRow->addWidget(anyBox);
        serversRow->addStretch(1);
        serversRow->addWidget(fetchNow);
        serversRow->addWidget(openFolder);
        layout->addLayout(serversRow);
        layout->addWidget(serversStatus);
        refreshServers();
    }
    table = new QTableWidget{0, 5, this};
    table->setHorizontalHeaderLabels({"Model", "Status", "Newest run (UTC)", "Seconds", "What was found"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table, 1);
    auto * row = new QHBoxLayout;
    summary = new QLabel{"Not looked at yet.", this};
    again = UiStandards::button(this, "Check again");
    QObject::connect(again, &QPushButton::clicked, this, [this] { start(); });
    row->addWidget(summary, 1);
    row->addWidget(again);
    layout->addLayout(row);
    {   // the 1991-2020 reanalysis the anomaly charts compare with: its daily means come a slice at a time, the standard deviations are worked out once from 30 years of daily values
        auto * heading = new QLabel{"<b>Climatology (NOAA PSL, NCEP/NCAR reanalysis 1991-2020)</b><br>The anomaly charts in standard deviations need each field's daily spread, worked out once "
                                    "from 30 years of the reanalysis (about a thousand small downloads a field, a few hundred MB in all). Nothing is downloaded until you press Download all; a chart that needs "
                                    "a field not stored yet says so. It is kept for good.", this};
        heading->setWordWrap(true);
        layout->addWidget(heading);
        climateTable = new QTableWidget{0, 3, this};
        climateTable->setHorizontalHeaderLabels({"Field", "Standard deviations", "Used by"});
        climateTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        climateTable->horizontalHeader()->setStretchLastSection(true);
        climateTable->verticalHeader()->setVisible(false);
        climateTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        climateTable->setSelectionMode(QAbstractItemView::NoSelection);
        int r = 0;
        for (const auto& f : climateFields()) {
            climateTable->insertRow(r);
            climateTable->setItem(r, 0, new QTableWidgetItem{f.name});
            climateTable->setItem(r, 1, new QTableWidgetItem{"-"});
            climateTable->setItem(r, 2, new QTableWidgetItem{f.usedBy});
            r++;
        }
        climateTable->setMaximumHeight(climateTable->horizontalHeader()->height() + r * 28 + 6);
        layout->addWidget(climateTable);
        auto * climateRow = new QHBoxLayout;
        climateSummary = new QLabel{this};
        climateButton = UiStandards::button(this, "Download all");
        climateButton->setToolTip("Works out the standard deviations of every field above that is not stored yet, a few at a time, in the background. Safe to leave; it carries on while the app is open.");
        QObject::connect(climateButton, &QPushButton::clicked, this, [this] {
            for (const auto& f : climateFields()) {
                if (f.isStat) {
                    climate().prepareStatistics(f.stat);
                } else {
                    climate().prepareDeviation(f.field);
                }
            }
            refreshClimate();
        });
        climateRow->addWidget(climateSummary, 1);
        climateRow->addWidget(climateButton);
        layout->addLayout(climateRow);
        climateTimer = new QTimer{this};
        climateTimer->setInterval(1500);
        QObject::connect(climateTimer, &QTimer::timeout, this, [this] { refreshClimate(); });
        refreshClimate();
    }
    int index = 0;
    for (const auto& def : GfsModels::all()) {
        table->insertRow(index);
        table->setItem(index, 0, new QTableWidgetItem{QString::fromStdString(def.id)});
        table->setItem(index, 1, new QTableWidgetItem{"-"});
        table->setItem(index, 2, new QTableWidgetItem);
        table->setItem(index, 3, new QTableWidgetItem);
        table->setItem(index, 4, new QTableWidgetItem{QString::fromStdString(def.label)});
        index++;
    }
}

SettingsSourcesBox::~SettingsSourcesBox() {
    generation->fetch_add(1);   // answers of a look in progress go nowhere
}

void SettingsSourcesBox::refreshServers() {
    const QSignalBlocker blocker{serversTable};
    serversTable->setRowCount(0);
    int r = 0;
    for (const auto& [name, entry] : Servers::table()) {
        serversTable->insertRow(r);
        auto * nameItem = new QTableWidgetItem{QString::fromStdString(name)};
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        QString places;
        for (const auto& base : entry.bases) {
            places += (places.isEmpty() ? "" : " | ") + QString::fromStdString(base);
        }
        auto * from = new QTableWidgetItem{QString::fromStdString(entry.origin)};
        from->setFlags(from->flags() & ~Qt::ItemIsEditable);
        auto * note = new QTableWidgetItem{QString::fromStdString(entry.note)};
        note->setFlags(note->flags() & ~Qt::ItemIsEditable);
        serversTable->setItem(r, 0, nameItem);
        serversTable->setItem(r, 1, new QTableWidgetItem{places});
        serversTable->setItem(r, 2, from);
        serversTable->setItem(r, 3, note);
        r++;
    }
    serversTable->setMinimumHeight(serversTable->horizontalHeader()->height() + std::max(r, 1) * 28 + 6);
    serversTable->setMaximumHeight(serversTable->minimumHeight());
    const auto last = Servers::lastFetch();
    serversStatus->setText(last.when.empty() ? "The project's file has not been read yet." : QString::fromStdString("The project's file, " + last.when + ": " + (last.ok ? "read (" : "not used (") + last.result + ")"));
}

void SettingsSourcesBox::entered() {
    refreshClimate();
    if (!looked) {
        start();
    }
}

void SettingsSourcesBox::refreshClimate() {
    const auto fields = climateFields();
    int ready = 0, working = 0, failed = 0;
    for (size_t i = 0; i < fields.size(); i++) {
        const auto state = fields[i].isStat ? climate().statisticsState(fields[i].stat) : climate().deviationState(fields[i].field);
        auto * item = climateTable->item(static_cast<int>(i), 1);
        if (state.ready) {
            item->setText("stored");
            item->setForeground(QBrush{QColor{30, 140, 60}});
            ready++;
        } else if (state.building) {
            item->setText("working out: " + QString::number(state.percent) + " %");
            item->setForeground(QBrush{});
            working++;
        } else if (!state.failed.empty()) {
            item->setText("failed: " + QString::fromStdString(state.failed));
            item->setForeground(QBrush{QColor{200, 40, 40}});
            failed++;
        } else {
            item->setText("not yet");
            item->setForeground(QBrush{Qt::gray});
        }
    }
    const int total = static_cast<int>(fields.size());
    climateSummary->setText(QString::number(ready) + " of " + QString::number(total) + " stored" + (working ? ", " + QString::number(working) + " being worked out" : QString{}) +
                            (failed ? ", " + QString::number(failed) + " failed (Download all tries again)" : QString{}) + ".");
    climateButton->setEnabled(ready + working < total);
    if (working > 0 && !climateTimer->isActive()) {
        climateTimer->start();
    } else if (working == 0) {
        climateTimer->stop();
    }
}

void SettingsSourcesBox::setRow(int row, const GfsRender::SourceCheck& check) {
    const bool good = check.ok;
    auto * status = table->item(row, 1);
    status->setText(check.skipped ? "skipped" : good ? "OK" : "PROBLEM");
    status->setForeground(check.skipped ? QBrush{Qt::gray} : good ? QBrush{QColor{30, 140, 60}} : QBrush{QColor{200, 40, 40}});
    table->item(row, 2)->setText(QString::fromStdString(check.run));
    table->item(row, 3)->setText(QString::number(check.seconds, 'f', 1));
    table->item(row, 4)->setText(QString::fromStdString(check.detail));
}

void SettingsSourcesBox::start() {
    if (running) {
        return;
    }
    running = true;
    looked = true;
    again->setEnabled(false);
    summary->setText("Looking...");
    const int mine = generation->fetch_add(1) + 1;
    std::vector<std::string> models;
    for (int row = 0; row < table->rowCount(); row++) {
        table->item(row, 1)->setText("looking...");
        table->item(row, 1)->setForeground(QBrush{});
        models.push_back(table->item(row, 0)->text().toStdString());
    }
    QPointer<SettingsSourcesBox> self{this};
    auto gen = generation;
    auto shared = session;
    std::thread{[self, gen, shared, models, mine] {
        std::atomic<size_t> next{0};
        std::atomic<int> okCount{0}, badCount{0};
        std::vector<std::future<void>> workers;
        for (int w = 0; w < 4; w++) {   // a few at a time: the servers are not hammered
            workers.push_back(std::async(std::launch::async, [&] {
                for (size_t i = next++; i < models.size(); i = next++) {
                    if (gen->load() != mine) {
                        return;
                    }
                    GfsRender::SourceCheck check;
                    try {
                        check = GfsRender::checkSource(*shared, models[i]);
                    } catch (const std::exception& e) {
                        check.model = models[i];
                        check.detail = std::string{"failed: "} + e.what();
                    }
                    (check.ok ? okCount : badCount) += check.skipped ? 0 : 1;
                    QMetaObject::invokeMethod(QCoreApplication::instance(), [self, gen, mine, i, check] {
                        if (self && gen->load() == mine) {
                            self->setRow(static_cast<int>(i), check);
                        }
                    }, Qt::QueuedConnection);
                }
            }));
        }
        for (auto& w : workers) {
            w.get();
        }
        const int ok = okCount, bad = badCount;
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, gen, mine, ok, bad] {
            if (self && gen->load() == mine) {
                self->running = false;
                self->again->setEnabled(true);
                self->summary->setText(QString::number(ok) + " models answer, " + QString::number(bad) + (bad == 1 ? " has a problem." : " have problems."));
            }
        }, Qt::QueuedConnection);
    }}.detach();
}
