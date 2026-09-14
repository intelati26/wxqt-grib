// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RTMA_H
#define RTMA_H

#include <string>
#include <vector>
#include <QByteArray>
#include "ui/AnimationBar.h"
#include "ui/BackForward.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// RTMA analysis image viewer. The hourly analyses are driven by a shared
// AnimationBar (scrub / play / export range / save); `times` is chronological
// reverse (index 0 = newest) since that's the order UtilityRtma::getTimes()
// returns and getUrl() expects, while the bar works in chronological (oldest
// first) global indices - comboIndexForChrono()/chronoForComboIndex() convert.
class Rtma : public Window {
public:
    explicit Rtma(Window *);

private:
    void reload();
    void moveBack();
    void moveForward();
    void changeProduct();
    void changeSector();
    void changeTime();
    void onRangeRequested(int rangeStart, int rangeEnd);
    void onFrameShown(int localIndex);
    void onScrub(int globalIndex);
    void onSave();
    void openFullImage();
    string titleForTime(const string&) const;
    int comboIndexForChrono(int chronoIndex) const;
    int chronoForComboIndex(int comboIndex) const;
    void resizeEventCustom() override;

    const string prefToken{"RMTA_PROD_INDEX"};
    const string prefTokenSector{"RMTA_SECTOR_INDEX"};
    ZoomImage image;
    ComboBox comboboxProduct;
    ComboBox comboboxSector;
    ComboBox comboboxTimes;
    BackForward backForward;
    AnimationBar animBar;
    int index;
    vector<string> times;      // chronological reverse (index 0 = newest)
    QByteArray currentBytes;
    vector<QByteArray> downloaded;   // worker -> callback handoff
    VBox box;
    HBox hbox;
};

#endif  // RTMA_H
