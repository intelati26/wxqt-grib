// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsCatalogueBox.h"
#include <thread>
#include <QDesktopServices>
#include <QDir>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "gfs/UserCharts.h"
#include "misc/HelpViewer.h"
#include "ui/UiStandards.h"

SettingsCatalogueBox::SettingsCatalogueBox(QWidget * parent) : QWidget{parent}, session{std::make_shared<GfsRender::Session>()} {
    auto * column = new QVBoxLayout{this};
    auto * intro = new QLabel{"What a model's newest file lists, against what the charts ask for. <b>Check</b> reads the model's index (nothing is downloaded). A field you find here can become a chart of "
                              "your own: select it and press <b>Make a chart from this field</b>; it is written to your models folder and read at once.", this};
    intro->setWordWrap(true);
    column->addWidget(intro);
    auto * row = new QHBoxLayout;
    model = UiStandards::combo(this);
    for (const auto& def : GfsModels::all()) {
        if (!def.storm) {
            model->addItem(QString::fromStdString(def.id + "  " + def.label), QString::fromStdString(def.id));
        }
    }
    checkButton = UiStandards::button(this, "Check");
    filterBox = UiStandards::combo(this);
    filterBox->addItem("Show: every field", 0);
    filterBox->addItem("Show: not cataloged (the model has it, no chart draws it)", 1);
    filterBox->addItem("Show: gone", 2);
    filterBox->addItem("Show: never found", 3);
    filterBox->addItem("Show: found", 4);
    makeButton = UiStandards::button(this, "Make a chart");
    makeButton->setEnabled(false);
    makeButton->setToolTip("Makes a chart of your own from the selected field: a starter file in your models folder, read at once");
    auto * folder = UiStandards::button(this, "Your folder...");
    folder->setToolTip("Opens the folder of your own models: the chart files (*.chart.json) are in it");
    QObject::connect(folder, &QPushButton::clicked, this, [] {
        const auto path = QString::fromStdString(UserCharts::folder());
        QDir{}.mkpath(path);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    row->addWidget(model, 1);
    row->addWidget(checkButton);
    row->addWidget(filterBox);
    row->addWidget(makeButton);
    row->addWidget(folder);
    row->addWidget(HelpViewer::button(this, "guide:chart-files", "How a chart file is made, and what the catalogue check means"));
    column->addLayout(row);
    status = new QLabel{"Choose a model and press Check.", this};
    status->setWordWrap(true);
    column->addWidget(status);
    table = new QTableWidget{0, 5, this};
    table->setHorizontalHeaderLabels({"State", "Variable", "Level", "Detail", "Charts that use it"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    column->addWidget(table, 1);
    QObject::connect(checkButton, &QPushButton::clicked, this, [this] { check(); });
    QObject::connect(filterBox, &QComboBox::currentIndexChanged, this, [this] (int) { fill(); });
    QObject::connect(model, &QComboBox::currentIndexChanged, this, [this] (int) {
        haveReport = false;
        generation++;
        table->setRowCount(0);
        makeButton->setEnabled(false);
        status->setText("Press Check to read this model's index.");
    });
    QObject::connect(table, &QTableWidget::itemSelectionChanged, this, [this] { makeButton->setEnabled(!table->selectedItems().isEmpty()); });
    QObject::connect(makeButton, &QPushButton::clicked, this, [this] { makeChart(); });
}

void SettingsCatalogueBox::run(const QString& modelId, int filter, bool make) {
    model->setCurrentIndex(std::max(0, model->findData(modelId)));
    filterBox->setCurrentIndex(filter);
    check();
    if (make) {   // once the check has filled the table: the first row becomes a chart
        auto * timer = new QTimer{this};
        timer->setInterval(500);
        int waited = 0;
        QObject::connect(timer, &QTimer::timeout, this, [this, timer, waited]() mutable {
            if ((haveReport && table->rowCount() > 0) || ++waited > 120) {
                timer->stop();
                table->selectRow(0);
                makeChart();
            }
        });
        timer->start();
    }
}

void SettingsCatalogueBox::check() {
    const int mine = ++generation;
    const auto id = model->currentData().toString().toStdString();
    checkButton->setEnabled(false);
    status->setText("reading the index of the newest run...");
    QPointer<SettingsCatalogueBox> self{this};
    auto shared = session;
    std::thread{[self, mine, id, shared] {
        const auto result = GfsRender::checkCatalog(*shared, id);
        QMetaObject::invokeMethod(QCoreApplication::instance(), [self, mine, result] {
            if (!self || self->generation != mine) {
                return;
            }
            self->checkButton->setEnabled(true);
            self->report = result;
            self->haveReport = true;
            if (!result.checked) {
                self->table->setRowCount(0);
                self->status->setText("Not checked: " + QString::fromStdString(result.note));
                return;
            }
            self->status->setText(QString::fromStdString(result.label + ", run " + result.run + ": " + result.summary()));
            self->fill();
        }, Qt::QueuedConnection);
    }}.detach();
}

void SettingsCatalogueBox::fill() {
    table->setRowCount(0);
    if (!haveReport || !report.checked) {
        return;
    }
    const int what = filterBox->currentData().toInt();
    struct Group {
        const char * name;
        const std::vector<CatalogCheck::Field> * fields;
        int kind;
        QColor color;
    };
    const Group groups[] = {{"Gone", &report.gone, 2, QColor{200, 40, 40}}, {"Never found", &report.neverFound, 3, QColor{200, 130, 20}}, {"Found", &report.found, 4, QColor{30, 140, 60}},
                            {"Not cataloged", &report.notCataloged, 1, QColor{90, 90, 90}}};
    for (const auto& g : groups) {
        if (what != 0 && what != g.kind) {
            continue;
        }
        for (const auto& f : *g.fields) {
            const int r = table->rowCount();
            table->insertRow(r);
            auto * state = new QTableWidgetItem{g.name};
            state->setForeground(QBrush{g.color});
            table->setItem(r, 0, state);
            table->setItem(r, 1, new QTableWidgetItem{QString::fromStdString(f.variable)});
            table->setItem(r, 2, new QTableWidgetItem{QString::fromStdString(f.level)});
            table->setItem(r, 3, new QTableWidgetItem{QString::fromStdString(f.detail)});
            QString users;
            const auto found = report.usedBy.find(CatalogCheck::key(f));
            if (found != report.usedBy.end()) {
                for (const auto& chart : found->second) {
                    users += (users.isEmpty() ? "" : ", ") + QString::fromStdString(chart);
                }
            }
            table->setItem(r, 4, new QTableWidgetItem{users});
        }
    }
}

void SettingsCatalogueBox::makeChart() {
    const auto rows = table->selectionModel()->selectedRows();
    if (rows.isEmpty() || !haveReport) {
        return;
    }
    const int r = rows.front().row();
    const auto variable = table->item(r, 1)->text().toStdString(), level = table->item(r, 2)->text().toStdString(), detail = table->item(r, 3)->text().toStdString();
    const auto chart = UserCharts::starter(report.model, variable, level, detail);
    std::string path, why;
    if (!UserCharts::writeStarter(chart, path, why)) {
        status->setText("Could not make the chart: " + QString::fromStdString(why));
        return;
    }
    GfsChart::refreshUserCharts(true);
    QString problems;
    for (const auto& p : UserCharts::problems()) {
        problems += (problems.isEmpty() ? "" : "  |  ") + QString::fromStdString(p);
    }
    status->setText(QString::fromStdString("Written: " + path + ". It is in the Model Viewer under \"Your charts\" of " + report.model + " (a new Model Viewer window, or the next time the chart list opens).") +
                    (chart.note.empty() ? QString{} : QString::fromStdString("  Check: " + chart.note + ".")) + (problems.isEmpty() ? QString{} : "  Problems in your chart files: " + problems));
}
