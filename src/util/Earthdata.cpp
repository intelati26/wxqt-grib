// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/Earthdata.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include "objects/NetManager.h"
#include "util/Utility.h"

std::string Earthdata::token() {
    auto text = Utility::readPref("EARTHDATA_TOKEN", "");
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ')) {
        text.pop_back();
    }
    return text;
}

void Earthdata::setToken(const std::string& text) {
    Utility::writePref("EARTHDATA_TOKEN", text);
}

bool Earthdata::configured() {
    return !token().empty();
}

QByteArray Earthdata::authorizationFor(const std::string& url) {
    const auto host = QUrl{QString::fromStdString(url)}.host().toLower();
    if (host != "earthdata.nasa.gov" && !host.endsWith(".earthdata.nasa.gov")) {
        return {};
    }
    const auto text = token();
    return text.empty() ? QByteArray{} : "Bearer " + QByteArray::fromStdString(text);
}

std::string Earthdata::test() {
    if (!configured()) {
        return "no token is saved";
    }
    const auto list = NetManager::get("https://cmr.earthdata.nasa.gov/search/granules.json?collection_concept_id=C2075141605-POCLOUD&page_size=1&sort_key=-start_date");
    if (list.bytes.isEmpty()) {
        return "NASA's search (cmr.earthdata.nasa.gov) did not answer: no connection?";
    }
    std::string link;
    for (const auto entry : QJsonDocument::fromJson(list.bytes).object().value("feed").toObject().value("entry").toArray()) {
        for (const auto item : entry.toObject().value("links").toArray()) {
            const auto href = item.toObject().value("href").toString();
            if (href.startsWith("https://") && href.endsWith(".nc")) {
                link = href.toStdString();
            }
        }
    }
    if (link.empty()) {
        return "NASA's search found no ASCAT file to try";
    }
    const auto answer = NetManager::get(link, "bytes=0-3");
    if (answer.bytes.startsWith("CDF") || answer.bytes.startsWith("\x89HDF")) {
        return {};
    }
    return answer.status == 401 || answer.status == 403 || answer.status == 0 ? "NASA did not accept the token (HTTP " + std::to_string(answer.status) + "): it may have expired, or been copied incompletely"
                                                                              : "unexpected answer from NASA (HTTP " + std::to_string(answer.status) + ")";
}
