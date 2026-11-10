// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ActivityLabel.h"
#include "misc/NiceWeatherOverview.h"
#include <memory>
#include <QDateTime>
#include <QGridLayout>
#include <QVBoxLayout>
#include "gfs/GfsChart.h"
#include "ui/ActivityLabel.h"
#include "gfs/GfsRender.h"
#include "misc/NiceWeatherOutlook.h"
#include "misc/OutlookNav.h"
#include "objects/FutureVoid.h"

namespace {
    constexpr int days = 8;
    constexpr int across = 4;
}

NiceWeatherOverview::NiceWeatherOverview(Window * parent)
    : Window{parent}
    , sw{this, box}
{
    setTitle("Nice Weather Outlook, days 1-8");
    for (int i = 0; i < days; i++) {
        images.emplace_back(this);
    }
    auto * grid = new QGridLayout;
    grid->setSpacing(4);
    for (int i = 0; i < days; i++) {
        auto * label = new QLabel{QString{"Day %1: ..."}.arg(i + 1), this};
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("padding: 3px; background: #F0F0F0; color: #000000;");
        labels.push_back(label);
        auto * cell = new QVBoxLayout;
        cell->setSpacing(0);
        cell->addWidget(label);
        cell->addWidget(images[static_cast<size_t>(i)].getView());
        grid->addLayout(cell, i / across, i % across);
        images[static_cast<size_t>(i)].connect([this, i] { OutlookNav::open(this, OutlookNav::NiceDay, i); });
        images[static_cast<size_t>(i)].setNumberAcross(across, getWindowWidth());
    }
    box.addLayoutReal(grid);
    auto * notes = new QLabel{"National Blend of Models, the daytime of each day by the sun's time of each place. Click a day for its map and the regions. A work in progress.", this};
    notes->setWordWrap(true);
    box.addWidgetReal(notes);
    box.addStretch();
    box.addWidgetReal(new ActivityLabel{this});   // the eight days download together: what is in flight, and a click for the network list
    load(0);
}

void NiceWeatherOverview::load(int day) {
    if (day >= days) {
        return;
    }
    struct Result {
        QByteArray png;
        std::string headline, error;
        QDateTime valid;
    };
    auto result = std::make_shared<Result>();
    new FutureVoid{this, [result, day] {
        GfsRender::Session session;
        GfsRender::Variant variant;
        variant.width = 900;
        GfsChart::Probe probe;
        result->png = GfsRender::png(session, "NBM", "nice_weather", "CONUS", "", 24 * (day + 1), {}, result->error, &probe, variant);
        if (probe.valid()) {
            result->headline = NiceWeatherOutlook::headlineOf(probe.fields.front().grid);
            result->valid = probe.validUtc;
        }
    }, [this, result, day] {
        auto * label = labels[static_cast<size_t>(day)];
        if (result->png.isEmpty()) {
            label->setText(QString{"Day %1: no chart"}.arg(day + 1));
        } else {
            label->setText(QString{"<b>Day %1</b>%2<br>%3"}.arg(day + 1).arg(result->valid.isValid() ? "  " + result->valid.toString("ddd MMM d") : QString{}).arg(QString::fromStdString(result->headline)));
            images[static_cast<size_t>(day)].setBytes(result->png);
            images[static_cast<size_t>(day)].resizeToWidth(static_cast<float>(width()) / across - 8);
        }
        load(day + 1);
    }};
}

void NiceWeatherOverview::resizeEventCustom() {
    for (auto& image : images) {
        if (!image.bytes.isEmpty()) {
            image.resizeToWidth(static_cast<float>(width()) / across - 8);
        }
    }
}
