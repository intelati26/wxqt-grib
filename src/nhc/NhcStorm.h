// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NHCSTORM_H
#define NHCSTORM_H

#include <string>
#include <vector>
#include "nhc/NhcStormDetails.h"
#include "ui/Button.h"
#include "ui/ComboBox.h"
#include "ui/Image.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/TwoWidgetScroll.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class NhcStorm : public Window {
public:
    NhcStorm(Window *, const NhcStormDetails&);

private:
    void reload();
    void changeProduct();
    void launchGoes();
    Window * parent;
    // declared before comboboxProduct: members initialize in declaration
    // order, and comboboxProduct is constructed *from* this list, so this
    // order is load-bearing, not cosmetic.
    const vector<string> stormTextProducts{
        "MIATCP: Public Advisory",
        "MIATCM: Forecast Advisory",
        "MIATCD: Forecast Discussion",
        "MIAPWS: Wind Speed Probabilities"
    };
    VBox boxText;
    VBox boxImages;
    VBox box;
    NhcStormDetails stormData;
    TwoWidgetScroll sw;
    ComboBox comboboxProduct;
    Button goesButton;
    Text text;
    vector<Image> images;
    string goesUrl;
    string product;
    string textProductUrl;
    string office;
    Shortcut shortcut;
    vector<string> textProducts;
    vector<Shortcut> shortcuts;
    const vector<string> urls{
        "_5day_cone_with_line_and_wind_sm2.png",
        "_key_messages.png",
        "WPCQPF_sm2.gif",
        "WPCERO_sm2.gif",
        "_earliest_reasonable_toa_34_sm2.png",
        "_most_likely_toa_34_sm2.png",
        "_wind_probs_34_F120_sm2.png",
        "_wind_probs_50_F120_sm2.png",
        "_wind_probs_64_F120_sm2.png"};
};

#endif  // NHCSTORM_H
