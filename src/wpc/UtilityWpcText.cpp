// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityWpcText.h"

vector<MenuTitle> UtilityWpcText::titles{
    MenuTitle{"General Forecast Discussions", 9},
    MenuTitle{"Precipitation Discussions", 2},
    MenuTitle{"Hazards", 6},
    MenuTitle{"Ocean Weather", 33},
    MenuTitle{"Misc North American Weather", 5},
    MenuTitle{"Misc Intl Weather", 4},
    MenuTitle{"SPC", 8},
    MenuTitle{"NHC", 7},
    MenuTitle{"Great Lakes", 7},
    MenuTitle{"Space Weather", 6},
    MenuTitle{"Canada", 4}
};

const vector<string> UtilityWpcText::labels{
    "pmdspd: Short Range Forecast Discussion",
    "pmdepd: Extended Forecast Discussion",

    "pmdhi: Hawaii Extended Forecast Discussion",
    "pmdak: Alaska Extended Forecast Discussion",
    "pmdca: Tropical Discussion",
    "pmdmrd: Prognostic disc for 6-10 and 8-14 Day Outlooks",
    "pmd30d: Prognostic disc for Monthly Outlook",
    "pmd90d: Prognostic disc for long-lead Seasonal Outlooks",
    "pmdhco: Prognostic disc for long-lead Hawaiian Outlooks",

    "qpferd: Excessive Rainfall Discussion",
    "qpfhsd: Heavy Snow and Icing Discussion",

    "pmdthr: CPC US Hazards Outlook Days 8-14",
    "sccns1: Storm Summary 1",
    "sccns2: Storm Summary 2",
    "sccns3: Storm Summary 3",
    "sccns4: Storm Summary 4",
    "sccns5: Storm Summary 5",

    "miahsfat2: High Seas Forecasts - Atlantic",
    "miahsfep2: High Seas Forecasts - NE Pacific",
    "miahsfep3: High Seas Forecasts - SE Pacific",
    "nfdhsfat1: High Seas Forecasts - N Atlantic",
    "nfdhsfep1: High Seas Forecasts - N Pacific",
    "nfdhsfepi: High Seas Forecasts - E and C N Pacific",
    "offn09: Marine fsct for WA and ORE Waters",
    "offn08: Marine fcst for N CA Waters",
    "offn07: Marine fcst for S CA Waters",
    "offpz5: Offshore Waters fsct - PAC 1",
    "offpz6: Offshore Waters fsct - PAC 2",
    "nfdoffn35: (VOBRA) for Offshore Waters - WA/OR",
    "nfdoffn36: (VOBRA) for Offshore Waters - CA",
    "offn01: Navtex Marine fcst for NE US Waters",
    "offn02: Navtex Marine fcst for Atlantic States Waters",
    "offn03: Navtex Marine fcst for SE US Waters",
    "offn04: Navtex Marine fcst for SE Gulf Of Mexico",
    "offn05: Navtex Marine fcst for San Jaun Atlantic Waters",
    "offn06: Navtex Marine fcst for NW Gulf Of Mexico",
    "offnt1: Offshore Waters fsct - ATL 1",
    "offnt2: Offshore Waters fsct - ATL 2",
    "offnt3: Offshore Waters fsct - Caribbean & SW North Atlantic",
    "offnt4: Offshore Waters fsct - Gulf of Mexico",
    "nfdoffn31: (VOBRA) for Offshore Waters - New England",
    "nfdoffn32: (VOBRA) for Offshore Waters - West Central North Atlantic",
    "offajk: Offshore Waters fsct - Eastern Gulf of Alaska",
    "offaer: Offshore Waters fsct - Western Gulf of Alaska",
    "offalu: Offshore Waters fsct - Bering Sea",
    "offafg: Offshore Waters fsct - US Artic Waters",
    "offhfo: Offshore Waters fsct - Hawaii",
    "offn10: Navtex Marine fcst for Hawaii",
    "offn14: Navtex Marine fcst for Kodiak, AK (NW)",
    "offn15: Navtex Marine fcst for Kodiak, AK (Arctic)",

    "uvicac: NOAA/EPA Ultraviolet Index /UVI/ Forecast",
    "tptwrn: Hourly temp/wx for Western US",
    "tptcrn: Hourly temp/wx for Central US",
    "tptern: Hourly temp/wx for Eastern US",
    "tptnam: Hourly temp/wx for NA Cities",

    "rwrmx: Latin America and Caribbean Regional Weather Roundup",
    "tptcan: Canadian temp and precip Table",
    "tptint: Foreign temp and weather table",
    "tptlat: Latin American temp and weather table",

    "swomcd: Most recent MCD",
    "swody1: Day 1 Convective Outlook",
    "swody2: Day 2 Convective Outlook",
    "swody3: Day 3 Convective Outlook",
    "swod48: Day 4-8 Convective Outlook",
    "fwddy1: Day 1 Fire Weather Outlook",
    "fwddy2: Day 2 Fire Weather Outlook",
    "fwddy38: Days 3-8 Fire Weather Outlook",

    "miatwoat: ATL Tropical Weather Outlook",
    "miatwdat: ATL Tropical Weather Discussion",
    "miatwsat: ATL Monthly Tropical Summary",
    "miatwoep: EPAC Tropical Weather Outlook",
    "miatwdep: EPAC Tropical Weather Discussion",
    "miatwsep: EPAC Monthly Tropical Summary",
    "hfotwocp: CPAC Tropical Weather Outlook",

    "GLFLM: Lake Michigan - Open Lake Forecast",
    "GLFLS: Lake Superior - Open Lake Forecast",
    "GLFLH: Lake Huron - Open Lake Forecast",
    "GLFSC: Lake St Clair - Open Lake Forecast",
    "GLFLE: Lake Erie - Open Lake Forecast",
    "GLFLO: Lake Ontario - Open Lake Forecast",
    "GLFSL: Saint Lawrence River",

    "swpc3day: NOAA Geomagnetic Activity Observation and Forecast",
    "swpc3daygeo: NOAA Geomagnetic Activity Probabilities",
    "swpchigh: Weekly Highlights and Forecasts",
    "swpc27day: 27-day Space Weather Outlook Table",
    "swpcdisc: Forecast Discussion",
    "swpcwwa: Advisory Outlook",

    "focn45: Significant Weather Discussion, PASPC",
    "awcn11: Weather Summary Manitoba",
    "awcn13: Weather Summary Saskatchewan",
    "awcn15: Weather Summary Alberta"
};
