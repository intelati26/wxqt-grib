// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SPCSTORMREPORTS_H
#define SPCSTORMREPORTS_H

#include <string>
#include <vector>
#include "spc/StormReport.h"
#include "ui/Button.h"
#include "ui/Calendar.h"
#include "ui/CardBlackHeaderText.h"
#include "ui/CardStormReportItem.h"
#include "ui/ComboBox.h"
#include "ui/DividerLine.h"
#include "ui/HBox.h"
#include "ui/Photo.h"
#include "ui/ScrolledWindow.h"
#include "ui/SpcStormReportsTable.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class SpcStormReports : public Window {
public:
    SpcStormReports(Window *, const string&);

private:
    void onDateChanged();
    void updateReports(const string&);
    void filterReports();
    void tableClicked(const QModelIndex&);
    void launchUrl(const string&);
    void reload();
    vector<DividerLine> dividerLines;
    vector<CardStormReportItem> reportItems;
    vector<CardBlackHeaderText> headerTextItems;
    VBox box;
    HBox boxImage;
    VBox boxText;
    ScrolledWindow sw;
    Calendar calendar;
    Photo photo;
    ComboBox comboBox;
    Button lsrWfoButton;
    string spcStormReportsDay;
    string url;
    string stormReportsUrl;
    vector<string> states;
    vector<StormReport> stormReports;
    SpcStormReportsTable * tableView;
};

#endif  // SPCSTORMREPORTS_H
