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
#include "misc/NiceWeatherCache.h"
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
    // a day is shown from the cache when it is there (the page of the day, or an earlier look, made it); else it is made, and kept for the page of the day
    const auto show = [this, day] (const NiceWeatherCache::Day * made, const std::string& error) {
        auto * label = labels[static_cast<size_t>(day)];
        if (made == nullptr) {
            label->setText(QString{"Day %1: no chart"}.arg(day + 1));
            (void)error;
        } else {
            label->setText(QString{"<b>Day %1</b>%2<br>%3"}.arg(day + 1).arg(made->valid.isValid() ? "  " + made->valid.toString("ddd MMM d") : QString{}).arg(QString::fromStdString(made->headline)));
            images[static_cast<size_t>(day)].setBytes(made->png);
            images[static_cast<size_t>(day)].resizeToWidth(static_cast<float>(width()) / across - 8);
        }
    };
    if (const auto cached = NiceWeatherCache::find(day)) {
        show(cached.get(), std::string{});
        load(day + 1);
        return;
    }
    auto result = std::make_shared<std::pair<std::shared_ptr<const NiceWeatherCache::Day>, std::string>>();
    new FutureVoid{this, [result, day] { result->first = NiceWeatherCache::make(day, result->second); }, [this, result, day, show] {
        show(result->first.get(), result->second);
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
