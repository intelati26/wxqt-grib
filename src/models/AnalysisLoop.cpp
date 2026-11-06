// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/AnalysisLoop.h"
#include <algorithm>
#include <limits>
#include <tuple>
#include <QTimeZone>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/UtilityModelNcepInterface.h"
#include "objects/FutureVoid.h"
#include "ui/UiStandards.h"
#include "util/Utility.h"

namespace {
    // label, the end of the models' names, the area drawn
    const std::vector<std::tuple<std::string, std::string, std::string>>& regions() {
        static const std::vector<std::tuple<std::string, std::string, std::string>> list{
            {"Contiguous US", "", "CONUS"}, {"Alaska", "-AK", "ALASKA"}, {"Hawaii", "-HI", "HAWAII-ISLANDS"}, {"Puerto Rico", "-PR", "PUERTO-RICO"}, {"Guam (RTMA only)", "-GU", "GUAM"}};
        return list;
    }
}

AnalysisLoop::AnalysisLoop(Window * parent, const std::string& area0, const std::vector<std::string>& charts, const QDateTime& fromTime, const QDateTime& toTime, const std::string& region)
    : LoopWindow{parent, area0, charts}
    , suffix{region}
    , urmaEnds{std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max())}
{
    setTitle("NWS analysis");
    std::vector<std::string> ids, names;
    if (const auto * def = GfsModels::find("RTMA"); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, ids, names);
    }
    setCharts(ids, names);
    auto * top = areaButton->parentWidget();
    const auto dateBox = [top] (const QDateTime& value, const QString& tip) {
        auto * edit = new QDateTimeEdit{value.toUTC(), top};
        edit->setDisplayFormat("yyyy-MM-dd HH 'Z'");
        edit->setTimeSpec(Qt::UTC);   // (Qt 6.4 has no setTimeZone on the date box)
        edit->setCalendarPopup(true);
        edit->setToolTip(tip);
        return edit;
    };
    regionBox = UiStandards::combo(top);
    for (const auto& [label, end, defaultArea] : regions()) {
        regionBox->addItem(QString::fromStdString(label), QString::fromStdString(end));
        if (end == region) {
            regionBox->setCurrentIndex(regionBox->count() - 1);
        }
    }
    regionBox->setToolTip("The analyses of an area: each has its own pair of RTMA and URMA (the URMA used where it has the hour, the RTMA for the newest hours). Guam has an RTMA only");
    from = dateBox(fromTime, "The first hour of the loop (UTC). The URMA goes back to 29 January 2014, the RTMA to 19 March 2013");
    to = dateBox(toTime, "The last hour of the loop (UTC). Hours the URMA has not come to yet (about 7 hours behind) are drawn from the RTMA");
    step = UiStandards::combo(top);
    for (const int hours : {1, 2, 3, 6, 12, 24}) {
        step->addItem(hours == 1 ? QString{"every hour"} : QString{"every %1 hours"}.arg(hours), hours);
    }
    step->setToolTip("The hours between the pictures: a long range is fewer, faster pictures at a larger step");
    topLayout->addWidget(regionBox);
    topLayout->addWidget(areaButton);
    topLayout->addWidget(new QLabel{"From", top});
    topLayout->addWidget(from);
    topLayout->addWidget(new QLabel{"To", top});
    topLayout->addWidget(to);
    topLayout->addWidget(step);
    topLayout->addWidget(panes);
    topLayout->addWidget(buildButton);
    topLayout->addStretch(1);
    grid->setMessage("Pick the range, then Draw the loop.");
    refreshAreaButton();
    QObject::connect(regionBox, &QComboBox::currentIndexChanged, this, [this] (int index) {   // another area: its own pair of models, drawn over the area that fits them
        if (index >= 0 && index < static_cast<int>(regions().size())) {
            suffix = regionBox->currentData().toString().toStdString();
            sector = std::get<2>(regions()[static_cast<size_t>(index)]);
            refreshAreaButton();
            invalidate("The area changed: Draw the loop again.");
        }
    });
}

AnalysisLoop * AnalysisLoop::openLatest(Window * parent, const std::string& chart) {
    auto sector = Utility::readPref("MODELNCEPSECTORLASTUSED", "CONUS");
    if (sector.compare(0, 6, "LOCAL:") != 0) {
        sector = "CONUS";
    }
    const auto last = QDateTime::currentDateTimeUtc().addSecs(-3600);
    const auto to = QDateTime{last.date(), QTime{last.time().hour(), 0}, QTimeZone::utc()};
    auto * loop = new AnalysisLoop{parent, sector, {chart}, to.addDays(-1), to, ""};
    loop->draw();
    return loop;
}

std::vector<std::string> AnalysisLoop::areaIds() const {
    const auto * def = GfsModels::find(pickerModel());
    return def != nullptr && def->sectors == GfsModels::Sectors::Conus ? UtilityModelNcepInterface::sectorsNbm : UtilityModelNcepInterface::sectorsGfs;
}

QString AnalysisLoop::captionOf(size_t frame, size_t pane) const {
    return QString::fromStdString(chartLabel(paneCharts[pane])) + "   " + QString::fromStdString(sources[frame]) + "  " + frameNames[frame];
}

std::string AnalysisLoop::titleOf(size_t frame) const {
    return "NWS analysis - " + frameNames[frame].toStdString() + " " + sources[frame];
}

std::string AnalysisLoop::saveName() const {
    return QString{"NWS-analysis_%1_%2"}.arg(from->dateTime().toUTC().toString("yyyyMMddHH"), to->dateTime().toUTC().toString("yyyyMMddHH")).toStdString();
}

void AnalysisLoop::build() {
    urmaEnds = std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max());
    const auto start = from->dateTime().toUTC();
    const auto end = to->dateTime().toUTC();
    const int hourStep = step->currentData().toInt();
    std::vector<std::string> hours;
    for (auto t = start; t <= end; t = t.addSecs(static_cast<qint64>(hourStep) * 3600)) {
        hours.push_back(t.toString("yyyyMMddHH").toStdString());
    }
    if (hours.empty()) {
        status->setText("The range is empty: From is after To.");
        return;
    }
    beginFrames(hours.size());
    const int mine = generation;
    times = hours;
    sources.assign(times.size(), std::string{});
    done = urma = rtma = failed = 0;
    firstError.clear();
    status->setText(QString{"Drawing %1 hours..."}.arg(times.size()));
    for (size_t index = 0; index < times.size(); index++) {
        renderFrame(index, mine, false);
    }
}

// One hour's panes (and the values under the pointer): from the URMA when it has the hour, else the RTMA. `again`: an hour already drawn, its values (let go of) wanted back, from the model it came from.
void AnalysisLoop::renderFrame(size_t index, int mine, bool again) {
    const auto charts = activeCharts();
    const auto valid = times[index];
    const auto urmaModel = "URMA" + suffix, rtmaModel = "RTMA" + suffix;
    const bool hasUrma = GfsModels::find(urmaModel) != nullptr;   // Guam has none
    const std::string only = again ? sources[index] : std::string{};
    const auto flag = cancel;
    const auto ends = urmaEnds;
    const auto area0 = sector;
    auto result = std::make_shared<std::pair<std::vector<Cell>, std::string>>();
    auto error = std::make_shared<std::string>();
    new FutureVoid{this, [result, error, valid, charts, flag, ends, area0, urmaModel, rtmaModel, hasUrma, only] {
                       if (flag->load()) {
                           return;
                       }
                       auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                       when.setTimeZone(QTimeZone::utc());
                       const long long seconds = when.toSecsSinceEpoch();
                       for (const auto& model : {urmaModel, rtmaModel}) {
                           if (!only.empty() && model != only) {
                               continue;
                           }
                           if (only.empty() && model == urmaModel && (!hasUrma || seconds >= ends->load())) {
                               continue;   // an earlier hour had no URMA: nor will this one
                           }
                           GfsRender::Session session;
                           std::vector<Cell> cells;
                           std::string problem;
                           for (const auto& chart : charts) {
                               GfsRender::Variant variant;
                               variant.validTime = valid;
                               variant.width = 900;
                               Cell cell;
                               cell.probe = std::make_shared<GfsChart::Probe>();
                               cell.bytes = GfsRender::png(session, model, chart, area0, "", 0, {}, problem, cell.probe.get(), variant);
                               if (cell.bytes.isEmpty()) {
                                   break;
                               }
                               cells.push_back(std::move(cell));
                           }
                           if (cells.size() == charts.size()) {
                               result->first = std::move(cells);
                               result->second = model;
                               return;
                           }
                           *error = problem;
                           if (model == urmaModel) {
                               auto current = ends->load();
                               while (seconds < current && !ends->compare_exchange_weak(current, seconds)) {
                               }
                           }
                       }
                   },
                   [this, mine, index, result, error, valid, again] {
                       if (closed || mine != generation) {
                           return;
                       }
                       if (again) {
                           storeAgain(index, std::move(result->first));
                           return;
                       }
                       ++done;
                       if (result->first.empty()) {
                           ++failed;
                           if (firstError.empty()) {
                               firstError = valid + ": " + *error;
                           }
                       } else {
                           sources[index] = result->second;
                           (result->second.compare(0, 4, "URMA") == 0 ? urma : rtma)++;
                           auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                           storeFrame(index, std::move(result->first), when.toString("yyyy-MM-dd HH") + "Z");
                       }
                       status->setText(QString{"%1 of %2 hours drawn: %3 from the URMA, %4 from the RTMA%5%6"}.arg(done).arg(frames.size()).arg(urma).arg(rtma)
                                           .arg(failed > 0 ? QString{", %1 could not be drawn"}.arg(failed) : QString{}).arg(failed > 0 && !firstError.empty() ? QString{" (first: %1)"}.arg(QString::fromStdString(firstError)) : QString{}));
                   }};
}
