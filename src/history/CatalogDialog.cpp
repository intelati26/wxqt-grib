// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/CatalogDialog.h"
#include <algorithm>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include "objects/FutureVoid.h"
#include "ui/Window.h"

CatalogDialog::CatalogDialog(Window * owner, std::vector<Source> sourceList, double lat, double lon, std::function<void(const HistoryEvent&)> add)
    : QDialog{static_cast<QWidget *>(owner)}, owner{owner}, sources{std::move(sourceList)}, add{std::move(add)}, lat{lat}, lon{lon}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Catalog");
    resize(980, 620);
    auto * column = new QVBoxLayout{this};
    auto * kindRow = new QHBoxLayout;
    kind = new QComboBox{this};
    for (const auto& s : sources) {
        kind->addItem(s.name);
    }
    kindRow->addWidget(new QLabel{"Catalog", this});
    kindRow->addWidget(kind, 1);
    column->addLayout(kindRow);
    status = new QLabel{this};
    status->setWordWrap(true);
    column->addWidget(status);
    auto * filters = new QHBoxLayout;
    from = new QSpinBox{this};
    from->setRange(1850, 2100);
    to = new QSpinBox{this};
    to->setRange(1850, 2100);
    to->setValue(QDate::currentDate().year());
    size = new QSpinBox{this};
    size->setRange(0, 1000);
    sizeText = new QLabel{this};
    whatText = new QLabel{"Show", this};
    what = new QComboBox{this};
    order = new QComboBox{this};
    load = new QPushButton{"Read again for this place", this};
    filters->addWidget(whatText);
    filters->addWidget(what);
    filters->addWidget(new QLabel{"From", this});
    filters->addWidget(from);
    filters->addWidget(new QLabel{"to", this});
    filters->addWidget(to);
    filters->addWidget(sizeText);
    filters->addWidget(size);
    filters->addWidget(new QLabel{"Order", this});
    filters->addWidget(order);
    filters->addWidget(load);
    filters->addStretch();
    column->addLayout(filters);
    table = new QTableWidget{0, 1, this};
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->hide();
    table->horizontalHeader()->setStretchLastSection(true);
    column->addWidget(table, 1);
    auto * buttons = new QHBoxLayout;
    addButton = new QPushButton{"Add as an event", this};
    addButton->setEnabled(false);
    auto * closeButton = new QPushButton{"Close", this};
    buttons->addStretch();
    buttons->addWidget(addButton);
    buttons->addWidget(closeButton);
    column->addLayout(buttons);
    QObject::connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
    QObject::connect(kind, &QComboBox::currentIndexChanged, this, [this] (int) {
        const auto& s = sources[static_cast<size_t>(kind->currentIndex())];
        table->setColumnCount(s.columns.size());
        table->setHorizontalHeaderLabels(s.columns);
        order->clear();
        order->addItems(s.orders);
        what->clear();
        what->addItems(s.whats);
        what->setVisible(!s.whats.isEmpty());
        whatText->setVisible(!s.whats.isEmpty());
        sizeText->setText("at least");
        size->setVisible(!s.sizeLabel.isEmpty());
        sizeText->setVisible(!s.sizeLabel.isEmpty());
        sizeText->setText("at least " + QString{});
        size->setSuffix(" " + s.sizeLabel);
        size->setValue(s.sizeDefault);
        load->setVisible(s.place);
        reload();
    });
    QObject::connect(load, &QPushButton::clicked, this, [this] { reload(); });
    QObject::connect(what, &QComboBox::currentIndexChanged, this, [this] (int) {
        if (sources[static_cast<size_t>(kind->currentIndex())].place) {
            reload();
        }
    });
    for (auto * spin : {from, to, size}) {
        QObject::connect(spin, &QSpinBox::valueChanged, this, [this] (int) { fill(); });
    }
    QObject::connect(order, &QComboBox::currentIndexChanged, this, [this] (int) { fill(); });
    QObject::connect(table, &QTableWidget::itemSelectionChanged, this, [this] { addButton->setEnabled(!table->selectedItems().isEmpty()); });
    QObject::connect(addButton, &QPushButton::clicked, this, [this] {
        const int row = table->currentRow();
        if (row >= 0 && row < static_cast<int>(shown.size())) {
            this->add(shown[static_cast<size_t>(row)]->event);
            close();
        }
    });
    emit kind->currentIndexChanged(0);
    show();
}

void CatalogDialog::reload() {
    const auto& source = sources[static_cast<size_t>(kind->currentIndex())];
    const int mine = ++generation;
    loaded.clear();
    shown.clear();
    table->setRowCount(0);
    status->setText("Reading " + source.name + "...");
    Params params;
    params.fromYear = 1850;
    params.toYear = 2100;
    params.what = std::max(0, what->currentIndex());
    params.lat = lat;
    params.lon = lon;
    auto result = std::make_shared<std::vector<Entry>>();
    auto problem = std::make_shared<std::string>();
    const auto function = source.load;
    new FutureVoid{owner, [function, params, result, problem] { *result = function(params, *problem); }, [this, mine, result, problem, guard = QPointer<CatalogDialog>{this}] {
        if (!guard || mine != generation) {
            return;
        }
        loaded = std::move(*result);
        error = *problem;
        if (!loaded.empty()) {
            int first = 9999, last = 0;
            for (const auto& e : loaded) {
                first = std::min(first, e.year);
                last = std::max(last, e.year);
            }
            from->blockSignals(true);
            to->blockSignals(true);
            from->setValue(std::max(from->minimum(), first));
            to->setValue(std::min(to->maximum(), last));
            from->blockSignals(false);
            to->blockSignals(false);
        }
        fill();
    }};
}

void CatalogDialog::fill() {
    const auto& source = sources[static_cast<size_t>(kind->currentIndex())];
    shown.clear();
    for (const auto& e : loaded) {
        if (e.year >= from->value() && e.year <= to->value() && (source.sizeLabel.isEmpty() || e.size >= size->value())) {
            shown.push_back(&e);
        }
    }
    const size_t by = static_cast<size_t>(std::max(0, order->currentIndex()));
    std::stable_sort(shown.begin(), shown.end(), [by] (const Entry * a, const Entry * b) {
        const double x = by < a->keys.size() ? a->keys[by] : 0.0, y = by < b->keys.size() ? b->keys[by] : 0.0;
        return x > y;
    });
    table->setRowCount(static_cast<int>(shown.size()));
    for (size_t i = 0; i < shown.size(); i++) {
        for (int c = 0; c < shown[i]->cells.size(); c++) {
            table->setItem(static_cast<int>(i), c, new QTableWidgetItem{shown[i]->cells[c]});
        }
    }
    table->resizeColumnsToContents();
    addButton->setEnabled(false);
    status->setText(error.empty() ? QString::number(shown.size()) + " of " + QString::number(loaded.size()) + ". " + source.note : QString::fromStdString(error));
}
