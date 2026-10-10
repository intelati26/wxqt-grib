// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/UtilityCams.h"
#include <algorithm>
#include <mutex>
#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QTimeZone>
#include "objects/URL.h"

namespace {
    const string site{"https://cams.nssl.noaa.gov/"};

    string query(const string& parameters) {
        return site + "query.php?" + parameters;
    }

    // How long a saved answer is used before the site is asked again (seconds). The slow-changing
    // answers - which models exist, what each one is, sector and category names - are kept on disk, so
    // opening the viewer does not ask for them every time: on networks where IPv6 to the host is broken
    // every new connection first waits for that attempt to time out, and the model list alone is one
    // request per model. Runs, products, the latest time and the images are always asked live.
    constexpr qint64 live = 0;
    constexpr qint64 sixHours = 6 * 3600;
    constexpr qint64 oneDay = 24 * 3600;
    constexpr qint64 oneWeek = 7 * 24 * 3600;

    QString cachePath(const string& url) {
        const auto dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/cams";
        QDir{}.mkpath(dir);
        const auto hash = QCryptographicHash::hash(QByteArray::fromStdString(url), QCryptographicHash::Sha1).toHex();
        return dir + "/" + QString::fromLatin1(hash) + ".json";
    }

    bool parse(const QByteArray& bytes, QJsonDocument& out, string& error) {
        QJsonParseError parseError;
        out = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            error = "cams.nssl.noaa.gov sent something unexpected: " + parseError.errorString().toStdString();
            return false;
        }
        return true;
    }

    // one JSON call; false (with a plain-language message) if the site cannot be reached or answers with
    // something that is not JSON. With maxAge > 0 a saved answer younger than that is used instead, a good
    // answer is saved, and a saved answer of any age is used when the site cannot be reached.
    bool fetchJson(const string& url, QJsonDocument& out, string& error, qint64 maxAge = live) {
        const auto path = maxAge > 0 ? cachePath(url) : QString{};
        QByteArray saved;
        if (maxAge > 0) {
            QFile file{path};
            if (file.open(QIODevice::ReadOnly)) {
                saved = file.readAll();
                const auto age = QFileInfo{path}.lastModified().secsTo(QDateTime::currentDateTime());
                string ignored;
                if (age >= 0 && age < maxAge && parse(saved, out, ignored)) {
                    return true;
                }
            }
        }
        const auto text = URL::getText(url);
        if (!text.empty() && parse(QByteArray::fromStdString(text), out, error)) {
            if (maxAge > 0) {
                QFile file{path};
                if (file.open(QIODevice::WriteOnly)) {
                    file.write(QByteArray::fromStdString(text));
                }
            }
            return true;
        }
        string ignored;
        if (!saved.isEmpty() && parse(saved, out, ignored)) {
            return true;   // offline or a bad answer: the last good one is better than nothing
        }
        if (text.empty()) {
            error = "no answer from cams.nssl.noaa.gov (offline, blocked, or the site is down)";
        }
        return false;
    }

    string text(const QJsonValue& value) {
        // product ids such as 500 are JSON numbers
        return value.isString() ? value.toString().toStdString()
            : value.isDouble() ? QString::number(value.toInt()).toStdString() : string{};
    }

    vector<string> strings(const QJsonValue& value) {
        vector<string> out;
        for (const auto& item : value.toArray()) {
            out.push_back(text(item));
        }
        return out;
    }

    vector<int> ints(const QJsonValue& value) {
        vector<int> out;
        for (const auto& item : value.toArray()) {
            out.push_back(item.toInt());
        }
        return out;
    }

    string hours3(int seconds) {
        const int hours = seconds / 3600;
        const int minutes = (seconds % 3600) / 60;
        return QString{"f%1%2"}.arg(hours, 3, 10, QChar{'0'}).arg(minutes, 2, 10, QChar{'0'}).toStdString();
    }

    std::mutex mapMutex;
    std::map<string, QByteArray> baseMaps;   // outline maps never change within a session

    QByteArray fetchLayer(const string& url, bool cacheable) {
        if (cacheable) {
            std::lock_guard<std::mutex> lock{mapMutex};
            const auto found = baseMaps.find(url);
            if (found != baseMaps.end()) {
                return found->second;
            }
        }
        int status = 0;
        auto bytes = URL::getBytesWithStatus(url, status);
        // the site answers a missing image with a 404 whose body is a "not available" PNG: require success
        if (status >= 200 && status < 300 && !QImage::fromData(bytes).isNull()) {
            if (cacheable) {
                std::lock_guard<std::mutex> lock{mapMutex};
                baseMaps[url] = bytes;
            }
            return bytes;
        }
        return {};   // an error page or nothing
    }
}

namespace UtilityCams {

bool models(vector<std::pair<string, string>>& out, string& error) {
    QJsonDocument document;
    if (!fetchJson(query("type=models"), document, error, sixHours)) {
        return false;
    }
    out.clear();
    // one small request per model for its display name - one at a time (parallel connections to this host
    // are what made discovery slow on networks with broken IPv6), and saved, so it is paid once a day
    for (const auto& id : strings(document.array())) {
        Model info;
        string ignored;
        const auto name = model(id, info, ignored) ? info.name : string{};
        out.emplace_back(id, name.empty() ? id : name);
    }
    if (out.empty()) {
        error = "cams.nssl.noaa.gov lists no models";
        return false;
    }
    return true;
}

bool model(const string& id, Model& out, string& error) {
    QJsonDocument document;
    if (!fetchJson(query("model=" + id + "&type=model"), document, error, sixHours)) {
        return false;
    }
    const auto object = document.object();
    out = Model{};
    out.id = id;
    out.name = text(object.value("name"));
    out.discontinued = object.value("discontinued").toBool();
    out.sectors = strings(object.value("sectors"));
    out.dailyRuns = strings(object.value("daily_runs"));
    const auto times = object.value("data_times");
    if (times.isArray()) {
        out.timesByRun[""] = ints(times);
    } else if (times.isObject()) {
        // hourly-cycling models list their forecast times per run hour
        const auto perRun = times.toObject();
        for (auto it = perRun.begin(); it != perRun.end(); ++it) {
            out.timesByRun[it.key().toStdString()] = ints(it.value());
        }
    }
    return true;
}

bool recentRuns(const string& modelId, size_t limit, vector<Run>& out, string& error) {
    QJsonDocument document;
    if (!fetchJson(query("model=" + modelId + "&type=runs"), document, error)) {
        return false;
    }
    out.clear();
    const auto object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        for (const auto& time : strings(it.value())) {
            out.push_back({it.key().toStdString(), time});
        }
    }
    std::sort(out.begin(), out.end(), [] (const Run& a, const Run& b) { return a.key() > b.key(); });
    if (out.size() > limit) {
        out.resize(limit);
    }
    if (out.empty()) {
        error = "no runs are listed for " + modelId;
        return false;
    }
    return true;
}

bool sectorNames(std::map<string, string>& out, string& error) {
    QJsonDocument document;
    if (!fetchJson(query("type=sectors"), document, error, oneWeek)) {
        return false;
    }
    out.clear();
    const auto object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        out[it.key().toStdString()] = text(it.value().toObject().value("name"));
    }
    return true;
}

bool catalog(const Model& modelInfo, const Run& run, const string& sector, Catalog& out, string& error) {
    out = Catalog{};
    QJsonDocument groups;
    string groupError;
    const bool haveGroups = fetchJson(query("type=deterministic_categories"), groups, groupError, oneDay);
    QJsonDocument document;
    const bool haveProducts = fetchJson(query("model=" + modelInfo.id + "&rd=" + run.date + "&rt=" + run.time + "&sector=" + sector +
                                              "&type=products&metadata=true"), document, error);
    if (haveGroups) {
        const auto top = groups.object();
        for (auto group = top.begin(); group != top.end(); ++group) {
            vector<string> ids;
            const auto members = group.value().toObject();
            for (auto member = members.begin(); member != members.end(); ++member) {
                ids.push_back(member.key().toStdString());
                out.categoryLabel[member.key().toStdString()] = text(member.value().toObject().value("label"));
            }
            out.groups.emplace_back(group.key().toStdString(), ids);
        }
    }
    // the category tree is a nicety; the products are what matter
    if (!haveProducts) {
        return false;
    }
    error.clear();
    const auto object = document.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
        const auto meta = it.value().toObject();
        Product product;
        product.id = it.key().toStdString();
        product.label = text(meta.value("label"));
        product.category = text(meta.value("category"));
        product.otherCategories = strings(meta.value("other_categories"));
        product.underlays = strings(meta.value("underlays").toObject().value("show"));
        product.overlays = strings(meta.value("overlays").toObject().value("show"));
        const auto lists = meta.value("plot_time_lists").toObject();
        if (lists.contains(QString::fromStdString(modelInfo.id))) {
            product.times = ints(lists.value(QString::fromStdString(modelInfo.id)));
        } else {
            const auto byRun = modelInfo.timesByRun.find(run.time);
            const auto fallback = modelInfo.timesByRun.find("");
            if (byRun != modelInfo.timesByRun.end()) {
                product.times = byRun->second;
            } else if (fallback != modelInfo.timesByRun.end()) {
                product.times = fallback->second;
            }
        }
        if (product.label.empty()) {
            product.label = product.id;
        }
        out.products.push_back(product);
    }
    std::sort(out.products.begin(), out.products.end(), [] (const Product& a, const Product& b) { return a.label < b.label; });
    if (out.products.empty()) {
        error = modelInfo.name + " offers no products for this run and sector";
        return false;
    }
    return true;
}

int latestAvailableSeconds(const string& modelId, const Run& run, const string& product, const string& sector) {
    QJsonDocument document;
    string ignored;
    if (!fetchJson(query("model=" + modelId + "&rd=" + run.date + "&rt=" + run.time + "&product=" + product +
                         "&sector=" + sector + "&type=latest"), document, ignored)) {
        return -1;
    }
    const auto value = document.object().value("model");
    return value.isDouble() ? value.toInt() : -1;
}

string imageUrl(const string& modelId, const Run& run, const string& layer, const string& sector, int seconds) {
    const auto frame = hours3(seconds);
    return site + "graphics/models/" + modelId + "/" + run.date.substr(0, 4) + "/" + run.date.substr(4, 2) + "/" +
        run.date.substr(6, 2) + "/" + run.time + "/" + frame + "/" + layer + "." + sector + "." + frame + ".png";
}

string baseMapUrl(const string& sector) {
    return site + "graphics/blank_maps/" + sector + ".png";
}

string layerSpec(const string& modelId, const Run& run, const Product& product, const string& sector, int seconds) {
    string spec;
    auto add = [&spec] (const string& url) { spec += (spec.empty() ? "" : "|") + url; };
    for (const auto& id : product.underlays) add(imageUrl(modelId, run, id, sector, seconds));
    add("*" + imageUrl(modelId, run, product.id, sector, seconds));   // "*" marks the one required layer
    for (const auto& id : product.overlays) add(imageUrl(modelId, run, id, sector, seconds));
    add(baseMapUrl(sector));
    return spec;
}

QByteArray composite(const string& spec, string& error) {
    const auto urls = QString::fromStdString(spec).split('|', Qt::SkipEmptyParts);
    if (urls.size() < 2) {
        error = "nothing to draw";
        return {};
    }
    QImage canvas;
    QPainter painter;
    bool haveProduct = false;
    string missingProduct;
    for (int i = 0; i < urls.size(); i += 1) {
        const bool isProduct = urls[i].startsWith('*');
        const bool isMap = i == urls.size() - 1;
        const auto url = (isProduct ? urls[i].mid(1) : urls[i]).toStdString();
        const auto layer = QImage::fromData(fetchLayer(url, isMap));
        if (layer.isNull()) {
            if (isProduct) {
                missingProduct = url;
            }
            continue;
        }
        if (canvas.isNull()) {
            canvas = QImage{layer.size(), QImage::Format_ARGB32};
            canvas.fill(Qt::white);
            painter.begin(&canvas);
        }
        painter.drawImage(canvas.rect(), layer);
        haveProduct = haveProduct || isProduct;
    }
    if (painter.isActive()) {
        painter.end();
    }
    if (!haveProduct) {
        error = "this image is not published (yet): " + QString::fromStdString(missingProduct).section('/', -1).toStdString();
        return {};
    }
    QByteArray png;
    QBuffer buffer{&png};
    buffer.open(QIODevice::WriteOnly);
    canvas.save(&buffer, "PNG");
    return png;
}

string frameLabel(const string& spec) {
    const auto match = QRegularExpression{R"(\.f(\d{3})(\d{2})\.png)"}.match(QString::fromStdString(spec));
    if (!match.hasMatch()) {
        return "";
    }
    return ("F" + QString::number(match.captured(1).toInt()).rightJustified(2, '0') +
            (match.captured(2) == "00" ? QString{} : ":" + match.captured(2))).toStdString();
}

string validLabel(const Run& run, int seconds) {
    const QDateTime start{QDate::fromString(QString::fromStdString(run.date), "yyyyMMdd"),
                          QTime{QString::fromStdString(run.time.substr(0, 2)).toInt(), QString::fromStdString(run.time.substr(2, 2)).toInt()},
                          QTimeZone::utc()};
    return start.addSecs(seconds).toString("ddd yyyy-MM-dd HH'Z'").toStdString();
}

}  // namespace UtilityCams
