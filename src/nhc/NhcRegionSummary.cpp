// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NhcRegionSummary.h"
#include "common/GlobalVariables.h"

NhcRegionSummary::NhcRegionSummary(const NhcOceanEnum& region) {
    switch (region) {
        case ATL:
            titles = {
                "Atlantic Tropical Cyclones and Disturbances ",
                "ATL: Two-Day Graphical Tropical Weather Outlook",
                "ATL: Seven-Day Graphical Tropical Weather Outlook"
            };
            urls = {
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_0d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_2d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_atl_7d0.png"
            };
            break;
        case EPAC:
            titles = {
                "EPAC Tropical Cyclones and Disturbances ",
                "EPAC: Two-Day Graphical Tropical Weather Outlook",
                "EPAC: Seven-Day Graphical Tropical Weather Outlook"
            };
            urls = {
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_pac_0d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_pac_2d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_pac_7d0.png"
            };
            break;
        case CPAC:
            titles = {
                "CPAC Tropical Cyclones and Disturbances ",
                "CPAC: Two-Day Graphical Tropical Weather Outlook",
                "CPAC: Seven-Day Graphical Tropical Weather Outlook"
            };
            urls = {
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_cpac_0d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_cpac_2d0.png",
                GlobalVariables::nwsNhcWebsitePrefix + "/xgtwo/two_cpac_7d0.png"
            };
            break;
    }
}
