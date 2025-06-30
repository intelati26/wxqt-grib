// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSTORMREPORTSTABLE_H
#define SPCSTORMREPORTSTABLE_H

#include <string>
#include <vector>
#include <QUrl>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QDesktopServices>
#include <QHeaderView>
#include <QTableView>
#include <QWidget>
#include <cmath>
#include "spc/StormReport.h"
#include "ui/Widget2.h"

using std::string;
using std::vector;

class SpcStormReportsTable: public Widget2 {
public:
    SpcStormReportsTable(QWidget *, const vector<StormReport>&, const string&);
    QWidget * getView();

    vector<StormReport> stormReports;
    QStandardItemModel * model;
    int index;
    QTableView * tableView;
};

#endif  // SPCSTORMREPORTSTABLE_H
