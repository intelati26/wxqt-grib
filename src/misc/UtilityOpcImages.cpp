// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityOpcImages.h"
#include <algorithm>

const vector<string> UtilityOpcImages::labels{
    "Atlantic Surface Analysis",
    "Atlantic Wind/Wave Analysis",
    "Atlantic 24-hour 500 mb",
    "Atlantic 48-hour 500 mb",
    "Atlantic 96-hour 500 mb",
    "Atlantic 24-hour Surface",
    "Atlantic 48-hour Surface",
    "Atlantic 96-hour Surface",
    "Atlantic 24-hour Wind & Wave",
    "Atlantic 48-hour Wind & Wave",
    "Atlantic 96-hour Wind & Wave",
    "Atlantic 24-hour Wave period & Direction",
    "Atlantic 48-hour Wave period & Direction",
    "Atlantic 96-hour Wave period & Direction",
    "Pacific Surface Analysis",
    "Pacific Wind/Wave Analysis",
    "Pacific 24-hour 500 mb",
    "Pacific 48-hour 500 mb",
    "Pacific 96-hour 500 mb",
    "Pacific 24-hour Surface",
    "Pacific 48-hour Surface",
    "Pacific 96-hour Surface",
    "Pacific 24-hour Wind & Wave",
    "Pacific 48-hour Wind & Wave",
    "Pacific 96-hour Wind & Wave",
    "Pacific 24-hour Wave period & Direction",
    "Pacific 48-hour Wave period & Direction",
    "Pacific 96-hour Wave period & Direction",
    "Alaska/Arctic Surface Analysis",
    "Alaska Near Sea Surface Temperature (NSST)",
    "Alaska/Arctic 24-hour Surface",
    "Alaska/Arctic 48-hour Surface",
    "Alaska/Arctic 96-hour Surface",
    "Alaska/Arctic 24-hour Wind & Wave",
    "Alaska/Arctic 48-hour Wind & Wave",
    "Alaska/Arctic 96-hour Wind & Wave",
    "Alaska/Arctic 24-hour Wave period & Direction",
    "Alaska/Arctic 48-hour Wave period & Direction",
    "Alaska/Arctic 96-hour Wave period & Direction"
};

const vector<string> UtilityOpcImages::urls{
    "https://ocean.weather.gov/shtml/A_full_00hrsfc.gif",
    "https://ocean.weather.gov/shtml/ira1.gif",
    "https://ocean.weather.gov/shtml/A_24hr500.gif",
    "https://ocean.weather.gov/shtml/A_48hr500.gif",
    "https://ocean.weather.gov/shtml/A_96hr500.gif",
    "https://ocean.weather.gov/shtml/A_24hrsfc.gif",
    "https://ocean.weather.gov/shtml/A_48hrsfc.gif",
    "https://ocean.weather.gov/shtml/A_96hrsfc.gif",
    "https://ocean.weather.gov/shtml/A_24hrww.gif",
    "https://ocean.weather.gov/shtml/A_48hrww.gif",
    "https://ocean.weather.gov/shtml/A_96hrww.gif",
    "https://ocean.weather.gov/shtml/A_024hrwper_color.gif",
    "https://ocean.weather.gov/shtml/A_048hrwper_color.gif",
    "https://ocean.weather.gov/shtml/A_096hrwper_color.gif",
    "https://ocean.weather.gov/shtml/P_full_00hrsfc.gif",
    "https://ocean.weather.gov/shtml/irp1.gif",
    "https://ocean.weather.gov/shtml/P_24hr500.gif",
    "https://ocean.weather.gov/shtml/P_48hr500.gif",
    "https://ocean.weather.gov/shtml/P_96hr500.gif",
    "https://ocean.weather.gov/shtml/P_24hrsfc.gif",
    "https://ocean.weather.gov/shtml/P_48hrsfc.gif",
    "https://ocean.weather.gov/shtml/P_96hrsfc.gif",
    "https://ocean.weather.gov/shtml/P_24hrww.gif",
    "https://ocean.weather.gov/shtml/P_48hrww.gif",
    "https://ocean.weather.gov/shtml/P_96hrww.gif",
    "https://ocean.weather.gov/shtml/P_024hrwper_color.gif",
    "https://ocean.weather.gov/shtml/P_048hrwper_color.gif",
    "https://ocean.weather.gov/shtml/P_096hrwper_color.gif",
    "https://ocean.weather.gov/shtml/arctic/UA_LATEST.gif",
    "https://ocean.weather.gov/data/nsst/nsst_f00.png",
    "https://ocean.weather.gov/shtml/arctic/24SFC_LATEST.gif",
    "https://ocean.weather.gov/shtml/arctic/48SFC_LATEST.gif",
    "https://ocean.weather.gov/shtml/arctic/96SFC_LATEST.gif",
    "https://ocean.weather.gov/shtml/arctic/24WW_LATEST.gif",
    "https://ocean.weather.gov/shtml/arctic/48WW_LATEST.gif",
    "https://ocean.weather.gov/shtml/arctic/96WW_LATEST.gif",
    "https://ocean.weather.gov/shtml/AK_024hrwper_color.gif",
    "https://ocean.weather.gov/shtml/AK_048hrwper_color.gif",
    "https://ocean.weather.gov/shtml/AK_096hrwper_color.gif"
};

namespace {
    // "Atlantic" for "Atlantic 24-hour 500 mb", "Alaska/Arctic" for "Alaska/Arctic Surface Analysis"
    string areaOf(const string& label) {
        auto cut = label.size();
        for (const string marker : {" Surface", " Wind", " 24-hour", " 48-hour", " 96-hour", " Near"}) {
            const auto at = label.find(marker);
            if (at != string::npos) {
                cut = std::min(cut, at);
            }
        }
        return label.substr(0, cut);
    }

    // the kind of chart: a surface chart (analysis or forecast), 500 mb, wind & wave, wave period & direction
    string kindOf(const string& label) {
        if (label.find("500 mb") != string::npos) {
            return "500mb";
        }
        if (label.find("Wave period") != string::npos) {
            return "waveperiod";
        }
        if (label.find("Wind") != string::npos) {
            return "windwave";
        }
        if (label.find("Surface") != string::npos && label.find("Temperature") == string::npos) {
            return "surface";
        }
        return string{};   // stands alone (the sea surface temperature chart)
    }
}

vector<int> UtilityOpcImages::seriesOf(int index) {
    const auto& label = labels[static_cast<size_t>(index)];
    const auto kind = kindOf(label);
    if (kind.empty()) {
        return {index};
    }
    const auto area = areaOf(label);
    vector<int> out;
    for (size_t i = 0; i < labels.size(); i += 1) {
        if (areaOf(labels[i]) == area && kindOf(labels[i]) == kind) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

string UtilityOpcImages::seriesName(int index) {
    const auto& label = labels[static_cast<size_t>(index)];
    string name{"opc_"};
    for (const char c : areaOf(label) + "_" + kindOf(label)) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            name += c;
        } else if (c >= 'A' && c <= 'Z') {
            name += static_cast<char>(c - 'A' + 'a');
        } else if (name.back() != '_') {
            name += '_';
        }
    }
    return name;
}
