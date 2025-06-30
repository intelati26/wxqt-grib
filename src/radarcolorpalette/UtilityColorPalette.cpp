// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityColorPalette.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "util/UtilityIO.h"

vector<string> UtilityColorPalette::getColorMapStringFromDisk(int product, const string& code) {
    string cmFileInt;
    switch (product) {
        case 94:
            cmFileInt = "colormapref" + WString::toLower(code) + ".txt";
            break;
        case 99:
            cmFileInt = "colormapbv" + WString::toLower(code) + ".txt";
            if (code == "CODENH") {
                cmFileInt = "colormapbvcod.txt";
            }
            break;
        case 135:
            cmFileInt = "colormap135cod.txt";
            break;
        case 155:
            cmFileInt = "colormap155cod.txt";
            break;
        case 161:
            cmFileInt = "colormap161cod.txt";
            break;
        case 163:
            cmFileInt = "colormap163cod.txt";
            break;
        case 159:
            cmFileInt = "colormap159cod.txt";
            break;
        case 134:
            cmFileInt = "colormap134cod.txt";
            break;
        case 165:
            cmFileInt = "colormap165cod.txt";
            break;
        case 172:
            cmFileInt = "colormap172cod.txt";
            break;
        case 56:
            cmFileInt = "colormap56.txt";
            break;
        case 19:
            cmFileInt = "colormap19.txt";
            break;
        case 30:
            cmFileInt = "colormap30.txt";
            break;
        case 41:
            cmFileInt = "colormap41.txt";
            break;
        default:
            break;
    }
    return UtilityIO::rawFileToStringArray(GlobalVariables::resDir + cmFileInt);
}
