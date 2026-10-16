// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcStormReportsTable.h"
#include <algorithm>
#include <iostream>
#include "util/Utility.h"


SpcStormReportsTable::SpcStormReportsTable(QWidget * parent, const vector<StormReport>& stormReports, const string& filter1) {
    this->stormReports = stormReports;
    model = new QStandardItemModel();
    model->setColumnCount(5);
    index = 0;
    for (auto& stormReport : stormReports) {
        if (stormReport.damageHeader == "" && filter1 == "All" && stormReport.text != "") {
            model->setItem(index, 0, new QStandardItem(QString::fromStdString(stormReport.latLon.printPretty())));
            model->setItem(index, 1, new QStandardItem(QString::fromStdString(stormReport.state + ", " + stormReport.city)));
            model->setItem(index, 2, new QStandardItem(QString::fromStdString(stormReport.time)));
            model->setItem(index, 3, new QStandardItem(QString::fromStdString(stormReport.address)));
            model->setItem(index, 4, new QStandardItem(QString::fromStdString(stormReport.magnitude + " - " + stormReport.damageReport)));
            index += 1;
        }
        else if (stormReport.damageHeader == "" && stormReport.state == filter1) {
            model->setItem(index, 0, new QStandardItem(QString::fromStdString(stormReport.latLon.printPretty())));
            model->setItem(index, 1, new QStandardItem(QString::fromStdString(stormReport.state + ", " + stormReport.city)));
            model->setItem(index, 2, new QStandardItem(QString::fromStdString(stormReport.time)));
            model->setItem(index, 3, new QStandardItem(QString::fromStdString(stormReport.address)));
            model->setItem(index, 4, new QStandardItem(QString::fromStdString(stormReport.magnitude + " - " + stormReport.damageReport)));
            index += 1;
        }
        else if (stormReport.damageHeader != "") {
            model->setItem(index, 0, new QStandardItem(QString::fromStdString(stormReport.damageHeader)));
            index += 1;
        }
    }
    tableView = new QTableView(parent);
    tableView->setModel(model);
    tableView->verticalHeader()->hide();
    model->setHorizontalHeaderLabels({"Lat, lon", "State, county", "Time (UTC)", "Location", "Report"});
    tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);   // the person drags the column edges
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->horizontalHeader()->setMinimumSectionSize(50);
    tableView->setWordWrap(true);
    tableView->setShowGrid(true);
    tableView->setColumnWidth(0, 160);
    tableView->setColumnWidth(1, 200);
    tableView->setColumnWidth(2, 200);
    tableView->setColumnWidth(3, 400);
    tableView->setColumnWidth(4, 500);
    tableView->resizeRowsToContents();
    tableView->setMinimumHeight(100);
    tableView->setFixedHeight(std::clamp(Utility::readPrefInt("STORM_REPORTS_TABLE_HEIGHT", 800), 150, 3000));
    // a double click on a column's edge fits it to its longest entry (up to a screen's width)
    QObject::connect(tableView->horizontalHeader(), &QHeaderView::sectionHandleDoubleClicked, tableView, [t = tableView] (int column) {
        t->resizeColumnToContents(column);
        if (t->columnWidth(column) > 1400) {
            t->setColumnWidth(column, 1400);
        }
        t->resizeRowsToContents();
    });
    // wrapped rows follow a column that is widened or narrowed
    QObject::connect(tableView->horizontalHeader(), &QHeaderView::sectionResized, tableView, [t = tableView] { t->resizeRowsToContents(); });
    tableView->show();
}

// void SpcStormReportsTable::launchUrl(const string& url) {
//     auto qUrl = QUrl(QString::fromStdString(url));
//     QDesktopServices::openUrl(qUrl);
// }

QWidget * SpcStormReportsTable::getView() {
    return tableView;
}
