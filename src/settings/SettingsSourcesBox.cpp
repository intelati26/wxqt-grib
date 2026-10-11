// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsSourcesBox.h"
#include <future>
#include <thread>
#include <QBrush>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"

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
    again = new QPushButton{"Check again", this};
    QObject::connect(again, &QPushButton::clicked, this, [this] { start(); });
    row->addWidget(summary, 1);
    row->addWidget(again);
    layout->addLayout(row);
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

void SettingsSourcesBox::entered() {
    if (!looked) {
        start();
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
