// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/SarWindsViewer.h"
#include <algorithm>
#include <QVBoxLayout>
#include <QWidget>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"

SarWindsViewer::SarWindsViewer(Window * parent, const string& id, const string& name, int year)
    : Window{parent}
    , image{this}
    , stormId{id}
    , stormName{name}
    , stormYear{year}
{
    setAttribute(Qt::WA_DeleteOnClose);
    string label = id;
    std::transform(label.begin(), label.end(), label.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    setTitle("SAR winds - " + label + " " + name);
    schedule = new QLabel{"Reading NOAA STAR's SAR winds page...", this};
    schedule->setWordWrap(true);
    box.addWidgetReal(schedule, 0, Qt::Alignment{});
    list = new QListWidget{this};
    list->setFixedWidth(320);
    picture = new QComboBox{this};
    picture->addItems({"Wind speed", "Normalized radar cross section", "Where the scene is", "Wind speed contours", "Radial winds by quadrant", "Radial winds, all"});
    measurements = new QLabel{this};
    measurements->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    measurements->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto * left = new QWidget{this};
    auto * leftColumn = new QVBoxLayout{left};
    leftColumn->setContentsMargins(0, 0, 0, 0);
    leftColumn->addWidget(list, 1);
    leftColumn->addWidget(measurements);
    left->setFixedWidth(330);
    auto * right = new QWidget{this};
    auto * rightColumn = new QVBoxLayout{right};
    rightColumn->setContentsMargins(0, 0, 0, 0);
    rightColumn->addWidget(picture);
    rightColumn->addWidget(&image, 1);
    rowMain.addWidgetReal(left, 0, Qt::Alignment{});
    rowMain.addWidgetReal(right, 1, Qt::Alignment{});
    box.addLayout(rowMain);
    auto * credit = new QLabel{"Synthetic Aperture Radar (SAR) based tropical wind products were generated and provided through the STAR Water Surface Conditions Science Team's tropical cyclone "
                               "SAR winds (www.star.nesdis.noaa.gov/socd/mecb/sar/sarwinds_tropical.php). Preliminary results.", this};
    credit->setWordWrap(true);
    credit->setStyleSheet("color: gray;");
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1100, 760);
    QObject::connect(list, &QListWidget::currentRowChanged, this, [this] (int row) { showScene(row); });
    QObject::connect(picture, &QComboBox::currentIndexChanged, this, [this] (int) { showPicture(); });
    auto passes = std::make_shared<UtilitySarWinds::Schedule>();
    auto found = std::make_shared<vector<UtilitySarWinds::Scene>>();
    auto problem = std::make_shared<string>();
    auto scheduleProblem = std::make_shared<string>();
    new FutureVoid{this,
        [this, passes, found, problem, scheduleProblem] {
            *scheduleProblem = UtilitySarWinds::fetchSchedule(*passes);
            *problem = UtilitySarWinds::storm(stormId, stormName, stormYear, *found);
        },
        [this, passes, found, problem, scheduleProblem] {
            if (closed) {
                return;
            }
            string upper = stormId;
            std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
            string text;
            for (const auto& pass : passes->passes) {
                if (pass.id == upper) {
                    text += (text.empty() ? "" : "    ") + pass.time + "Z  " + pass.sensor;
                }
            }
            schedule->setText(QString::fromStdString(text.empty() ? (scheduleProblem->empty() ? "No SAR pass of this storm is scheduled right now (STAR's list, " + passes->updated + ")."
                                                                                              : "The schedule: " + *scheduleProblem)
                                                                  : "Scheduled passes (UTC): " + text + "    (STAR's list, " + passes->updated + ")"));
            scenes = *found;
            if (scenes.empty()) {
                measurements->setText(QString::fromStdString(*problem));
                return;
            }
            for (const auto& scene : scenes) {
                list->addItem(QString::fromStdString(scene.time + "   " + scene.sensor));
            }
            list->setCurrentRow(0);
        }};
}

void SarWindsViewer::showScene(int row) {
    if (row < 0 || static_cast<size_t>(row) >= scenes.size()) {
        return;
    }
    const auto& scene = scenes[static_cast<size_t>(row)];
    QString text;
    for (const auto& line : scene.measurements) {
        text += QString::fromStdString(line) + "\n";
    }
    measurements->setText(text.trimmed());
    showPicture();
}

void SarWindsViewer::showPicture() {
    const int row = list->currentRow();
    if (row < 0 || static_cast<size_t>(row) >= scenes.size()) {
        return;
    }
    const auto& scene = scenes[static_cast<size_t>(row)];
    const string * urls[] = {&scene.wind, &scene.nrcs, &scene.footprint, &scene.contours, &scene.radialQuads, &scene.radialAll};
    const auto& url = *urls[std::clamp(picture->currentIndex(), 0, 5)];
    if (url.empty()) {
        return;
    }
    new FutureBytes{this, url, [this] (const auto& bytes) {
        if (!closed && !bytes.isEmpty()) {
            image.setBytes(bytes);
        }
    }};
}
