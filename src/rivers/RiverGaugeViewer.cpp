// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "rivers/RiverGaugeViewer.h"
#include "dams/DamViewer.h"
#include <algorithm>
#include <cmath>
#include <ctime>
#include <QDateTime>
#include <QString>
#include "misc/ImageViewer.h"
#include "misc/TextViewerStatic.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "ui/CaptionedTile.h"
#include "ui/UiStandards.h"

namespace {
    using UtilityRivers::has;

    string number(double value, int decimals = 2) {
        return has(value) ? QString::number(value, 'f', decimals).toStdString() : string{"-"};
    }

    string timeText(long long time) {
        return QDateTime::fromSecsSinceEpoch(time, Qt::UTC).toString("yyyy-MM-dd HH:mm'Z'").toStdString();
    }

    // the change in the last `hours` hours, from the readings at the end of the series
    string change(const UtilityRivers::Series& series, int hours, const string& unit) {
        if (series.points.size() < 2) {
            return "";
        }
        const auto& last = series.points.back();
        const UtilityRivers::Point * before = nullptr;
        for (const auto& point : series.points) {
            if (point.time <= last.time - hours * 3600LL) {
                before = &point;
            }
        }
        if (before == nullptr) {
            return "";
        }
        const double delta = last.value - before->value;
        return string{delta >= 0.0 ? "+" : ""} + number(delta, 2) + " " + unit + " in " + std::to_string(hours) + " h";
    }

    // the highest reading of a series and when
    const UtilityRivers::Point * peak(const UtilityRivers::Series& series) {
        const UtilityRivers::Point * best = nullptr;
        for (const auto& point : series.points) {
            if (best == nullptr || point.value > best->value) {
                best = &point;
            }
        }
        return best;
    }

    QColor categoryColor(const string& category) {
        if (category == "major") {
            return QColor{160, 32, 240};
        }
        if (category == "moderate") {
            return QColor{227, 73, 72};
        }
        if (category == "minor") {
            return QColor{255, 153, 0};
        }
        return QColor{201, 166, 0};   // action
    }
}

RiverGaugeViewer::RiverGaugeViewer(Window * parent, const string& lid)
    : Window{parent}
    , lid{lid}
    , sw{this, box}
    , comboMode{this, {"Stage", "Flow"}}
    , comboRange{this, {"Last 3 days", "Last 7 days", "Last 14 days", "Last 30 days"}}
    , buttonRefresh{this, None, "Refresh"}
    , buttonImpacts{this, None, "What each stage floods"}
    , textTitle{this, "Loading " + lid + "..."}
    , textSubtitle{this, ""}
    , textNow{this, ""}
    , headingDam{this, ""}
    , textDam{this, ""}
    , buttonDam{this, None, "Dam history"}
    , chart{new HydrographChart{this}}
    , headingCurrent{this, "Now"}
    , textCurrent{this, ""}
    , headingModel{this, "Modelled (National Water Model)"}
    , textModel{this, ""}
    , headingHistory{this, "Record and outlook"}
    , textHistory{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("River gauge " + lid);
    textTitle.setBold();
    textSubtitle.setGray();
    textSubtitle.setWordWrap(true);
    textNow.setWordWrap(true);
    for (auto * heading : {&headingCurrent, &headingModel, &headingHistory, &headingDam}) {
        heading->setBold();
        heading->setBlue();
    }
    for (auto * text : {&textCurrent, &textModel, &textHistory, &textDam}) {
        text->setWordWrap(true);
    }
    comboRange.setIndex(1);
    comboMode.connect([this] { drawChart(); });
    comboRange.connect([this] { drawChart(); });
    buttonRefresh.connect([this] { load(); });
    buttonImpacts.connect([this] {
        if (detail) {
            new TextViewerStatic{this, impactsText(), "Flood impacts - " + detail->name, 800, 600};
        }
    });
    rowTop.addWidget(comboMode);
    rowTop.addWidget(comboRange);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonImpacts);
    rowTop.addStretch();
    box.addWidget(textTitle);
    box.addWidget(textSubtitle);
    box.addWidget(textNow);
    box.addWidget(headingDam);
    box.addWidget(textDam);
    box.addWidgetReal(buttonDam.getView(), 0, Qt::AlignLeft | Qt::AlignTop);
    headingDam.setVisible(false);
    textDam.setVisible(false);
    buttonDam.setVisible(false);
    buttonDam.connect([this] {
        if (dam != nullptr) {
            new DamViewer{this, *dam};
        }
    });
    box.addLayout(rowTop);
    box.addWidgetReal(chart);
    box.addWidget(headingCurrent);
    box.addWidget(textCurrent);
    box.addWidget(headingModel);
    box.addWidget(textModel);
    box.addWidget(headingHistory);
    box.addWidget(textHistory);
    flowImages.setEqualRowHeights(true);
    box.addLayout(flowImages);
    box.addStretch();
    load();
}

void RiverGaugeViewer::load() {
    textNow.setText(string{"Loading the gauge, its readings and the model..."});
    auto fresh = std::make_shared<UtilityRivers::Detail>();
    auto error = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    const auto id = lid;
    new FutureVoid{this,
        [fresh, error, ok, id] { *ok = UtilityRivers::loadDetail(id, *fresh, *error); },
        [this, fresh, error, ok] {
            if (closed) {
                return;
            }
            if (!*ok) {
                textTitle.setText(lid);
                textNow.setText(*error);
                return;
            }
            detail = fresh;
            build();
        }};
}

void RiverGaugeViewer::build() {
    const auto& d = *detail;
    setTitle("River gauge " + d.lid + " - " + d.name);
    textTitle.setText(d.name + "  (" + d.lid + ")");
    string subtitle = d.county.empty() ? d.state : d.county + " County, " + d.state;
    subtitle += "   NWS " + d.wfo + " / " + d.rfc;
    if (!d.usgsId.empty()) {
        subtitle += "   USGS " + d.usgsId;
    }
    if (!d.upstream.empty() || !d.downstream.empty()) {
        subtitle += "   (upstream " + (d.upstream.empty() ? string{"-"} : d.upstream) + ", downstream " + (d.downstream.empty() ? string{"-"} : d.downstream) + ")";
    }
    textSubtitle.setText(subtitle);
    string now;
    if (!d.observedStage.points.empty()) {
        const auto& last = d.observedStage.points.back();
        now = "Stage " + number(last.value) + " " + d.stageUnit + " at " + timeText(last.time) + "  -  " + UtilityRivers::statusLabel(d.observedCategory);
        if (!d.observedFlow.points.empty()) {
            now += "   flow " + number(d.observedFlow.points.back().value, 0) + " cfs";
        }
    } else {
        now = "No recent stage readings.  " + UtilityRivers::statusLabel(d.observedCategory);
    }
    now += "\nFlood stages: " + UtilityRivers::thresholdText(d);
    textNow.setText(now);
    textCurrent.setText(currentText());
    textModel.setText(modelText());
    textHistory.setText(historyText());
    drawChart();
    // a Corps of Engineers hydropower dam within 15 km: what it is releasing and generating now
    double km = 0.0;
    dam = UtilityDams::nearest(DamData::projects(), d.lat, d.lon, 15.0, &km);
    if (dam != nullptr) {
        const auto * project = dam;
        headingDam.setText(project->name + " (Corps of Engineers), " + std::to_string(static_cast<int>(std::lround(km))) + " km from this gauge");
        textDam.setText(string{"Loading the dam's release and power..."});
        headingDam.setVisible(true);
        textDam.setVisible(true);
        buttonDam.setVisible(true);
        auto latest = std::make_shared<DamData::Latest>();
        new FutureVoid{this,
            [latest, project] { *latest = DamData::loadLatestOne(*project); },
            [this, latest, project] {
                if (!closed && dam == project) {
                    textDam.setText(DamViewer::summary(*latest));
                }
            }};
    } else {
        headingDam.setVisible(false);
        textDam.setVisible(false);
        buttonDam.setVisible(false);
    }
    // the pictures: the weekly chance of exceeding each stage, and the NWS's own hydrograph
    flowImages.removeChildren();
    images.clear();
    const auto addPicture = [this] (const string& url, const string& caption) {
        if (url.empty()) {
            return;
        }
        images.emplace_back(this);
        images.back().imageSize = UiStandards::tileImage;
        const auto index = images.size() - 1;
        const auto text = QString::fromStdString(caption);
        flowImages.addWidgetReal(CaptionedTile::make(this, images.back().getView(), text, text, UiStandards::tileImage, true, UiStandards::tileWidth));
        images.back().connect([this, url, caption] { new ImageViewer{this, url, caption}; });
        new FutureBytes{this, url, [this, index] (const auto& bytes) {
            if (!closed && index < images.size() && !bytes.isEmpty()) {
                images[index].setBytes(bytes);
            }
        }};
    };
    addPicture(d.probabilityStageUrl, "Chance of exceeding each stage, by week (NWS)");
    addPicture(d.hydrographUrl, "NWS hydrograph");
}

void RiverGaugeViewer::drawChart() {
    if (!detail) {
        return;
    }
    const auto& d = *detail;
    const bool flow = comboMode.getIndex() == 1;
    static const int days[] = {3, 7, 14, 30};
    const auto now = static_cast<long long>(std::time(nullptr));
    const auto from = now - days[std::clamp(comboRange.getIndex(), 0, 3)] * 86400LL;
    const auto within = [from] (const UtilityRivers::Series& series) {
        std::vector<UtilityRivers::Point> out;
        for (const auto& point : series.points) {
            if (point.time >= from) {
                out.push_back(point);
            }
        }
        return out;
    };
    std::vector<HydrographChart::Line> lines;
    std::vector<HydrographChart::Level> levels;
    HydrographChart::Line observed{"Observed", QColor{42, 120, 214}, Qt::SolidLine, 2.2, within(flow ? d.observedFlow : d.observedStage)};
    HydrographChart::Line forecast{"NWS forecast", QColor{98, 80, 214}, Qt::SolidLine, 2.2, flow ? d.forecastFlow.points : d.forecastStage.points};
    // the National Water Model: the analysis (the model's estimate of the past) and the forecast, continuous; its flow goes through the rating curve for the stage view
    HydrographChart::Line model{QString{"National Water Model"} + (flow ? "" : " (via rating)"), QColor{27, 175, 122}, Qt::DashLine, 2.0, {}};
    for (const auto * series : {&d.modelAnalysis, &d.modelShort, &d.modelMedium}) {
        for (const auto& point : series->points) {
            if (point.time < from) {
                continue;
            }
            const double value = flow ? point.value : UtilityRivers::stageFromFlow(d.rating, point.value);
            if (has(value) && (model.points.empty() || point.time > model.points.back().time)) {
                model.points.push_back({point.time, value});
            }
        }
    }
    lines.push_back(observed);
    lines.push_back(forecast);
    lines.push_back(model);
    const auto level = [&] (const char * label, double stage, const char * category) {
        const double value = flow ? UtilityRivers::flowFromStage(d.rating, stage) : stage;
        if (has(value)) {
            levels.push_back({label, value, categoryColor(category)});
        }
    };
    level("Action", d.action, "action");
    level("Minor", d.minor, "minor");
    level("Moderate", d.moderate, "moderate");
    level("Major", d.major, "major");
    chart->setData(lines, levels, flow ? "Flow (cfs)" : "Stage (" + QString::fromStdString(d.stageUnit) + ")", now);
}

string RiverGaugeViewer::currentText() const {
    const auto& d = *detail;
    string text;
    if (!d.observedStage.points.empty()) {
        const auto& last = d.observedStage.points.back();
        text += "Observed: " + number(last.value) + " " + d.stageUnit + " at " + timeText(last.time) + "  (" + UtilityRivers::statusLabel(d.observedCategory) + ")\n";
        for (int hours : {1, 6, 24}) {
            const auto c = change(d.observedStage, hours, d.stageUnit);
            if (!c.empty()) {
                text += "   " + c + "\n";
            }
        }
    }
    if (!d.forecastStage.points.empty()) {
        const auto * crest = peak(d.forecastStage);
        text += "NWS forecast (issued " + d.forecastIssued + "): highest " + number(crest->value) + " " + d.stageUnit + " at " + timeText(crest->time) +
            ", ending " + number(d.forecastStage.points.back().value) + " " + d.stageUnit + " at " + timeText(d.forecastStage.points.back().time) + "\n";
    } else {
        text += "NWS forecast: none issued.  " + d.forecastNote + "\n";
    }
    return text;
}

string RiverGaugeViewer::modelText() const {
    const auto& d = *detail;
    if (d.modelAnalysis.points.empty() && d.modelShort.points.empty()) {
        return "The National Water Model has no flow series for this gauge's river reach (" + (d.reachId.empty() ? string{"no reach given"} : "reach " + d.reachId) + ").";
    }
    string text;
    if (!d.modelAnalysis.points.empty()) {
        const auto& last = d.modelAnalysis.points.back();
        text += "Model analysis (its estimate of the past two days), latest " + timeText(last.time) + ": " + number(last.value, 0) + " cfs";
        const double stage = UtilityRivers::stageFromFlow(d.rating, last.value);
        if (has(stage)) {
            text += "  (about " + number(stage) + " " + d.stageUnit + " on the gauge's rating curve)";
        }
        text += "\n";
        if (!d.observedFlow.points.empty()) {
            text += "   The gauge measured " + number(d.observedFlow.points.back().value, 0) + " cfs at " + timeText(d.observedFlow.points.back().time) + "\n";
        }
    }
    if (!d.modelShort.points.empty()) {
        const auto * crest = peak(d.modelShort);
        text += "Short range (" + std::to_string(d.modelShort.points.size()) + " hourly values from " + timeText(d.modelShort.points.front().time) + "): highest " +
            number(crest->value, 0) + " cfs at " + timeText(crest->time);
        const double stage = UtilityRivers::stageFromFlow(d.rating, crest->value);
        if (has(stage)) {
            text += "  (about " + number(stage) + " " + d.stageUnit + ")";
        }
        text += "\n";
    }
    text += "The model is a computer forecast for the river reach, not the official NWS forecast; its stage is the flow put through the gauge's rating curve.";
    return text;
}

string RiverGaugeViewer::historyText() const {
    const auto& d = *detail;
    string text;
    // the readings in the window the chart shows
    static const int days[] = {3, 7, 14, 30};
    const auto from = static_cast<long long>(std::time(nullptr)) - days[std::clamp(comboRange.getIndex(), 0, 3)] * 86400LL;
    double lo = UtilityRivers::none;
    double hi = UtilityRivers::none;
    double sum = 0.0;
    int count = 0;
    long long hiTime = 0;
    for (const auto& point : d.observedStage.points) {
        if (point.time < from) {
            continue;
        }
        lo = has(lo) ? std::min(lo, point.value) : point.value;
        if (!has(hi) || point.value > hi) {
            hi = point.value;
            hiTime = point.time;
        }
        sum += point.value;
        count += 1;
    }
    if (count > 0) {
        text += "Observed in the last " + std::to_string(days[std::clamp(comboRange.getIndex(), 0, 3)]) + " days: lowest " + number(lo) + ", highest " + number(hi) + " (" + timeText(hiTime) +
            "), average " + number(sum / count) + " " + d.stageUnit + "\n";
    }
    if (!d.crestsByStage.empty()) {
        text += "Highest crests on record:\n";
        for (size_t i = 0; i < std::min<size_t>(5, d.crestsByStage.size()); i += 1) {
            const auto& c = d.crestsByStage[i];
            text += "   " + c.time.substr(0, 10) + "   " + number(c.stage) + " " + d.stageUnit + (has(c.flow) && c.flow > 0.0 ? "   " + number(c.flow, 0) + " cfs" : "") + "\n";
        }
    }
    if (!d.crestsRecent.empty()) {
        text += "Recent crests (as listed by NWS):\n";
        for (size_t i = 0; i < std::min<size_t>(5, d.crestsRecent.size()); i += 1) {
            const auto& c = d.crestsRecent[i];
            text += "   " + c.time.substr(0, 10) + "   " + number(c.stage) + " " + d.stageUnit + "\n";
        }
    }
    if (!d.outlookInterval.empty()) {
        text += "Long-range flood outlook (" + d.outlookInterval + ", produced " + d.outlookProduced.substr(0, 10) + "), as published: minor " + d.outlookMinor +
            ", moderate " + d.outlookModerate + ", major " + d.outlookMajor + "\n";
    }
    // what the next stage up would flood, and what the stage now already does
    const double now = d.observedStage.points.empty() ? UtilityRivers::none : d.observedStage.points.back().value;
    if (has(now) && !d.impacts.empty()) {
        const UtilityRivers::Impact * next = nullptr;
        for (const auto& impact : d.impacts) {   // highest first: the last one above the stage is the nearest
            if (impact.stage > now) {
                next = &impact;
            }
        }
        if (next != nullptr) {
            text += "Next impact up, at " + number(next->stage, 1) + " " + d.stageUnit + ": " + next->statement + "\n";
        }
    }
    return text;
}

string RiverGaugeViewer::impactsText() const {
    const auto& d = *detail;
    string text = d.name + " (" + d.lid + ")\nFlood stages: " + UtilityRivers::thresholdText(d) + "\n\n";
    for (const auto& impact : d.impacts) {
        text += number(impact.stage, 1) + " " + d.stageUnit + "\n    " + impact.statement + "\n\n";
    }
    return d.impacts.empty() ? text + "No impact statements for this gauge." : text;
}
