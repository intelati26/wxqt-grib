// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradDownload.h"
#include "common/GlobalDictionaries.h"
#include "common/GlobalVariables.h"
#include "objects/DownloadParallel.h"
#include "objects/WString.h"
#include "util/To.h"
#include "util/UtilityIO.h"
#include "util/UtilityList.h"
#include "util/UtilityString.h"

const string NexradDownload::pattern1{">(sn.[0-9]{4})</a>"};
const string NexradDownload::pattern2{".*?([0-9]{2}-[A-Za-z]{3}-[0-9]{4} [0-9]{2}:[0-9]{2}).*?"};

string NexradDownload::getRadarDirectoryUrl(const string& radarSite, const string& product) {
    const auto& productString = GlobalDictionaries::nexradProductString.at(product);
    return GlobalVariables::tgftpSitePrefix + "SL.us008001/DF.of/DC.radar/" + productString + "/SI." + WString::toLower(radarSite) + "/";
}

string NexradDownload::getRadarFileUrl(const string& radarSite, const string& product) {
    const auto& productString = GlobalDictionaries::nexradProductString.at(product);
    return GlobalVariables::tgftpSitePrefix + "SL.us008001/DF.of/DC.radar/" + productString + "/SI." + WString::toLower(radarSite) + "/sn.last";
}

void NexradDownload::getRadarFilesForAnimation(int frameCount, const string& product, const string& radarSite, FileStorage * fileStorage) {
    auto html = UtilityIO::getHtml(getRadarDirectoryUrl(radarSite, product));
    auto snFiles = UtilityString::parseColumn(html, pattern1);
    auto snDates = UtilityString::parseColumn(html, pattern2);
    if (snDates.empty()) {
        html = UtilityIO::getHtml(getRadarDirectoryUrl(radarSite, product));
        snFiles = UtilityString::parseColumn(html, pattern1);
        snDates = UtilityString::parseColumn(html, pattern2);
    }
    if (snDates.empty()) {
        return;
    }
    string mostRecentSn;
    const auto mostRecentTime = snDates.back();
    for (auto index : range(snDates.size() - 1)) {
        if (snDates[index] == mostRecentTime) {
            mostRecentSn = snFiles[index];
        }
    }
    const auto seq = To::Int(WString::replace(mostRecentSn, "sn.", ""));
    auto index = seq - frameCount + 1;
    vector<string> listOfFiles;
    for ([[maybe_unused]] auto unused : range(frameCount)) {
        auto tmpK = index;
        if (tmpK < 0) {
            tmpK += 251;
        }
        listOfFiles.push_back("sn." + To::stringPadLeftZeros(tmpK, 4));
        index += 1;
    }
    vector<string> urlList;
    for (auto i : range(frameCount)) {
        urlList.push_back(getRadarDirectoryUrl(radarSite, product) + listOfFiles[i]);
    }
    DownloadParallel{fileStorage, urlList};
}
