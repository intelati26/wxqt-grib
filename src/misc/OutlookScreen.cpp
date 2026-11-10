// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "OutlookScreen.h"
#include <QHBoxLayout>
#include <QLabel>
#include "misc/NiceWeatherOutlook.h"
#include "misc/NiceWeatherOverview.h"
#include "spc/SpcFireSummary.h"
#include "spc/SpcFireWeatherOutlook.h"
#include "spc/SpcSwoDay1.h"
#include "spc/SpcSwoStateGraphics.h"
#include "spc/SpcSwoSummary.h"
#include "spc/SpcTstormOutlooks.h"
#include "ui/ActivityLabel.h"
#include "ui/UiStandards.h"
#include "wpc/NationalImages.h"
#include "wpc/RainfallOutlook.h"
#include "wpc/RainfallOutlookSummary.h"

namespace {
    // the fire weather page has the days in a box of its own: one page for all of them
    int keyOf(OutlookNav::Kind kind, int value) {
        return static_cast<int>(kind) * 100 + (kind == OutlookNav::FireDay || kind == OutlookNav::NiceDay ? 0 : value);
    }
}

OutlookScreen::OutlookScreen(Window * parent, OutlookNav::Kind kind, int value)
    : Window{parent}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Outlooks");
    auto * row = new QWidget{this};
    auto * rowLayout = new QHBoxLayout{row};
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->addWidget(new QLabel{"Outlook:", row});
    combo = UiStandards::combo(row);
    for (const auto& d : OutlookNav::destinations()) {
        combo->addItem(d.label, static_cast<int>(d.kind) * 100 + d.value);
    }
    combo->setToolTip("The convective, fire weather, thunderstorm and excessive rainfall outlooks are pages of this window; the other products open in windows of their own");
    rowLayout->addWidget(combo);
    rowLayout->addStretch();
    stack = new QStackedWidget{this};
    box.addWidgetReal(row, 0, Qt::Alignment{});
    box.addWidgetReal(stack, 1, Qt::Alignment{});
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);
    QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] (int index) {
        if (index < 0) {
            return;
        }
        const int code = combo->itemData(index).toInt();
        showPage(static_cast<OutlookNav::Kind>(code / 100), code % 100);
    });
    showPage(kind, value);
}

void OutlookScreen::open(Window * from, OutlookNav::Kind kind, int value) {
    for (QWidget * w = from; w != nullptr; w = w->parentWidget()) {
        if (auto * screen = dynamic_cast<OutlookScreen *>(w)) {
            screen->showPage(kind, value);
            return;
        }
    }
    new OutlookScreen{from, kind, value};
}

Window * OutlookScreen::make(OutlookNav::Kind kind, int value) {
    Window::embedNextWindow();
    Window * page = nullptr;
    switch (kind) {
        case OutlookNav::SevereSummary: page = new SpcSwoSummary{this}; break;
        case OutlookNav::SevereDay: page = new SpcSwoDay1{this, value}; break;
        case OutlookNav::State: page = new SpcSwoStateGraphics{this, value}; break;
        case OutlookNav::FireSummary: page = new SpcFireSummary{this}; break;
        case OutlookNav::FireDay: page = new SpcFireWeatherOutlook{this, value}; break;
        case OutlookNav::Thunderstorm: page = new SpcTstormOutlooks{this}; break;
        case OutlookNav::NiceOverview: page = new NiceWeatherOverview{this}; break;
        case OutlookNav::NiceDay: page = new NiceWeatherOutlook{this, value}; break;
        case OutlookNav::RainfallSummary: page = new RainfallOutlookSummary{this}; break;
        case OutlookNav::NationalImages: page = new NationalImages{this}; break;
        default: page = new RainfallOutlook{this, value}; break;
    }
    page->setAttribute(Qt::WA_DeleteOnClose, false);   // the page goes with the window
    QObject::connect(page, &QWidget::windowTitleChanged, this, [this, page] (const QString& title) {
        if (stack->currentWidget() == page) {
            setWindowTitle(title);
        }
    });
    stack->addWidget(page);
    return page;
}

void OutlookScreen::showPage(OutlookNav::Kind kind, int value) {
    if (!OutlookNav::isPage(kind)) {   // a product with a window of its own: open it, and leave the page as it is
        OutlookNav::open(this, kind, value);
        if (shown >= 0) {
            combo->blockSignals(true);
            combo->setCurrentIndex(combo->findData(shown));
            combo->blockSignals(false);
        }
        return;
    }
    const int key = keyOf(kind, value);
    auto found = pages.find(key);
    Window * page = found != pages.end() ? found->second : (pages[key] = make(kind, value));
    if (kind == OutlookNav::FireDay) {
        if (auto * fire = dynamic_cast<SpcFireWeatherOutlook *>(page)) {
            fire->selectDay(value);
        }
    }
    if (kind == OutlookNav::NiceDay) {
        if (auto * nice = dynamic_cast<NiceWeatherOutlook *>(page)) {
            nice->selectDay(value);
        }
    }
    stack->setCurrentWidget(page);
    setWindowTitle(page->windowTitle());
    shown = static_cast<int>(kind) * 100 + (kind == OutlookNav::FireDay || kind == OutlookNav::NiceDay ? 0 : value);
    int index = combo->findData(shown);
    if (index < 0 && kind == OutlookNav::State) {   // state graphics of day 2 or 3: the list has the one entry
        index = combo->findData(static_cast<int>(kind) * 100 + 1);
    }
    if (index >= 0) {
        combo->blockSignals(true);
        combo->setCurrentIndex(index);
        combo->blockSignals(false);
    }
}

void OutlookScreen::closeEventCustom() {
    for (auto& [key, page] : pages) {   // each page closes as its own window would (stops its timers and loops)
        page->close();
    }
}
