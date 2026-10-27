#include "SevereHazardMatrixWidget.h"
#include <QVBoxLayout>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QApplication>
#include <QPointer>
#include <thread>
#include "severe/SevereHazardMatrixData.h"

SevereHazardMatrixWidget::SevereHazardMatrixWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

SevereHazardMatrixWidget::~SevereHazardMatrixWidget() {
    *m_alive = false;
}

void SevereHazardMatrixWidget::setupUi() {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_tableWidget = new QTableWidget(RowCount, 8, this);
    layout->addWidget(m_tableWidget);

    // Days 1 through 8 as columns
    QStringList dayHeaders{"Day 1", "Day 2", "Day 3", "Day 4", "Day 5", "Day 6", "Day 7", "Day 8"};
    m_tableWidget->setHorizontalHeaderLabels(dayHeaders);

    // Hazard Categories as rows
    QStringList categoryHeaders{
        "SPC Tornado",
        "SPC Wind",
        "SPC Hail",
        "SPC Any Severe",
        "WPC Heavy Rain (ERO)",
        "WPC Heavy Snow",
        "WPC Freezing Rain / Ice"
    };
    m_tableWidget->setVerticalHeaderLabels(categoryHeaders);

    configureTableFormatting();

    connect(m_tableWidget, &QTableWidget::cellClicked, this, [this](int row, int col) {
        emit cellClicked(row, col);
    });
}

void SevereHazardMatrixWidget::configureTableFormatting() {
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setSelectionMode(QAbstractItemView::NoSelection);
    m_tableWidget->setAlternatingRowColors(true);
    m_tableWidget->setGridStyle(Qt::SolidLine);

    clearMatrix();
}

void SevereHazardMatrixWidget::clearMatrix() {
    for (int r = 0; r < RowCount; ++r) {
        for (int c = 0; c < 8; ++c) {
            auto *item = new QTableWidgetItem("—");
            item->setTextAlignment(Qt::AlignCenter);
            item->setBackground(QColor(245, 245, 245));
            item->setForeground(QColor(120, 120, 120));
            m_tableWidget->setItem(r, c, item);
        }
    }
}

void SevereHazardMatrixWidget::updateHazardCell(RowIndex row, int dayColumn, const HazardCellData &data) {
    if (row < 0 || row >= RowCount || dayColumn < 0 || dayColumn >= 8) return;

    auto *item = m_tableWidget->item(row, dayColumn);
    if (!item) {
        item = new QTableWidgetItem();
        m_tableWidget->setItem(row, dayColumn, item);
    }

    item->setText(data.label.isEmpty() ? "—" : data.label);
    item->setTextAlignment(Qt::AlignCenter);
    item->setBackground(data.backgroundColor);
    item->setForeground(data.textColor);
    item->setToolTip(data.tooltip);
}

// the fills of the National Weather Service's outlook map layers
QColor SevereHazardMatrixWidget::getSpcCategoricalColor(const QString &category) {
    QString cat = category.trimmed().toUpper();
    if (cat == "MRGL" || cat == "MARGINAL") return QColor(102, 163, 102);
    if (cat == "SLGT" || cat == "SLIGHT")   return QColor(255, 224, 102);
    if (cat == "ENH"  || cat == "ENHANCED") return QColor(255, 163, 102);
    if (cat == "MDT"  || cat == "MODERATE") return QColor(224, 102, 102);
    if (cat == "HIGH")                     return QColor(238, 153, 238);
    if (cat == "TSTM")                     return QColor(193, 233, 193);
    return QColor(240, 240, 240);
}

QColor SevereHazardMatrixWidget::getWpcEroColor(const QString &category) {
    QString cat = category.trimmed().toUpper();
    if (cat == "MRGL" || cat == "MARGINAL") return QColor(56, 168, 0);
    if (cat == "SLGT" || cat == "SLIGHT")   return QColor(255, 254, 0);
    if (cat == "MOD"  || cat == "MODERATE") return QColor(245, 0, 0);
    if (cat == "HIGH")                     return QColor(255, 105, 197);
    return QColor(240, 240, 240);
}

void SevereHazardMatrixWidget::reload() {
    const auto alive = m_alive;
    QPointer<SevereHazardMatrixWidget> self{this};
    std::thread{[alive, self] {
        QString problem;
        auto cells = std::make_shared<std::vector<SevereHazardMatrixData::Cell>>(SevereHazardMatrixData::fetch(problem));
        QMetaObject::invokeMethod(qApp, [alive, self, cells, problem] {
            if (!*alive || self.isNull()) {
                return;
            }
            self->clearMatrix();
            for (const auto& c : *cells) {
                if (!c.label.isEmpty()) {
                    self->updateHazardCell(static_cast<RowIndex>(c.row), c.day, {c.label, c.background, c.text, c.tooltip});
                } else if (auto *item = self->m_tableWidget->item(c.row, c.day)) {
                    item->setToolTip(c.tooltip);
                }
            }
            emit self->loaded(problem);
        }, Qt::QueuedConnection);
    }}.detach();
}
