// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "NexradDownload.h"
#include <unordered_map>
#include <QDateTime>
#include <QRegularExpression>
#include <QString>
#include "common/GlobalDictionaries.h"
#include "common/GlobalVariables.h"
#include "objects/DownloadParallel.h"
#include "objects/WString.h"
#include "settings/RadarPreferences.h"
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
    if (RadarPreferences::useS3) {
        // the bucket lists the real files with their times: no guessing at sequence numbers
        const auto urls = s3RecentUrls(radarSite, product, frameCount);
        if (urls.size() >= 2) {
            DownloadParallel{fileStorage, urls};
            return;
        }
    }
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

namespace {
    const string s3Base{"https://unidata-nexrad-level3.s3.amazonaws.com"};

    // the keys of a radar's product from the last `hours` hours, oldest first (S3 lists keys in name order, which is time order)
    vector<string> s3Keys(const string& site, const string& product, int hours) {
        const auto prefix = site + "_" + product + "_";
        const auto start = QDateTime::currentDateTimeUtc().addSecs(-3600LL * hours);
        const auto url = s3Base + "/?list-type=2&prefix=" + prefix + "&start-after=" + prefix + start.toString("yyyy_MM_dd_HH_mm_ss").toStdString() +
                         "&max-keys=1000";
        const auto xml = QString::fromStdString(UtilityIO::getHtml(url));
        vector<string> keys;
        static const QRegularExpression key{"<Key>([^<]+)</Key>"};
        for (auto it = key.globalMatch(xml); it.hasNext();) {
            keys.push_back(it.next().captured(1).toStdString());
        }
        return keys;
    }
}

string NexradDownload::s3Site(const string& radarSite) {
    const auto site = WString::toUpper(radarSite);
    return site.size() == 4 ? site.substr(1) : site;
}

string NexradDownload::s3Product(const string& product) {
    // the bucket's reflectivity and velocity are the super-resolution products (0.25 km by 0.5 degrees)
    static const std::unordered_map<string, string> superRes{{"N0Q", "N0B"}, {"N1Q", "N1B"}, {"N2Q", "N2B"}, {"N3Q", "N3B"},
                                                             {"N0U", "N0G"}, {"N1U", "N1G"}};
    const auto found = superRes.find(product);
    return found == superRes.end() ? product : found->second;
}

vector<string> NexradDownload::s3RecentUrls(const string& radarSite, const string& product, int count) {
    auto keys = s3Keys(s3Site(radarSite), s3Product(product), 8);
    if (static_cast<int>(keys.size()) > count) {
        keys.erase(keys.begin(), keys.end() - count);
    }
    vector<string> urls;
    for (const auto& key : keys) {
        urls.push_back(s3Base + "/" + key);
    }
    return urls;
}

string NexradDownload::latestFileUrl(const string& radarSite, const string& product) {
    if (RadarPreferences::useS3) {
        const auto urls = s3RecentUrls(radarSite, product, 1);
        if (!urls.empty()) {
            return urls.back();
        }
    }
    return getRadarFileUrl(radarSite, product);   // the NWS server: a product or radar the bucket does not carry, or it is unreachable
}
