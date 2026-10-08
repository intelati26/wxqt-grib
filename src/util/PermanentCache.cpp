// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/PermanentCache.h"
#include <algorithm>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

namespace {
    QString& rootOverride() {
        static QString root;
        return root;
    }

    bool store(const QString& file, const std::string& text) {
        QDir{}.mkpath(QFileInfo{file}.absolutePath());
        QFile out{file + ".tmp"};
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        const auto packed = qCompress(QByteArray::fromRawData(text.data(), static_cast<qsizetype>(text.size())), 9);
        const bool ok = out.write(packed) == packed.size();
        out.close();
        if (!ok) {
            QFile::remove(file + ".tmp");
            return false;
        }
        QFile::remove(file);
        return QFile::rename(file + ".tmp", file);
    }
}

PermanentCache::PermanentCache(const QString& module, const QString& legacyFolder) : module{module}, legacy{legacyFolder} {}

void PermanentCache::setRoot(const QString& root) {
    rootOverride() = root;
}

QString PermanentCache::folder() const {
    const auto base = rootOverride().isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : rootOverride();
    return base + "/data/" + module;
}

QString PermanentCache::path(const std::string& name) const {
    return folder() + "/" + QString::fromStdString(name) + ".z";
}

bool PermanentCache::has(const std::string& name) const {
    return !name.empty() && (QFileInfo::exists(path(name)) || (!legacy.isEmpty() && !read(name).empty()));
}

std::string PermanentCache::read(const std::string& name) const {
    if (name.empty()) {
        return {};
    }
    QFile in{path(name)};
    if (in.open(QIODevice::ReadOnly)) {
        const auto text = qUncompress(in.readAll());
        return std::string{text.constData(), static_cast<size_t>(text.size())};
    }
    // a file from before this cache existed: take it over
    if (!legacy.isEmpty()) {
        const auto old = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + legacy + "/" + QString::fromStdString(name);
        QFile raw{old};
        if (raw.open(QIODevice::ReadOnly)) {
            const auto bytes = raw.readAll();
            raw.close();
            if (!bytes.isEmpty()) {
                const std::string text{bytes.constData(), static_cast<size_t>(bytes.size())};
                if (store(path(name), text)) {
                    QFile::remove(old);
                }
                return text;
            }
        }
    }
    return {};
}

bool PermanentCache::write(const std::string& name, const std::string& text) const {
    return !name.empty() && !text.empty() && store(path(name), text);
}

std::string PermanentCache::newestName(const std::string& glob) const {
    QStringList found = QDir{folder()}.entryList({QString::fromStdString(glob) + ".z"}, QDir::Files, QDir::Name | QDir::Reversed);
    if (!legacy.isEmpty()) {
        const auto old = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/" + legacy;
        for (const auto& name : QDir{old}.entryList({QString::fromStdString(glob)}, QDir::Files)) {
            found.push_back(name + ".z");
        }
        std::sort(found.begin(), found.end(), std::greater<>{});
    }
    return found.isEmpty() ? std::string{} : found.first().chopped(2).toStdString();
}

void PermanentCache::prune(const std::string& glob, const std::string& keep) const {
    const auto keepFile = QString::fromStdString(keep) + ".z";
    for (const auto& old : QDir{folder()}.entryList({QString::fromStdString(glob) + ".z"}, QDir::Files)) {
        if (old != keepFile) {
            QFile::remove(folder() + "/" + old);
        }
    }
}

PermanentCache::Got PermanentCache::fetch(const std::string& name, const std::string& glob, size_t minBytes, const std::function<std::string()>& download) const {
    Got got;
    if (!name.empty()) {
        got.text = read(name);
        if (!got.text.empty()) {
            got.name = name;
            return got;
        }
        auto fresh = download();
        if (fresh.size() >= minBytes && !fresh.empty()) {
            write(name, fresh);
            prune(glob, name);
            got.text = std::move(fresh);
            got.name = name;
            got.downloaded = true;
            return got;
        }
    }
    // no network, or no listing: the newest one kept
    const auto newest = newestName(glob);
    got.text = read(newest);
    got.name = newest;
    got.stale = !got.text.empty();
    return got;
}
