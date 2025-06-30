// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "DownloadText.h"
#include "common/GlobalVariables.h"
#include "misc/UtilityHourly.h"
#include "objects/WString.h"
#include "settings/Location.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityString.h"

bool DownloadText::useNwsApi{false};

string DownloadText::byProduct(const string& produ) {
    if (WString::startsWith(produ, "http")) {
        return UtilityIO::getHtml(produ);
    }
    string text;
    string no;
    string textUrl;
    const auto prod = WString::toUpper(produ);
    if (prod == "AFDLOC") {
        const auto tmp = Location::wfo();
        text = byProduct("afd" + WString::toLower(tmp));
    } else if (prod == "HWOLOC") {
        const auto tmp = Location::wfo();
        text = byProduct("hwo" + WString::toLower(tmp));
    } else if (prod == "WFO_TEXT") {
        text = byProduct("AFD" + Location::wfo());
    } else if (prod == "HOURLY") {
        text = UtilityHourly::get(static_cast<int>(Location::getCurrentLocation()));
    } else if (prod == "SWPC3DAY") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/3-day-forecast.txt");
    } else if (prod == "SWPC27DAY") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/27-day-outlook.txt");
    } else if (prod == "SWPCWWA") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/advisory-outlook.txt");
    } else if (prod == "SWPCHIGH") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/weekly.txt");
    } else if (prod == "SWPCDISC") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/discussion.txt");
    } else if (prod == "SWPC3DAYGEO") {
        text = UtilityIO::getHtml("https://services.swpc.noaa.gov/text/3-day-geomag-forecast.txt");
    } else if (WString::contains(prod, "MIAPWS") || WString::contains(prod, "MIAHS") || WString::contains(prod, "MIATCP") || WString::contains(prod, "MIATCM") || WString::contains(prod, "HFOTWOCP")) {
        textUrl = GlobalVariables::nwsNhcWebsitePrefix + "/text/" + prod + ".shtml";
        text = UtilityIO::getHtml(textUrl);
        text = UtilityString::extractPreLsr(text);
    } else if (WString::contains(prod, "MIAT")) {
        const auto url = GlobalVariables::nwsNhcWebsitePrefix + "/ftp/pub/forecasts/discussion/" + prod;
        text = UtilityIO::getHtml(url);
    } else if (WString::startsWith(prod, "SCCNS")) {
            auto textUrl1 = GlobalVariables::nwsWPCwebsitePrefix + "/discussions/nfd" + WString::replace(WString::toLower(prod), "ns", "") + ".html";
            text = UtilityIO::getHtml(textUrl1);
            text = UtilityString::extractPreLsr(text);
    } else if (WString::contains(prod, "SPCMCD")) {
        no = WString::replace(prod, "SPCMCD", "");
        textUrl = GlobalVariables::nwsSPCwebsitePrefix + "/products/md/md" + no + ".html";
        text = UtilityIO::getHtml(textUrl);
        text = UtilityString::parseNwsPre(text);
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "SPCWAT")) {
        no = WString::replace(prod, "SPCWAT", "");
        textUrl = GlobalVariables::nwsSPCwebsitePrefix + "/products/watch/ww" + no + ".html";
        text = UtilityIO::getHtml(textUrl);
        text = UtilityString::parseNwsPre(text);
        text = UtilityString::removeHtml(text);
        // text = UtilityString::parse(text, GlobalVariables::pre2Pattern);
    } else if (WString::contains(prod, "WPCMPD")) {
        no = WString::replace(prod, "WPCMPD", "");
        textUrl = GlobalVariables::nwsWPCwebsitePrefix + "/metwatch/metwatch_mpd_multi.php?md=" + no;
        text = UtilityIO::getHtml(textUrl);
        text = UtilityString::parseNwsPre(text);
    } else if (prod == "QPF94E") {
        text = string("https://www.wpc.Ncep.noaa.gov/qpf/ero.php?opt=curr&day=") + "1";
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (prod == "QPF98E") {
        text = string("https://www.wpc.Ncep.noaa.gov/qpf/ero.php?opt=curr&day=") + "2";
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (prod == "QPF99E") {
        text = string("https://www.wpc.Ncep.noaa.gov/qpf/ero.php?opt=curr&day=") + "3";
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "FWDDY1")) {
        text = GlobalVariables::nwsSPCwebsitePrefix + "/products/fire_wx/fwdy1.html";
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "FWDDY2")) {
        text = GlobalVariables::nwsSPCwebsitePrefix + "/products/fire_wx/fwdy2.html";
        //  text = url.getHtml();
        //  text = text.extractPre().removeLineBreaks().removeHtml().removeDuplicateSpaces();
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "FWDDY38")) {
        text = GlobalVariables::nwsSPCwebsitePrefix + "/products/exper/fire_wx/";
        text = UtilityIO::getHtml(text);
        text = UtilityString::extractPreLsr(text);
        text = UtilityString::removeHtml(text);
    } else if (WString::startsWith(prod, "GLF")) {
        auto product = prod.substr(0, 3);
        auto site = WString::replace(prod.substr(3), "%", "");
        auto url =
        "https://forecast.weather.gov/product.php?site=NWS&issuedby=" + site + "&product=" + product + "&format=txt&version=1&glossary=0";
        auto html = UtilityIO::getHtml(url);
        text = UtilityString::extractPreLsr(html);
    } else if (WString::contains(prod, "FOCN45")) {
        text = UtilityIO::getHtml(GlobalVariables::tgftpSitePrefix + "/data/raw/fo/focn45.cwwg..txt");
    } else if (WString::startsWith(prod, "VFD")) {
        auto t2 = UtilityString::substring(prod, 3);
        text = UtilityIO::getHtml(GlobalVariables::nwsAWCwebsitePrefix + "/fcstdisc/data?cwa=K" + t2);
        text = WString::replace(text, "\n", "<br>");
        text = UtilityString::parse(text, "<!-- raw data starts -->(.*?)<!-- raw data ends -->");
        text = WString::replace(text, "<br>", "\n");
    } else if (WString::startsWith(prod, "AWCN")) {
        text = UtilityIO::getHtml(GlobalVariables::tgftpSitePrefix + "data/raw/aw/" + WString::toLower(prod) + ".cwwg..txt");
    } else if (prod == "HSFSP") {
        text = UtilityIO::getHtml("https://tgftp.nws.noaa.gov/data/forecasts/marine/high_seas/south_hawaii.txt");
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "NFD")) {
        text = UtilityIO::getHtml(GlobalVariables::nwsOpcWebsitePrefix + "/mobile/mobile_product.php?id=" + WString::toUpper(prod));
        text = UtilityString::removeHtml(text);
    } else if (WString::contains(prod, "PMD30D")) {
        textUrl = GlobalVariables::tgftpSitePrefix + "/data/raw/fx/fxus07.kwbc.pmd.30d.txt";
        text = UtilityIO::getHtml(textUrl);
    } else if (WString::contains(prod, "PMD90D")) {
        textUrl = GlobalVariables::tgftpSitePrefix + "/data/raw/fx/fxus05.kwbc.pmd.90d.txt";
        text = UtilityIO::getHtml(textUrl);
    } else if (WString::contains(prod, "PMDMRD")) {
        textUrl = GlobalVariables::tgftpSitePrefix + "/data/raw/fx/fxus06.kwbc.pmd.mrd.txt";
        text = UtilityIO::getHtml(textUrl);
    } else if (WString::contains(prod, "PMDHCO")) {
        textUrl = GlobalVariables::tgftpSitePrefix + "/data/raw/fx/fxhw40.kwbc.pmd.hco.txt";
        text = UtilityIO::getHtml(textUrl);
    } else if (WString::contains(prod, "PMDTHR")) {
        auto url = GlobalVariables::tgftpSitePrefix + "/data/raw/fx/fxus21.kwnc.pmd.thr.txt";
        text = UtilityIO::getHtml(url);
    } else if (WString::startsWith(prod, "FTM")) {
        string radarSite;
        if (prod.size() == 6) {
            radarSite = UtilityString::substring(prod, 3);
        } else {
            radarSite = UtilityString::substring(prod, 4);
        }
        auto url = "https://forecast.weather.gov/product.php?site=NWS&product=FTM&issuedby=" + radarSite;
        text = UtilityIO::getHtml(url);
        text = UtilityString::extractPreLsr(text);
        text = WString::replace(text, "<br>", "\n");
    } else if (WString::contains(prod, "OFF") || prod == "UVICAC" || prod == "RWRMX" || WString::startsWith(prod, "TPT")) {
        auto product = UtilityString::substring(prod, 0, 3);
        auto site = UtilityString::substring(prod, 3);
        auto url = "https://forecast.weather.gov/product.php?site=NWS&issuedby=" + site + "&product=" + product + "&format=txt&version=1&glossary=0";
        auto html = UtilityIO::getHtml(url);
        text = UtilityString::extractPreLsr(html);
    } else {
        // exmaple URL https://api.weather.gov/products/types/AFD/locations/DTX
        // product
        const auto t1 = UtilityString::substring(prod, 0, 3);
        // site
        auto t2 = UtilityString::substring(prod, 3, 6);
        t2 = WString::replace(t2, "%", "");
        // Feb 8 2020 Sat
        // The NWS API for text products has been unstable Since Wed Feb 5
        // resorting to alternatives
        if (useNwsApi) {
            const auto urlToGet = GlobalVariables::nwsApiUrl + "/products/types/" + t1 + "/locations/" + t2;
            //  print(urlToGet);
            const auto htmlFuture = UtilityIO::getHtml(urlToGet);
            const auto urlProd = GlobalVariables::nwsApiUrl + string{"/products/"} + UtilityString::parse(htmlFuture, "\"id\": \"(.*?)\"");
            const auto prodHtmlFuture = UtilityIO::getHtml(urlProd);
            text = UtilityString::parse(prodHtmlFuture, "\"productText\": \"(.*?)\\$");
            if (!WString::startsWith(prod, "RTP")) {
                text = WString::replace(text, "\\n\\n", "\n");
                text = WString::replace(text, "\\n", " ");
            } else {
                text = WString::replace(text, "\\n", "\n");
            }
        } else {
            if (prod == "SWOMCD") {
                const string url = "https://forecast.weather.gov/product.php?site=NWS&issuedby=MCD&product=SWO&format=CI&version=1&glossary=1";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::extractPreLsr(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "SWODY1") {
                const string url = "https://www.spc.noaa.gov/products/outlook/day1otlk.html";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "SWODY2") {
                const string url = "https://www.spc.noaa.gov/products/outlook/day2otlk.html";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "SWODY3") {
                const string url = "https://www.spc.noaa.gov/products/outlook/day3otlk.html";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "SWOD48") {
                const string url = "https://www.spc.noaa.gov/products/exper/day4-8/";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "PMDSPD" || prod == "PMDEPD" || prod == "PMDHMD" || prod == "PMDHI" || prod == "PMDAK" || prod == "QPFERD" || prod == "QPFHSD") {
                const string url = "https://www.wpc.ncep.noaa.gov/discussions/hpcdiscussions.php?disc=" + WString::toLower(prod);
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                text = UtilityString::removeHtml(text);
                return text;
            } else if (prod == "QPFHSD") {
                const string url = "https://www.wpc.ncep.noaa.gov/discussions/hpcdiscussions.php?disc=fxsa20";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                return text;
            } else if (prod == "PMDCA") {
                const string url = "https://www.wpc.ncep.noaa.gov/discussions/hpcdiscussions.php?disc=fxca20";
                const auto html = UtilityIO::getHtml(url);
                text = UtilityString::parseNwsPre(html);
                return text;
            }
            const auto url = "https://forecast.weather.gov/product.php?site=" + t2 + "&issuedby=" + t2 + "&product=" + t1 + "&format=txt&version=1&glossary=0";
            const auto html = UtilityIO::getHtml(url);
            text = UtilityString::extractPreLsr(html);
        }
    }
    return text;
}

string DownloadText::getTextProductWithVersion(const string& product, int version) {
    const auto prodLocal = WString::toUpper(product);
    const auto t1 = UtilityString::substring(prodLocal, 0, 3);
    const auto t2 = UtilityString::substring(prodLocal, 3, 6);
    const auto textUrl = "https://forecast.weather.gov/product.php?site=NWS&product=" + t1 + "&issuedby=" + t2 + "&version=" + To::string(version);
    auto text = UtilityIO::getHtml(textUrl);
    text = UtilityString::extractPreLsr(text);
    text = WString::replace(text, "Graphics available at <a href=\"/basicwx/basicwxwbg.php\"><u>www.wpc.ncep.noaa.gov/basicwx/basicwxwbg.php</u></a>", "");
    return text;
}
