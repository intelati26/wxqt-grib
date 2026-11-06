// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SubhourlyViewer.h"
#include <algorithm>
#include <QTimeZone>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/ChartResolution.h"
#include "models/UtilityModelNcepInterface.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"

namespace {
    QString leadText(int minutes) {
        return QString{"+%1:%2"}.arg(minutes / 60).arg(minutes % 60, 2, 10, QChar{'0'});
    }
}

SubhourlyViewer::SubhourlyViewer(Window * parent, const std::string& modelId, const std::string& area0, const std::string& run, const std::vector<std::string>& charts)
    : LoopWindow{parent, area0, charts}
    , model{modelId}
{
    setTitle(model + " 15 minute surface fields");
    std::vector<std::string> ids, names;
    if (const auto * def = GfsModels::find(model); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, ids, names);   // (all of them until the ones with fields in the subhourly files are known)
    }
    setCharts(ids, names);
    for (const auto * id : {"sim_radar_comp", "2m_temp_10m_wnd", "10m_wnd_gust", "visibility"}) {
        if (paneCharts.size() < PaneGrid::maxPanes && std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
            paneCharts.push_back(id);
        }
    }
    auto * top = areaButton->parentWidget();
    runEdit = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addSecs(-3 * 3600), top};
    runEdit->setDisplayFormat("yyyy-MM-dd HH 'Z'");
    runEdit->setTimeSpec(Qt::UTC);   // (Qt 6.4 has no setTimeZone on the date box)
    runEdit->setCalendarPopup(true);
    runEdit->setToolTip("The run (UTC). The subhourly files stay on the server for a day or two");
    newestButton = UiStandards::button(top, "Newest", "The newest run that has its subhourly files");
    fromLead = new QSpinBox{top};
    fromLead->setRange(15, 1080);
    fromLead->setSingleStep(15);
    fromLead->setValue(15);
    fromLead->setSuffix(" min");
    fromLead->setToolTip("The first time of the loop, in minutes after the run's start");
    toLead = new QSpinBox{top};
    toLead->setRange(15, 1080);
    toLead->setSingleStep(15);
    toLead->setValue(180);
    toLead->setSuffix(" min");
    toLead->setToolTip("The last time of the loop (the files go to 18 hours: 1080 minutes)");
    step = UiStandards::combo(top);
    for (const int m : {15, 30, 60}) {
        step->addItem(QString{"every %1 min"}.arg(m), m);
    }
    buildButton->setEnabled(false);
    modelBox = UiStandards::combo(top);
    for (const auto& def : GfsModels::all()) {   // every model whose 15 minute files are read
        if (def.subhourly) {
            modelBox->addItem(QString::fromStdString(def.id), QString::fromStdString(def.id));
        }
    }
    modelBox->setCurrentIndex(std::max(0, modelBox->findData(QString::fromStdString(model))));
    modelBox->setToolTip("The model: the RRFS and the HRRR have 15 minute surface files");
    topLayout->addWidget(modelBox);
    topLayout->addWidget(new QLabel{"run", top});
    topLayout->addWidget(runEdit);
    topLayout->addWidget(newestButton);
    topLayout->addWidget(areaButton);
    topLayout->addWidget(new QLabel{"from", top});
    topLayout->addWidget(fromLead);
    topLayout->addWidget(new QLabel{"to", top});
    topLayout->addWidget(toLead);
    topLayout->addWidget(step);
    topLayout->addWidget(panes);
    topLayout->addWidget(resolution);
    topLayout->addWidget(buildButton);
    topLayout->addStretch(1);
    grid->setMessage("Looking for the newest run...");
    refreshAreaButton();
    QObject::connect(newestButton, &QPushButton::clicked, this, [this] { findNewestRun(); });
    QObject::connect(modelBox, &QComboBox::currentIndexChanged, this, [this] (int) {
        const auto chosen = modelBox->currentData().toString().toStdString();
        if (!chosen.empty() && chosen != model) {
            switchModel(chosen);
        }
    });
    if (!run.empty()) {
        auto t = QDateTime::fromString(QString::fromStdString(run), "yyyyMMddHH");
        t.setTimeZone(QTimeZone::utc());
        runEdit->setDateTime(t);
        runId = run;
        listCharts();
    } else {
        findNewestRun();
    }
}

// another model: its own charts and runs
void SubhourlyViewer::switchModel(const std::string& id) {
    model = id;
    invalidate("");
    setTitle(model + " 15 minute surface fields");
    std::vector<std::string> ids, names;
    if (const auto * def = GfsModels::find(model); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, ids, names);
        sector = def->sectors == GfsModels::Sectors::Conus || sector.empty() ? "CONUS" : sector;
    }
    setCharts(ids, names);
    refreshAreaButton();
    findNewestRun();
}

SubhourlyViewer * SubhourlyViewer::openNewest(Window * parent, const std::string& model) {
    return new SubhourlyViewer{parent, model, "CONUS", std::string{}, {"sim_radar_comp", "2m_temp_10m_wnd", "10m_wnd_gust", "visibility"}};
}

std::string SubhourlyViewer::summary() const {
    std::string out = model + " run " + runEdit->dateTime().toUTC().toString("yyyyMMddHH").toStdString() + " area " + sector + " leads " + std::to_string(fromLead->value()) + "-" + std::to_string(toLead->value()) + " charts";
    for (const auto& chart : paneCharts) {
        out += " " + chart;
    }
    return out;
}

std::vector<std::string> SubhourlyViewer::areaIds() const {
    const auto * def = GfsModels::find(model);
    return def != nullptr && def->sectors == GfsModels::Sectors::Conus ? UtilityModelNcepInterface::sectorsNbm : UtilityModelNcepInterface::sectorsGfs;
}

QString SubhourlyViewer::captionOf(size_t frame, size_t pane) const {
    return QString::fromStdString(chartLabel(paneCharts[pane])) + "   " + frameNames[frame];
}

std::string SubhourlyViewer::titleOf(size_t frame) const {
    return model + " 15 minute - " + frameNames[frame].toStdString();
}

// the newest run (counting back from the lag the model has) whose subhourly file of the first hour has the radar
void SubhourlyViewer::findNewestRun() {
    status->setText("Looking for the newest run with subhourly files...");
    newestButton->setEnabled(false);
    buildButton->setEnabled(false);
    const auto id = model;
    auto found = std::make_shared<std::string>();
    new FutureVoid{this, [id, found] {
                       const auto * def = GfsModels::find(id);
                       const int lag = def != nullptr ? def->source("").lagHours : 2;
                       auto base = QDateTime::currentDateTimeUtc().addSecs(-3600LL * lag);
                       base = QDateTime{base.date(), QTime{base.time().hour(), 0}, QTimeZone::utc()};
                       GfsRender::Session session;
                       for (int back = 0; back < 14; back++) {
                           const auto run = base.addSecs(-3600LL * back).toString("yyyyMMddHH").toStdString();
                           if (!GfsRender::availableMinutes(session, id, "sim_radar_comp", run, {60}).empty()) {
                               *found = run;
                               return;
                           }
                       }
                   },
                   [this, found] {
                       if (closed) {
                           return;
                       }
                       newestButton->setEnabled(true);
                       if (found->empty()) {
                           status->setText("No run with subhourly files was found in the last 14 hours: pick a run.");
                           grid->setMessage("No run with subhourly files was found.");
                           buildButton->setEnabled(true);
                           return;
                       }
                       auto t = QDateTime::fromString(QString::fromStdString(*found), "yyyyMMddHH");
                       t.setTimeZone(QTimeZone::utc());
                       runEdit->setDateTime(t);
                       runId = *found;
                       invalidate("");
                       listCharts();
                   }};
}

// the charts of the model that have their fields in the subhourly files of this run: the surface ones
void SubhourlyViewer::listCharts() {
    status->setText("Looking at which charts the subhourly files hold...");
    buildButton->setEnabled(false);
    const auto id = model;
    const auto run = runId;
    const auto ids = chartIds;
    const auto names = chartNames;
    auto usable = std::make_shared<std::pair<std::vector<std::string>, std::vector<std::string>>>();
    new FutureVoid{this, [id, run, ids, names, usable] {
                       GfsRender::Session session;
                       for (size_t i = 0; i < ids.size() && i < names.size(); i++) {
                           if (!GfsRender::availableMinutes(session, id, ids[i], run, {60}).empty()) {
                               usable->first.push_back(ids[i]);
                               usable->second.push_back(names[i]);
                           }
                       }
                   },
                   [this, usable] {
                       if (closed) {
                           return;
                       }
                       if (!usable->first.empty()) {
                           setCharts(usable->first, usable->second);
                       }
                       // a pane whose chart is not in the files gets the first one that is
                       for (size_t i = 0; i < paneCharts.size(); i++) {
                           if (std::find(chartIds.begin(), chartIds.end(), paneCharts[i]) == chartIds.end() && !chartIds.empty()) {
                               for (const auto& id : chartIds) {
                                   if (std::find(paneCharts.begin(), paneCharts.end(), id) == paneCharts.end()) {
                                       paneCharts[i] = id;
                                       break;
                                   }
                               }
                           }
                           grid->tile(i)->caption->setText(QString::fromStdString(chartLabel(paneCharts[i])));
                           grid->tile(i)->setMessage("Pick the times, then Draw the loop.");
                       }
                       buildButton->setEnabled(true);
                       status->setText(QString::number(chartIds.size()) + " charts have their fields in the subhourly files (the surface ones: no pressure levels, no hourly maxima or amounts).");
                   }};
}

void SubhourlyViewer::build() {
    runId = runEdit->dateTime().toUTC().toString("yyyyMMddHH").toStdString();
    const int each = step->currentData().toInt();
    std::vector<int> leads;
    for (int m = fromLead->value(); m <= toLead->value(); m += each) {
        leads.push_back(m);
    }
    if (leads.empty()) {
        status->setText("The range is empty: from is after to.");
        return;
    }
    beginFrames(leads.size());
    const int mine = generation;
    minutes = leads;
    done = failed = 0;
    firstError.clear();
    status->setText(QString{"Drawing %1 times..."}.arg(minutes.size()));
    for (size_t index = 0; index < minutes.size(); index++) {
        renderFrame(index, mine, false);
    }
}

void SubhourlyViewer::renderFrame(size_t index, int mine, bool again) {
    const auto charts = activeCharts();
    const int lead = minutes[index];
    const auto flag = cancel;
    const auto id = model, area0 = sector, run = runId;
    const double scale = ChartResolution::scale();   // (read here, on the screen's thread)
    auto result = std::make_shared<std::vector<Cell>>();
    auto error = std::make_shared<std::string>();
    new FutureVoid{this, [result, error, charts, lead, flag, id, area0, run, scale] {
                       if (flag->load()) {
                           return;
                       }
                       GfsRender::Session session;
                       std::vector<Cell> cells;
                       std::string problem;
                       for (const auto& chart : charts) {
                           GfsRender::Variant variant;
                           variant.minutes = lead;
                           variant.scale = scale;
                           Cell cell;
                           cell.probe = std::make_shared<GfsChart::Probe>();
                           cell.bytes = GfsRender::png(session, id, chart, area0, run, (lead + 59) / 60, {}, problem, cell.probe.get(), variant);
                           if (cell.bytes.isEmpty()) {
                               break;
                           }
                           cells.push_back(std::move(cell));
                       }
                       if (cells.size() == charts.size()) {
                           *result = std::move(cells);
                       } else {
                           *error = problem;
                       }
                   },
                   [this, mine, index, result, error, again] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (again) {
                           storeAgain(index, std::move(*result));
                           return;
                       }
                       ++done;
                       if (result->empty()) {
                           ++failed;
                           if (firstError.empty()) {
                               firstError = leadText(minutes[index]).toStdString() + ": " + *error;
                           }
                       } else {
                           auto valid = QDateTime::fromString(QString::fromStdString(runId), "yyyyMMddHH");
                           valid.setTimeZone(QTimeZone::utc());
                           valid = valid.addSecs(60LL * minutes[index]);
                           storeFrame(index, std::move(*result), QString::fromStdString(runId.substr(8, 2)) + "Z run " + leadText(minutes[index]) + "   valid " + valid.toString("ddd dd HH:mm") + "Z");
                       }
                       status->setText(QString{"%1 of %2 times drawn%3%4"}.arg(done).arg(frames.size())
                                           .arg(failed > 0 ? QString{", %1 could not be drawn"}.arg(failed) : QString{}).arg(failed > 0 && !firstError.empty() ? QString{" (first: %1)"}.arg(QString::fromStdString(firstError)) : QString{}));
                   }};
}
