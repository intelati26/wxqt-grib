// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "SpcStormReports.h"
#include <algorithm>
#include <set>
#include <QFile>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QLabel>
#include <QMessageBox>
#include <QObject>
#include <QSlider>
#include "ui/FlowBox.h"
#include "util/Utility.h"
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

namespace {
    bool isDate(const std::string& day) {   // "261015": a day given as yymmdd (not "today" / "yesterday")
        return day.size() == 6 && std::all_of(day.begin(), day.end(), [] (char c) { return c >= '0' && c <= '9'; });
    }
}

SpcStormReports::SpcStormReports(Window * parent, const string& spcStormReportsDay)
    : Window{parent}
    , sw{this, box}
    , calendar{this}
    , photo{this, Scaled}
    , comboBox{this}
    , lsrWfoButton{this, None, "LSR by WFO"}
    , exportButton{this, None, "Export the reports (CSV)"}
    , spcStormReportsDay{spcStormReportsDay}
    , url{GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + (isDate(spcStormReportsDay) ? "_rpts" : "") + ".gif"}
    , stormReportsUrl{GlobalVariables::nwsSPCwebsitePrefix + "/climo/reports/" + spcStormReportsDay + (isDate(spcStormReportsDay) ? "_rpts" : "") + ".csv"}
{
    setTitle("SPC Storm Reports");
    calendar.connect([this] { onDateChanged(); });
    lsrWfoButton.connect([this] { new LsrByWfo{this}; });

    exportButton.connect([this] { exportCsv(); });
    plotWidth = std::clamp(Utility::readPrefInt("STORM_REPORTS_PLOT_WIDTH", 1000), 300, 2400);
    photo.setToWidth(plotWidth + 100);
    boxImage.addWidget(photo);
    boxImage.addWidget(calendar);
    box.addWidget(lsrWfoButton);
    box.addWidget(exportButton);
    // the picture's size is the person's: the slider sets its width (kept for the next time)
    auto * sizeRow = new QWidget{this};
    auto * sizeLayout = new FlowLayout{8};   // wraps when the window is narrow
    sizeRow->setLayout(sizeLayout);
    sizeLayout->addWidget(new QLabel{"Size of the map:", sizeRow});
    auto * slider = new QSlider{Qt::Horizontal, sizeRow};
    slider->setRange(300, 2400);
    slider->setValue(plotWidth);
    slider->setFixedWidth(300);
    sizeLayout->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, this, [this] (int value) {
        plotWidth = value;
        photo.setToWidth(plotWidth + 100);
        Utility::writePrefInt("STORM_REPORTS_PLOT_WIDTH", value);
    });
    sizeLayout->addWidget(new QLabel{"Height of the table:", sizeRow});
    auto * tall = new QSlider{Qt::Horizontal, sizeRow};
    tall->setRange(150, 3000);
    tall->setValue(std::clamp(Utility::readPrefInt("STORM_REPORTS_TABLE_HEIGHT", 800), 150, 3000));
    tall->setFixedWidth(300);
    sizeLayout->addWidget(tall);
    QObject::connect(tall, &QSlider::valueChanged, this, [this] (int value) {
        Utility::writePrefInt("STORM_REPORTS_TABLE_HEIGHT", value);
        if (tableView && tableView->tableView) {
            tableView->tableView->setFixedHeight(value);
        }
    });
    box.addWidgetReal(sizeRow);
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

// the list as a CSV file (the state chosen in the box only, when one is): type, time (UTC, HHMM), magnitude, location, county, state, lat, lon, comments
void SpcStormReports::exportCsv() {
    if (stormReports.empty()) {
        QMessageBox::information(this, "Storm reports", "No reports are loaded yet.");
        return;
    }
    const size_t index = comboBox.getIndex();
    const string filter = index < states.size() ? WString::split(states[index], ":")[0] : string{"All"};
    const auto quote = [] (const string& text) {
        QString q = QString::fromStdString(text);
        q.replace('"', "\"\"");
        return "\"" + q + "\"";
    };
    QString out = "type,time_utc,magnitude,location,county,state,lat,lon,comments\n";
    string type;
    int rows = 0;
    for (const auto& r : stormReports) {
        if (!r.damageHeader.empty()) {
            type = r.damageHeader.substr(0, r.damageHeader.find(' '));
            continue;
        }
        if (r.text.empty() || (filter != "All" && r.state != filter)) {
            continue;
        }
        out += QString::fromStdString(type) + "," + quote(r.time) + "," + quote(r.magnitude) + "," + quote(r.address) + "," + quote(r.city) + "," + quote(r.state) + "," + QString::fromStdString(r.lat) + "," + QString::fromStdString(r.lon) + "," + quote(r.damageReport) + "\n";
        rows++;
    }
    const auto target = QFileDialog::getSaveFileName(this, "Export the storm reports", QString{"storm_reports_%1.csv"}.arg(QString::fromStdString(spcStormReportsDay)), "CSV files (*.csv)");
    if (target.isEmpty()) {
        return;
    }
    QFile file{target};
    if (!file.open(QIODevice::WriteOnly) || file.write(out.toUtf8()) < 0) {
        QMessageBox::warning(this, "Storm reports", "The file could not be written: " + target);
        return;
    }
    QMessageBox::information(this, "Storm reports", QString::number(rows) + " reports saved to " + target);
}

void SpcStormReports::launchUrl(const string& url) {
    auto qUrl = QUrl(QString::fromStdString(url));
    QDesktopServices::openUrl(qUrl);
}

void SpcStormReports::reload() {
    new FutureBytes{this, url, [this] (const auto& ba) { photo.setBytes(ba); }};
    new FutureText{this, stormReportsUrl, [this] (const auto& s) { updateReports(s); }};
}
