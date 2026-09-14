// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NHC_H
#define NHC_H

#include <memory>
#include <string>
#include <vector>
#include "common/GlobalVariables.h"
#include "nhc/CardNhcStormReportItem.h"
#include "nhc/ObjectNhc.h"
#include "ui/ComboBox.h"
#include "ui/ScrolledWindow.h"
#include "ui/Shortcut.h"
#include "ui/Window.h"

using std::string;
using std::vector;

class Nhc : public Window {
public:
    explicit Nhc(Window *);

private:
    void updateText();
    void launchImage(size_t);
    Window * parent;
    // declared before comboBoxText/comboBoxImages: members initialize in
    // declaration order, and those two are constructed *from* these lists,
    // so this order is load-bearing, not cosmetic.
    const vector<string> textProducts{
        "Text Products",
        "MIATWOAT: ATL Tropical Weather Outlook",
        "MIATWDAT: ATL Tropical Weather Discussion",
        "MIATWSAT: ATL Monthly Tropical Summary",
        "MIATWOEP: EPAC Tropical Weather Outlook",
        "MIATWDEP: EPAC Tropical Weather Discussion",
        "MIATWSEP: EPAC Monthly Tropical Summary",
        "HFOTWOCP: CNP Tropical Weather Outlook"
    };
    const vector<string> imageUrls{
        "",
        "https://www.ssd.noaa.gov/PS/TROP/DATA/RT/SST/PAC/20.jpg",
        "https://www.ssd.noaa.gov/PS/TROP/DATA/RT/SST/ATL/20.jpg",
        GlobalVariables::nwsNhcWebsitePrefix + "/tafb/sst_loop/14_pac.png",
        GlobalVariables::nwsNhcWebsitePrefix + "/tafb/sst_loop/14_atl.png",
        GlobalVariables::nwsNhcWebsitePrefix + "/tafb/sst_loop/14_pac_anom.png",
        GlobalVariables::nwsNhcWebsitePrefix + "/tafb/sst_loop/14_atl_anom.png",
    };
    const vector<string> imageLabels{
        "Image Products",
        "EPAC Daily Analysis",
        "ATL Daily Analysis",
        "EPAC Reynolds SST Daily Analysis",
        "ATL Reynolds SST Daily Analysis",
        "EPAC SST Anomaly",
        "ATL SST Anomaly"
    };
    VBox boxText;
    VBox box;
    ScrolledWindow sw;
    ComboBox comboBoxText;
    ComboBox comboBoxImages;
    ObjectNhc objectNhc;
    vector<std::unique_ptr<CardNhcStormReportItem>> stormCards;
    vector<Image> images;
    vector<string> urls;
    vector<Shortcut> shortcuts;
    vector<string> imageTitles;
};

#endif  // NHC_H
