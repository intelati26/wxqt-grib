// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/RouteItem.h"

namespace {
    // "MRMS (radar-derived: ...)" -> "MRMS"; "NSSL CAMs (experimental ...)" -> "NSSL CAMs"
    string shortLabel(string text) {
        for (const auto& cut : {string{", Ctrl-"}, string{" ("}}) {
            const auto at = text.find(cut);
            if (at != string::npos) {
                text.erase(at);
            }
        }
        while (!text.empty() && text.back() == ' ') {
            text.pop_back();
        }
        return text;
    }
}

RouteItem::RouteItem(const string& iconString, const string& toolTip, const function<void()>& fn)
    : iconString{iconString}
    , id{iconString}
    , toolTip{toolTip}
    , label{shortLabel(toolTip)}
    , fn{fn}
{}
