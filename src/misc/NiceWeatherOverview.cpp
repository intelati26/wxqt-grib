// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ActivityLabel.h"
#include "misc/NiceWeatherOverview.h"
#include "misc/OutlookScreen.h"
#include <cmath>
#include <memory>
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
    made.resize(days);
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
    if (dynamic_cast<OutlookScreen *>(parent) == nullptr) {   // (in the Outlook screen the screen has the one)
        box.addWidgetReal(new ActivityLabel{this});   // the eight days download together: what is in flight, and a click for the network list
    }
    load(0);
}

void NiceWeatherOverview::load(int day) {
    if (day >= days) {
        return;
    }
    // a day is shown from the cache when it is there (the page of the day, or an earlier look, made it); else it is made, and kept for the page of the day
    const auto show = [this, day] (const std::shared_ptr<const NiceWeatherCache::Day>& shown, const std::string& error) {
        const auto * made = shown.get();
        this->made[static_cast<size_t>(day)] = shown;
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
        show(cached, std::string{});
        load(day + 1);
        if (day + 1 >= days) {
            colourDays();
        }
        return;
    }
    auto result = std::make_shared<std::pair<std::shared_ptr<const NiceWeatherCache::Day>, std::string>>();
    new FutureVoid{this, [result, day] { result->first = NiceWeatherCache::make(day, result->second); }, [this, result, day, show] {
        show(result->first, result->second);
        load(day + 1);
        if (day + 1 >= days) {
            colourDays();
        }
    }};
}

// Each day's box as the SPC summary has its days: the highest level the day reaches (over at least 1% of the area) on that level's colour, with the shares under it. A day with dangerous gross weather
// (Miserable over 1% of the area, or Nasty or worse over 10%) says so on the gross colour instead of its best level: a nice corner of such a day is not what the box is for.
void NiceWeatherOverview::colourDays() {
    static const char * const names[6] = {"No level", "Decent", "Fair", "Nice", "Beautiful", "Perfect"};
    static const char * const colors[6] = {"#F0F0F0", "#d6e7a3", "#9fd486", "#4fb868", "#1a8c9a", "#f5b800"};
    for (int i = 0; i < days; i++) {
        const auto& d = made[static_cast<size_t>(i)];
        if (!d) {
            continue;
        }
        auto * label = labels[static_cast<size_t>(i)];
        const QString when = d->valid.isValid() ? "  " + d->valid.toString("ddd MMM d") : QString{};
        QString text, style;
        if (d->shares.dangerous()) {
            const bool miserable = d->shares.miserable >= 1.0;
            text = QString{"<b>Day %1</b>%2: <b>Dangerous %3</b><br>%4% Nasty or worse%5"}.arg(i + 1).arg(when).arg(miserable ? "Miserable" : "Nasty").arg(d->shares.nasty, 0, 'f', 0)
                       .arg(miserable ? QString{", %1% Miserable"}.arg(d->shares.miserable, 0, 'f', 0) : QString{});
            style = miserable ? "padding: 3px; background: #a8141c; color: #FFFFFF;" : "padding: 3px; background: #e8603c; color: #000000;";
        } else {
            const int level = d->shares.valid ? d->shares.highest : 0;
            text = QString{"<b>Day %1</b>%2: <b>%3</b><br>%4"}.arg(i + 1).arg(when).arg(names[level]).arg(QString::fromStdString(d->headline));
            style = QString{"padding: 3px; background: %1; color: %2;"}.arg(colors[level]).arg(level == 4 ? "#FFFFFF" : "#000000");
        }
        label->setText(text);
        label->setStyleSheet(style);
    }
}

void NiceWeatherOverview::resizeEventCustom() {
    for (auto& image : images) {
        if (!image.bytes.isEmpty()) {
            image.resizeToWidth(static_cast<float>(width()) / across - 8);
        }
    }
}
