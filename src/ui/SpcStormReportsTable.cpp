// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcStormReportsTable.h"
#include <iostream>


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
    tableView->horizontalHeader()->hide();
    tableView->setWordWrap(true);
    tableView->setShowGrid(true);
    tableView->setColumnWidth(0, 160);
    tableView->setColumnWidth(1, 200);
    tableView->setColumnWidth(2, 200);
    tableView->setColumnWidth(3, 1000);
    tableView->resizeRowsToContents();
    tableView->setMinimumHeight(800);
    tableView->show();
}

// void SpcStormReportsTable::launchUrl(const string& url) {
//     auto qUrl = QUrl(QString::fromStdString(url));
//     QDesktopServices::openUrl(qUrl);
// }

QWidget * SpcStormReportsTable::getView() {
    return tableView;
}
