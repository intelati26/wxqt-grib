// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/AnalysisLoop.h"
#include <algorithm>
#include <limits>
#include <QHBoxLayout>
#include <QPixmap>
#include <QTimeZone>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/UtilityModelNcepInterface.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "ui/UiStandards.h"

AnalysisLoop::AnalysisLoop(Window * parent, const std::string& sector, const std::vector<std::string>& charts, const QDateTime& fromTime, const QDateTime& toTime)
    : Window{parent}
    , sector{sector}
    , cancel{std::make_shared<std::atomic<bool>>(false)}
    , urmaEnds{std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max())}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("RTMA / URMA loop");
    std::vector<std::string> labels;
    if (const auto * def = GfsModels::find("RTMA"); def != nullptr) {
        UtilityModelNcepInterface::chartList(*def, chartIds, labels);
    }
    auto * top = new QWidget{this};
    auto * topLayout = new QHBoxLayout{top};
    topLayout->setContentsMargins(0, 0, 0, 0);
    const auto dateBox = [this, top] (const QDateTime& value, const QString& tip) {
        auto * edit = new QDateTimeEdit{value.toUTC(), top};
        edit->setDisplayFormat("yyyy-MM-dd HH 'Z'");
        edit->setTimeZone(QTimeZone::utc());
        edit->setCalendarPopup(true);
        edit->setToolTip(tip);
        return edit;
    };
    topLayout->addWidget(new QLabel{"From", top});
    from = dateBox(fromTime, "The first hour of the loop (UTC). The URMA goes back to 29 January 2014, the RTMA to 19 March 2013");
    topLayout->addWidget(from);
    topLayout->addWidget(new QLabel{"To", top});
    to = dateBox(toTime, "The last hour of the loop (UTC). Hours the URMA has not come to yet (about 7 hours behind) are drawn from the RTMA");
    topLayout->addWidget(to);
    step = UiStandards::combo(top);
    for (const int hours : {1, 2, 3, 6, 12, 24}) {
        step->addItem(hours == 1 ? QString{"every hour"} : QString{"every %1 hours"}.arg(hours), hours);
    }
    step->setToolTip("The hours between the pictures: a long range is fewer, faster pictures at a larger step");
    topLayout->addWidget(step);
    panes = UiStandards::combo(top);
    for (int n = 1; n <= 4; n++) {
        panes->addItem(n == 1 ? QString{"1 chart"} : QString{"%1 charts"}.arg(n), n);
    }
    panes->setToolTip("One chart, or two to four side by side (the same hours, one loop)");
    topLayout->addWidget(panes);
    buildButton = UiStandards::button(top, "Draw the loop", "Draw each hour of the range in the background; the loop plays as the hours arrive");
    topLayout->addWidget(buildButton);
    topLayout->addStretch(1);
    box.addWidgetReal(top, 0, Qt::Alignment{});
    auto * second = new QWidget{this};
    auto * secondLayout = new QHBoxLayout{second};
    secondLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 4; i++) {
        auto * combo = UiStandards::combo(second);
        for (size_t k = 0; k < chartIds.size(); k++) {
            combo->addItem(QString::fromStdString(labels[k]), QString::fromStdString(chartIds[k]));
        }
        const auto wanted = i < static_cast<int>(charts.size()) ? charts[static_cast<size_t>(i)] : std::string{};
        const auto at = std::find(chartIds.begin(), chartIds.end(), wanted);
        combo->setCurrentIndex(at != chartIds.end() ? static_cast<int>(at - chartIds.begin()) : std::min(i, static_cast<int>(chartIds.size()) - 1));
        chartBoxes.push_back(combo);
        secondLayout->addWidget(combo);
    }
    secondLayout->addStretch(1);
    box.addWidgetReal(second, 0, Qt::Alignment{});
    view = new QLabel{"Pick the range and the charts, then Draw the loop.", this};
    view->setAlignment(Qt::AlignCenter);
    view->setMinimumSize(360, 260);
    view->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    view->setStyleSheet("background: #ffffff; color: #555555;");
    box.addWidgetReal(view, 1, Qt::Alignment{});
    auto * bottom = new QWidget{this};
    auto * bottomLayout = new QHBoxLayout{bottom};
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    playButton = UiStandards::button(bottom, "Play");
    playButton->setEnabled(false);
    slider = new QSlider{Qt::Horizontal, bottom};
    slider->setEnabled(false);
    saveButton = UiStandards::button(bottom, "Save...", "Save the pictures drawn as a loop (GIF or video), or the picture on view");
    saveButton->setEnabled(false);
    bottomLayout->addWidget(playButton);
    bottomLayout->addWidget(slider, 1);
    bottomLayout->addWidget(saveButton);
    box.addWidgetReal(bottom, 0, Qt::Alignment{});
    status = new QLabel{"", this};
    UiStandards::muted(status);
    box.addWidgetReal(status, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1000, 800);
    panes->setCurrentIndex(std::clamp(static_cast<int>(charts.size()), 1, 4) - 1);
    QObject::connect(panes, &QComboBox::currentIndexChanged, this, [this] (int) { panesChanged(); });
    panesChanged();
    QObject::connect(buildButton, &QPushButton::clicked, this, [this] { build(); });
    QObject::connect(playButton, &QPushButton::clicked, this, [this] {
        if (timer.isActive()) {
            timer.stop();
            playButton->setText("Play");
        } else {
            timer.start(450);
            playButton->setText("Pause");
        }
    });
    QObject::connect(slider, &QSlider::valueChanged, this, [this] (int index) { showFrame(index); });
    QObject::connect(saveButton, &QPushButton::clicked, this, [this] { save(); });
    QObject::connect(&timer, &QTimer::timeout, this, [this] { advance(); });
}

void AnalysisLoop::closeEventCustom() {
    closed = true;
    cancel->store(true);
    timer.stop();
}

void AnalysisLoop::resizeEventCustom() {
    redraw();
}

void AnalysisLoop::panesChanged() {
    const int n = panes->currentData().toInt();
    for (int i = 0; i < 4; i++) {
        chartBoxes[static_cast<size_t>(i)]->setVisible(i < n);
    }
}

void AnalysisLoop::stop() {
    cancel->store(true);   // the hours not yet started are left
    ++generation;
}

void AnalysisLoop::build() {
    stop();
    cancel = std::make_shared<std::atomic<bool>>(false);
    urmaEnds = std::make_shared<std::atomic<long long>>(std::numeric_limits<long long>::max());
    const int mine = generation;
    const auto start = from->dateTime().toUTC();
    const auto end = to->dateTime().toUTC();
    const int hourStep = step->currentData().toInt();
    std::vector<std::string> times;
    for (auto t = start; t <= end; t = t.addSecs(static_cast<qint64>(hourStep) * 3600)) {
        times.push_back(t.toString("yyyyMMddHH").toStdString());
    }
    if (times.empty()) {
        status->setText("The range is empty: From is after To.");
        return;
    }
    frames.assign(times.size(), QByteArray{});
    frameNames.assign(times.size(), QString{});
    sources.assign(times.size(), std::string{});
    done = urma = rtma = failed = 0;
    firstError.clear();
    shown = -1;
    timer.stop();
    playButton->setText("Play");
    slider->setRange(0, static_cast<int>(times.size()) - 1);
    slider->setEnabled(true);
    playButton->setEnabled(true);
    saveButton->setEnabled(true);
    const int count = panes->currentData().toInt();
    std::vector<std::string> chosen;
    for (int i = 0; i < count; i++) {
        chosen.push_back(chartBoxes[static_cast<size_t>(i)]->currentData().toString().toStdString());
    }
    status->setText(QString{"Drawing %1 hours..."}.arg(times.size()));
    for (size_t index = 0; index < times.size(); index++) {
        const auto valid = times[index];
        auto result = std::make_shared<std::pair<QByteArray, std::string>>();   // the picture, and the model it came from
        auto error = std::make_shared<std::string>();
        const auto flag = cancel;
        const auto ends = urmaEnds;
        const auto area = sector;
        new FutureVoid{this, [result, error, valid, chosen, flag, ends, area] {
                           if (flag->load()) {
                               return;
                           }
                           auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                           when.setTimeZone(QTimeZone::utc());
                           const long long seconds = when.toSecsSinceEpoch();
                           for (const char * model : {"URMA", "RTMA"}) {
                               if (std::string{model} == "URMA" && seconds >= ends->load()) {
                                   continue;   // an earlier hour had no URMA: nor will this one
                               }
                               GfsRender::Session session;
                               std::string problem;
                               QByteArray bytes;
                               if (chosen.size() == 1) {
                                   GfsRender::Variant variant;
                                   variant.validTime = valid;
                                   variant.width = 900;
                                   bytes = GfsRender::png(session, model, chosen.front(), area, "", 0, {}, problem, nullptr, variant);
                               } else {
                                   std::vector<GfsRender::Panel> list;
                                   for (const auto& chart : chosen) {
                                       list.push_back({model, chart, ""});
                                   }
                                   bytes = GfsRender::sideBySide(session, list, area, valid, 2, chosen.size() == 2 ? 0.7 : 0.55, problem);
                               }
                               if (!bytes.isEmpty()) {
                                   result->first = bytes;
                                   result->second = model;
                                   return;
                               }
                               *error = problem;
                               if (std::string{model} == "URMA") {
                                   auto current = ends->load();
                                   while (seconds < current && !ends->compare_exchange_weak(current, seconds)) {
                                   }
                               }
                           }
                       },
                       [this, mine, index, result, error, valid] {
                           if (closed || mine != generation) {
                               return;
                           }
                           ++done;
                           if (result->first.isEmpty()) {
                               ++failed;
                               if (firstError.empty()) {
                                   firstError = valid + ": " + *error;
                               }
                           } else {
                               frames[index] = result->first;
                               sources[index] = result->second;
                               (result->second == "URMA" ? urma : rtma)++;
                               auto when = QDateTime::fromString(QString::fromStdString(valid), "yyyyMMddHH");
                               frameNames[index] = when.toString("yyyy-MM-dd HH") + "Z " + QString::fromStdString(result->second);
                               if (shown < 0) {
                                   showFrame(static_cast<int>(index));
                               }
                           }
                           status->setText(QString{"%1 of %2 hours drawn: %3 from the URMA, %4 from the RTMA%5%6"}.arg(done).arg(frames.size()).arg(urma).arg(rtma)
                                               .arg(failed > 0 ? QString{", %1 could not be drawn"}.arg(failed) : QString{}).arg(failed > 0 && !firstError.empty() ? QString{" (first: %1)"}.arg(QString::fromStdString(firstError)) : QString{}));
                       }};
    }
}

void AnalysisLoop::showFrame(int index) {
    if (index < 0 || index >= static_cast<int>(frames.size()) || frames[static_cast<size_t>(index)].isEmpty()) {
        return;
    }
    shown = index;
    if (slider->value() != index) {
        slider->blockSignals(true);
        slider->setValue(index);
        slider->blockSignals(false);
    }
    setTitle("RTMA / URMA loop - " + frameNames[static_cast<size_t>(index)].toStdString());
    redraw();
}

void AnalysisLoop::redraw() {
    if (shown < 0 || shown >= static_cast<int>(frames.size())) {
        return;
    }
    QPixmap picture;
    if (picture.loadFromData(frames[static_cast<size_t>(shown)])) {
        view->setPixmap(picture.scaled(view->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}

// the next hour that is drawn, going round
void AnalysisLoop::advance() {
    const int n = static_cast<int>(frames.size());
    for (int k = 1; k <= n; k++) {
        const int next = ((shown < 0 ? -1 : shown) + k) % n;
        if (!frames[static_cast<size_t>(next)].isEmpty()) {
            showFrame(next);
            return;
        }
    }
}

void AnalysisLoop::save() {
    std::vector<QByteArray> ready;
    for (const auto& bytes : frames) {
        if (!bytes.isEmpty()) {
            ready.push_back(bytes);
        }
    }
    if (ready.empty()) {
        return;
    }
    const auto name = QString{"URMA-RTMA_%1_%2"}.arg(from->dateTime().toUTC().toString("yyyyMMddHH"), to->dateTime().toUTC().toString("yyyyMMddHH"));
    UtilityAnimationExport::saveWithDialog(this, ready.size() >= 2 ? ready : std::vector<QByteArray>{}, 450, shown >= 0 ? frames[static_cast<size_t>(shown)] : ready.front(), name, QByteArray{}, false, name);
}
