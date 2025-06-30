// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcStormReports.h"
#include <algorithm>
#include <set>
#include <QObject>
#include "common/GlobalVariables.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/WString.h"
#include "spc/LsrByWfo.h"
#include "spc/UtilitySpcStormReports.h"
#include "util/To.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

using std::set;

SpcStormReports::SpcStormReports(Window * parent, const string& spcStormReportsDay)
    : Window{parent}
    , sw{this, box}
    , calendar{this}
    , photo{this, Scaled}
    , comboBox{this}
    , lsrWfoButton{this, None, "LSR by WFO"}
    , spcStormReportsDay{spcStormReportsDay}
    , url{GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + ".gif"}
    , stormReportsUrl{GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + ".csv"}
{
    setTitle("SPC Storm Reports");
    calendar.connect([this] { onDateChanged(); });
    lsrWfoButton.connect([this] { new LsrByWfo{this}; });

    boxImage.addWidget(photo);
    boxImage.addWidget(calendar);
    box.addWidget(lsrWfoButton);
    box.addWidget(comboBox);
    box.addLayout(boxImage);
    box.addLayout(boxText);
    reload();
}

void SpcStormReports::onDateChanged() {
    const auto dayStr = WString::fixedLengthStringPad0(To::string(calendar.getDayOfMonth()), 2);
    const auto monthStr = WString::fixedLengthStringPad0(To::string(calendar.getMonth()), 2);
    const auto yearStr = UtilityString::substring(To::string(calendar.getYear()), 2, 4);
    const auto dateStr = yearStr + monthStr + dayStr;
    spcStormReportsDay = dateStr;
    url = GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + "_rpts.gif";
    stormReportsUrl = GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + "_rpts.csv";
    reload();
}

void SpcStormReports::updateReports(const string& html) {
    stormReports = UtilitySpcStormReports::process(WString::split(html, GlobalVariables::newline));
    vector<string> rawStates;
    for (const auto& stormReport : stormReports) {
        if (stormReport.damageHeader.empty() && !stormReport.state.empty()) {
            rawStates.push_back(stormReport.state);
        }
    }
    const set<string> tmpSet{rawStates.begin(), rawStates.end()};
    vector<string> uniqStates{tmpSet.begin(), tmpSet.end()};
    sort(uniqStates.begin(), uniqStates.end());
    vector<string> uniqStatesWithCount;
    for (const auto& item : uniqStates) {
        uniqStatesWithCount.push_back(item + ": " + To::string(UtilityList::count(rawStates, item)));
    }
    states = {"All"};
    addAll(states, uniqStatesWithCount);
    comboBox.setList(states);
    comboBox.connect([this] { filterReports(); });
    comboBox.setIndex(0);
    filterReports();
}

void SpcStormReports::filterReports() {
    const size_t index = comboBox.getIndex();
    if (index >= states.size()) {
        return;
    }
    const auto filter = WString::split(states[index], ":")[0];
    boxText.removeChildren();

    tableView = new SpcStormReportsTable(this, stormReports, filter);
    // tableView->tableView.clicked.connect(tableClicked);
    // tableView->tableView->connect([this] (QModelIndex * qmi) { tableClicked(qmi); });
    QObject::connect(tableView->tableView, &QTableView::clicked, this, [this] (const QModelIndex& qmi) { tableClicked(qmi); });
    boxText.addWidgetReal(tableView->getView());

    // reportItems.clear();
    // dividerLines.clear();
    // headerTextItems.clear();
    // for (const auto& stormReport : stormReports) {
    //     if (stormReport.damageHeader.empty() && filter == "All") {
    //         reportItems.emplace_back(this, stormReport);
    //         boxText.addLayout(reportItems.back());
    //         dividerLines.emplace_back(this);
    //         boxText.addWidget(dividerLines.back());
    //     } else if (stormReport.damageHeader.empty() && stormReport.state == filter) {
    //         reportItems.emplace_back(this, stormReport);
    //         boxText.addLayout(reportItems.back());
    //         dividerLines.emplace_back(this);
    //         boxText.addWidget(dividerLines.back());
    //     } else if (!stormReport.damageHeader.empty()) {
    //         headerTextItems.emplace_back(this, stormReport.damageHeader);
    //         boxText.addLayout(headerTextItems.back());
    //         dividerLines.emplace_back(this);
    //         boxText.addWidget(dividerLines.back());
    //     }
    // }
    // boxText.addStretch();
}

void SpcStormReports::tableClicked(const QModelIndex& qmi) {
    auto latLonString = tableView->model->data(qmi).toString().toStdString();
    auto items= WString::split(latLonString, ", ");
    auto lat = items[0];
    auto lon = items[1];
    auto url = "https://www.openstreetmap.org/?mlat=" + lat + "&mlon=" + lon + "&zoom=12#map=10/" + lat + "/" + lon;
    launchUrl(url);
}

void SpcStormReports::launchUrl(const string& url) {
    auto qUrl = QUrl(QString::fromStdString(url));
    QDesktopServices::openUrl(qUrl);
}

void SpcStormReports::reload() {
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureText{this, stormReportsUrl, [this] (const auto& s) { updateReports(s); }};
}
