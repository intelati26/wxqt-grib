// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/TarReader.h"
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <QFile>
#include <QSaveFile>
#include <QTextStream>
#include <algorithm>
#include "objects/URL.h"
#include "util/UtilityGzip.h"

namespace {
    // the most a file of a tar may be (an hour of a model is some hundreds of megabytes at the most): a header that says more is not believed
    const long long maxMember = 1536ll * 1024 * 1024;

    struct Member {
        long long offset{0};   // of the file's bytes in the tar
        long long size{0};
    };

    // one download or unpacking of a file at a time: the charts of a screen ask for many fields of the same hour at once
    std::mutex& lockFor(const QString& key) {
        static std::mutex guard;
        static std::map<QString, std::unique_ptr<std::mutex>> locks;
        const std::lock_guard lock{guard};
        auto& slot = locks[key];
        if (!slot) {
            slot = std::make_unique<std::mutex>();
        }
        return *slot;
    }

    // The tar's entries ("name offset size next" a line) up to and including the one wanted: the headers are walked by range, a file at a time, and what was seen is kept for the next hour.
    bool findMember(const std::string& url, const QString& indexPath, const std::string& name, Member& out, std::string& error) {
        std::map<std::string, Member> seen;
        long long next = 0;
        QFile file{indexPath};
        if (file.open(QIODevice::ReadOnly)) {
            QTextStream in{&file};
            while (!in.atEnd()) {
                const auto parts = in.readLine().split(' ');
                if (parts.size() == 4) {
                    seen[parts[0].toStdString()] = {parts[1].toLongLong(), parts[2].toLongLong()};
                    next = parts[3].toLongLong();
                }
            }
            file.close();
        }
        const auto found = [&] {
            const auto it = seen.find(name);
            if (it == seen.end()) {
                return false;
            }
            out = it->second;
            return true;
        };
        if (found()) {
            return true;
        }
        QByteArray add;
        bool done = false;
        for (int guard = 0; guard < 400 && !done; guard++) {
            const auto header = URL::getBytesRange(url, next, next + 511);
            if (header.size() < 512 || header[0] == '\0') {
                break;
            }
            const std::string entry{header.constData(), strnlen(header.constData(), 100)};
            const long long size = QByteArray{header.constData() + 124, 12}.trimmed().toLongLong(nullptr, 8);
            if (size < 0 || size > maxMember) {   // a header that is not one (or a tar that is not ours): stop, do not follow it
                break;
            }
            const long long data = next + 512;
            next = data + (size + 511) / 512 * 512;
            // (a name goes into the index file, a line of words: one with a space or a control character would be read back as something else)
            const bool plainName = std::none_of(entry.begin(), entry.end(), [] (unsigned char c) { return c <= 0x20 || c == 0x7f; });
            if (!plainName) {
                continue;
            }
            if (size > 0) {
                const auto slash = entry.rfind('/');
                const auto base = slash == std::string::npos ? entry : entry.substr(slash + 1);
                seen[base] = {data, size};
                add += QByteArray::fromStdString(base) + " " + QByteArray::number(data) + " " + QByteArray::number(size) + " " + QByteArray::number(next) + "\n";
                done = base == name;
            } else {
                add += QByteArray::fromStdString(entry) + " 0 0 " + QByteArray::number(next) + "\n";
            }
        }
        if (!add.isEmpty()) {
            QFile keep{indexPath};
            if (keep.open(QIODevice::Append)) {
                keep.write(add);
            }
        }
        if (!found()) {
            error = "the tar does not have " + name + " (yet)";
            return false;
        }
        return true;
    }
}

bool TarReader::extract(const std::string& url, const std::string& name, const QString& indexPath, const QString& path, const std::string& expect, std::string& error) {
    std::lock_guard lock{lockFor(path)};
    if (QFile::exists(path)) {
        return true;
    }
    Member member;
    if (!findMember(url, indexPath, name, member, error)) {
        return false;
    }
    if (member.offset < 0 || member.size <= 0 || member.size > maxMember) {   // (also what an index file kept from before says)
        error = name + " has a size that can not be right";
        return false;
    }
    const auto packed = URL::getBytesRange(url, member.offset, member.offset + member.size - 1);
    if (packed.size() != member.size) {
        error = "could not download " + name;
        return false;
    }
    std::string plain;
    if (name.size() > 3 && name.compare(name.size() - 3, 3, ".gz") == 0) {
        if (!UtilityGzip::gunzip(std::string{packed.constData(), static_cast<size_t>(packed.size())}, plain)) {
            error = "could not unpack " + name;
            return false;
        }
    } else {
        plain.assign(packed.constData(), static_cast<size_t>(packed.size()));
    }
    if (!expect.empty() && plain.compare(0, expect.size(), expect) != 0) {
        error = name + " is not what it should be (" + expect + ")";
        return false;
    }
    QSaveFile save{path};
    if (!save.open(QIODevice::WriteOnly) || save.write(plain.data(), static_cast<qint64>(plain.size())) != static_cast<qint64>(plain.size()) || !save.commit()) {
        error = "could not write " + path.toStdString();
        return false;
    }
    return true;
}
