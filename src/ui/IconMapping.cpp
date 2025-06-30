// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "IconMapping.h"
#include "ui/Icon.h"

// TODO FIXME alphabetize

string IconMapping::toString(Icon icon) {
    switch (icon) {
        case Settings:
            return "baseline_settings_black_48dp.png";
        case Play:
            return "outline_play_arrow_black_48dp.png";
        case Update:
            return "reload.png";
        case Left:
            return "baseline_chevron_left_black_48dp.png";
        case Right:
            return "baseline_chevron_right_black_48dp.png";
        case Plus:
            return "baseline_zoom_in_black_48dp.png";
        case Minus:
            return "baseline_zoom_out_black_48dp.png";
        case Up:
            return "baseline_expand_less_black_48dp.png";
        case Down:
            return "baseline_expand_more_black_48dp.png";
        case Delete:
            return "outline_delete_black_48dp.png";
        case Radar:
            return "baseline_flash_on_black_48dp.png";
        default:
            return "";
    }
}
