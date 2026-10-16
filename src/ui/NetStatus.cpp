// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/NetStatus.h"
#include <set>
#include <tuple>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QUrl>
#include <QVBoxLayout>
#include "objects/NetManager.h"

QPointer<NetStatus> NetStatus::open;

void NetStatus::show(QWidget * parent) {
    if (!open) {
        open = new NetStatus{parent};
    }
    open->QDialog::show();
    open->raise();
    open->activateWindow();
}

NetStatus::NetStatus(QWidget * parent) : QDialog{parent, Qt::Tool} {
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Network");
    resize(900, 360);
    auto * column = new QVBoxLayout{this};
    totals = new QLabel{this};
    column->addWidget(totals);
    table = new QTableWidget{0, 7, this};
    table->setHorizontalHeaderLabels({"Priority", "State", "In", "Of", "Waited", "Callers", "Request"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    column->addWidget(table, 1);
    auto * buttons = new QHBoxLayout;
    for (const auto& [label, tip, what] : {std::tuple<const char *, const char *, int>{"Stop selected", "Cancel the selected requests: what waits on them gets nothing, and asks again only if you do", 0},
                                           {"Stop read-ahead and background", "Cancel every request that is not for what is on screen (hours read ahead, station files, tiles, archives)", 1},
                                           {"Stop everything", "Cancel every request, the ones for what is on screen too", 2}}) {
        auto * button = new QPushButton{label, this};
        button->setToolTip(tip);
        QObject::connect(button, &QPushButton::clicked, this, [this, what] { stop(what); });
        buttons->addWidget(button);
    }
    buttons->addStretch();
    column->addLayout(buttons);
    QObject::connect(&timer, &QTimer::timeout, this, [this] { refresh(); });
    timer.start(500);
    refresh();
}

void NetStatus::stop(int what) {
    if (what == 0) {
        std::set<std::string> urls;
        for (const auto& index : table->selectionModel()->selectedRows()) {
            urls.insert(table->item(index.row(), 6)->data(Qt::UserRole).toString().toStdString());
        }
        NetManager::cancelMatching([&urls] (const std::string& url, const std::string& range, NetManager::Priority) { return urls.count(url + "|" + range) > 0; });
    } else {
        NetManager::cancelMatching([what] (const std::string&, const std::string&, NetManager::Priority priority) { return what == 2 || priority != NetManager::Priority::Visible; });
    }
    refresh();
}

void NetStatus::refresh() {
    std::set<QString> selected;   // the table is rebuilt twice a second: what was selected stays selected
    for (const auto& index : table->selectionModel()->selectedRows()) {
        selected.insert(table->item(index.row(), 6)->data(Qt::UserRole).toString());
    }
    const auto rows = NetManager::snapshot();
    const auto sum = NetManager::totals();
    totals->setText(QString{"%1 waiting or downloading   -   %2 finished, %3 shared with a request already going, %4 MB received   -   %5"}
                        .arg(rows.size()).arg(sum.requests).arg(sum.reused).arg(sum.bytes / 1048576.0, 0, 'f', 1)
                        .arg(QString{"%1 retried, %2 cancelled   -   %3"}.arg(sum.retried).arg(sum.cancelled).arg(NetManager::enabled() ? "persistent client on" : "persistent client OFF")));
    table->setRowCount(static_cast<int>(rows.size()));
    const auto size = [] (long long bytes) { return bytes < 0 ? QString{"?"} : bytes < 10240 ? QString::number(bytes) + " B" : QString::number(bytes / 1024) + " KB"; };
    for (int i = 0; i < static_cast<int>(rows.size()); i++) {
        const auto& r = rows[static_cast<size_t>(i)];
        const QUrl url{QString::fromStdString(r.url)};
        const QString name = url.host() + "/..." + url.path().section('/', -1) + (r.range.empty() ? QString{} : "  " + QString::fromStdString(r.range));
        const QStringList cells{r.priority == NetManager::Priority::Visible ? "on screen" : r.priority == NetManager::Priority::Ahead ? "read ahead" : "background",
                                r.started ? (r.attempts > 0 ? QString{"downloading (try %1)"}.arg(r.attempts + 1) : QString{"downloading"}) : (r.attempts > 0 ? QString{"waiting to retry (%1)"}.arg(r.attempts) : QString{"queued"}), size(r.received), size(r.total), QString::number(r.seconds, 'f', 1) + " s", QString::number(r.waiters), name};
        for (int c = 0; c < cells.size(); c++) {
            table->setItem(i, c, new QTableWidgetItem{cells[c]});
        }
        table->item(i, 6)->setToolTip(QString::fromStdString(r.url));
        const QString key = QString::fromStdString(r.url + "|" + r.range);
        table->item(i, 6)->setData(Qt::UserRole, key);
        if (selected.count(key) > 0) {
            table->selectionModel()->select(table->model()->index(i, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
    }
    table->resizeColumnsToContents();
}
