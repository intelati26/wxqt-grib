// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityGoesFullDisk.h"
#include "common/GlobalVariables.h"
#include "objects/WString.h"
#include "settings/UtilityLocation.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

vector<string> UtilityGoesFullDisk::getAnimation(const string& urlOriginal, [[maybe_unused]] const string& unused1, [[maybe_unused]] size_t unused2) {
    if  (WString::contains(urlOriginal, "jma") && WString::endsWith(urlOriginal, "10.gif")) {
        auto url = WString::replace(urlOriginal, "10.gif", "");
        auto count = 10;
        vector<string> urls;
        for (auto index : range2(1, count + 1)) {
            urls.push_back(url + To::string(index) + ".gif");
        }
        return urls;
    } else {
        auto product = WString::split(urlOriginal, "/").back();
        product = WString::split(product, ".")[0];
        auto urlBase = WString::replace(urlOriginal, product + ".jpg", "");
        urlBase = WString::replace(urlBase, product + ".gif", "");
        const auto urlFile = urlBase + "txtfiles/" + product + "_names.txt";
        const auto html = UtilityIO::getHtml(urlFile);
        const auto lines = WString::split(html, GlobalVariables::newline);
        vector<string> urls;
        for (const auto& it : lines) {
            const auto tokens = WString::split(it, " ");
            urls.push_back(urlBase + tokens[0]);
        }
        return urls;
    }
}

bool UtilityGoesFullDisk::canAnimate(const string& url) {
    return
        !WString::contains(url, "GUAM") &&
        !WString::contains(url, "https://www.ospo.noaa.gov/Products/imagery/sohemi") &&
        !WString::contains(url, "https://www.ospo.noaa.gov/img/samoa_.PNG") &&
        !WString::contains(url, "https://cdn.star.nesdis.noaa.gov/GOES18/ABI/SECTOR/tsp/Sandwich/900x540.jpg");
}

const vector<string> UtilityGoesFullDisk::labels{
    "Eastern Atlantic Infrared",
    "Eastern Atlantic Infrared 2",
    "Eastern Atlantic Visible",
    "Eastern Atlantic Water Vapor",
    "Eastern Atlantic IR channel 4",
    "Eastern Atlantic AVN",
    "Eastern Atlantic Dvorak",
    "Eastern Atlantic JSL",
    "Eastern Atlantic RGB",
    "Eastern Atlantic Funktop",
    "Eastern Atlantic Rainbow",

    "Northeast Atlantic Infrared",
    "Northeast Atlantic Infrared 2",
    "Northeast Atlantic Visible",
    "Northeast Atlantic Water Vapor",
    "Northeast Atlantic IR channel 4",
    "Northeast Atlantic AVN",
    "Northeast Atlantic Dvorak",
    "Northeast Atlantic JSL",
    "Northeast Atlantic RGB",
    "Northeast Atlantic Funktop",
    "Northeast Atlantic Rainbow",

    "India Ocean Infrared",
    "India Ocean Infrared 2",
    "India Ocean Visible",
    "India Ocean Water Vapor",
    "India Ocean IR channel 4",
    "India Ocean AVN",
    "India Ocean Dvorak",
    "India Ocean JSL",
    "India Ocean RGB",
    "India Ocean Funktop",
    "India Ocean Rainbow",

    "Himawari Infrared",
    "Himawari IR, Ch. 4",
    "Himawari Water Vapor",
    "Himawari Water Vapor (Blue)",
    "Himawari Visible",
    "Himawari AVN Infrared",
    "Himawari Funktop Infrared",
    "Himawari RBTop Infrared, Ch. 4",

    "Himawari, West Central Pacific Infrared",
    "Himawari, West Central Pacific Visible",
    "Himawari, West Central Pacific Water Vapor",
    "Himawari, West Central Pacific Enhanced IR",

    "Himawari, Northwest Pacific Wide View Infrared",
    "Himawari, Northwest Pacific Wide View IR2",
    "Himawari, Northwest Pacific Wide View Visible",
    "Himawari, Northwest Pacific Wide View Water Vapor",

    "Himawari, West Pacific Infrared",
    "Himawari, West Pacific IR2",
    "Himawari, West Pacific Visible",
    "Himawari, West Pacific Water Vapor",

    "Himawari, Central Pacific IR2",
    "Himawari, Central Pacific Visible",
    "Himawari, Central Pacific Water Vapor",
    "Himawari, Central Pacific IR channel 4",
    "Himawari, Central Pacific AVN",
    "Himawari, Central Pacific Dvorak",
    "Himawari, Central Pacific JSL",
    "Himawari, Central Pacific RGB",
    "Himawari, Central Pacific Funktop",
    "Himawari, Central Pacific Rainbow",

    // # // "Himawari, Tropical West Pacific IR2",
    // # // "Himawari, Tropical West Pacific Visible",
    // # // "Himawari, Tropical West Pacific Water Vapor",
    "Himawari, Tropical West Pacific IR channel 4",
    "Himawari, Tropical West Pacific AVN",
    "Himawari, Tropical West Pacific Dvorak",
    "Himawari, Tropical West Pacific JSL",
    "Himawari, Tropical West Pacific RGB",
    "Himawari, Tropical West Pacific Funktop",
    "Himawari, Tropical West Pacific Rainbow",

    "Himawari, NorthWest Pacific IR channel 4",
    "Himawari, NorthWest Pacific AVN",
    "Himawari, NorthWest Pacific Dvorak",
    "Himawari, NorthWest Pacific JSL",
    "Himawari, NorthWest Pacific RGB",
    "Himawari, NorthWest Pacific Funktop",
    "Himawari, NorthWest Pacific Rainbow",

    "Himawari, Palau",
    "Himawari, Yap",
    "Himawari, Marianas",
    "Himawari, Chuuk",
    "Himawari, Pohnpei/Kosrae",
    "Himawari, Marshall Islands",
    "Himawari, Philippines",

    // # TODO FIXME https: // www.ospo.noaa.gov / products / imagery / sohemi.html

    "Himawari, Fiji",
    "Himawari, Tuvalu/Wallis and Futuna",
    "Himawari, Solomon Islands",
    "Himawari, Vanuatu and New Caledonia",
    "Himawari, Samoa",
    "Himawari, Tonga and Niue",
    "Himawari, New Guinea",
    "Himawari, New Zealand"
};

const vector<string> UtilityGoesFullDisk::urls{
    "https://www.ssd.noaa.gov/eumet/eatl/rb.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/ir2.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/vis.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/wv.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/ir4.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/avn.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/bd.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/jsl.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/rgb.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/ft.jpg",
    "https://www.ssd.noaa.gov/eumet/eatl/rb.jpg",

    "https://www.ssd.noaa.gov/eumet/neatl/rb.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/ir2.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/vis.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/wv.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/ir4.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/avn.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/bd.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/jsl.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/rgb.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/ft.jpg",
    "https://www.ssd.noaa.gov/eumet/neatl/rb.jpg",

    "https://www.ssd.noaa.gov/eumet/indiano/rb.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/ir2.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/vis.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/wv.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/ir4.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/avn.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/bd.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/jsl.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/rgb.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/ft.jpg",
    "https://www.ssd.noaa.gov/eumet/indiano/rb.jpg",

    "https://www.ospo.noaa.gov/dimg/jma/fd/rb/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/ir4/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/wv/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/wvblue/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/vis/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/avn/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/ft/10.gif",
    "https://www.ospo.noaa.gov/dimg/jma/fd/rbtop/10.gif",

    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMIR.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMVS.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMWV.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMCOL.JPG",

    "https://www.ssd.noaa.gov/jma/nwpac/rb.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/ir2.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/vis.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/wv.gif",

    "https://www.ssd.noaa.gov/jma/wpac/rb.gif",
    "https://www.ssd.noaa.gov/jma/wpac/ir2.gif",
    "https://www.ssd.noaa.gov/jma/wpac/vis.gif",
    "https://www.ssd.noaa.gov/jma/wpac/wv.gif",

    "https://www.ssd.noaa.gov/jma/wcpac/ir2.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/vis.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/wv.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/ir4.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/avn.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/bd.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/jsl.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/rgb.jpg",
    "https://www.ssd.noaa.gov/jma/wcpac/ft.gif",
    "https://www.ssd.noaa.gov/jma/wcpac/rb.gif",

    // # // "https://www.ssd.noaa.gov/jma/twpac/ir2.gif",
    // # // "https://www.ssd.noaa.gov/jma/twpac/vis.gif",
    // # // "https://www.ssd.noaa.gov/jma/twpac/wv.gif",
    "https://www.ssd.noaa.gov/jma/twpac/ir4.gif",
    "https://www.ssd.noaa.gov/jma/twpac/avn.gif",
    "https://www.ssd.noaa.gov/jma/twpac/bd.gif",
    "https://www.ssd.noaa.gov/jma/twpac/jsl.gif",
    "https://www.ssd.noaa.gov/jma/twpac/rgb.jpg",
    "https://www.ssd.noaa.gov/jma/twpac/ft.gif",
    "https://www.ssd.noaa.gov/jma/twpac/rb.gif",

    "https://www.ssd.noaa.gov/jma/nwpac/ir4.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/avn.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/bd.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/jsl.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/rgb.jpg",
    "https://www.ssd.noaa.gov/jma/nwpac/ft.gif",
    "https://www.ssd.noaa.gov/jma/nwpac/rb.gif",

    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMB.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMC.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMD.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAME.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMF.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/guam/GUAMG.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/PHILVIS.JPG",

    // # TODO FIXME https: // www.ospo.noaa.gov / products / imagery / sohemi.html

    "https://www.ospo.noaa.gov/Products/imagery/sohemi/SHGMSB.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/SHGMSC.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/SHGMSD.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/SHGMSF.JPG",
    "https://www.ospo.noaa.gov/img/samoa_.PNG",
    "https://cdn.star.nesdis.noaa.gov/GOES18/ABI/SECTOR/tsp/Sandwich/900x540.jpg",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/SHGMSG.JPG",
    "https://www.ospo.noaa.gov/Products/imagery/sohemi/NEWZVIS.JPG",
};
