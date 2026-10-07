// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/UtilityZip.h"
#include "util/UtilityGzip.h"

namespace {
    unsigned long le(const unsigned char * p, int n) {
        unsigned long value = 0;
        for (int i = n - 1; i >= 0; i--) {
            value = (value << 8) | p[i];
        }
        return value;
    }
}

bool UtilityZip::read(const std::string& zip, std::map<std::string, std::string>& files) {
    files.clear();
    const auto * p = reinterpret_cast<const unsigned char *>(zip.data());
    const size_t size = zip.size();
    if (size < 22) {
        return false;
    }
    // the end of central directory record: the last 22 bytes unless there is a comment, so search backwards
    size_t end = size - 22;
    while (le(p + end, 4) != 0x06054b50UL) {
        if (end == 0) {
            return false;
        }
        end--;
    }
    const size_t entries = le(p + end + 10, 2);
    size_t at = le(p + end + 16, 4);   // offset of the central directory
    for (size_t i = 0; i < entries; i++) {
        if (at + 46 > size || le(p + at, 4) != 0x02014b50UL) {
            return false;
        }
        const auto method = le(p + at + 10, 2);
        const size_t compressed = le(p + at + 20, 4);
        const size_t nameLength = le(p + at + 28, 2);
        const size_t extraLength = le(p + at + 30, 2);
        const size_t commentLength = le(p + at + 32, 2);
        const size_t local = le(p + at + 42, 4);
        if (at + 46 + nameLength > size || local + 30 > size || le(p + local, 4) != 0x04034b50UL) {
            return false;
        }
        const std::string name{reinterpret_cast<const char *>(p + at + 46), nameLength};
        // the local header repeats the name and extra field, with lengths of its own
        const size_t data = local + 30 + le(p + local + 26, 2) + le(p + local + 28, 2);
        at += 46 + nameLength + extraLength + commentLength;
        if (name.empty() || name.back() == '/') {
            continue;   // a folder
        }
        if (data + compressed > size) {
            return false;
        }
        std::string contents;
        if (method == 0) {
            contents.assign(reinterpret_cast<const char *>(p + data), compressed);
        } else if (method == 8) {
            if (!UtilityGzip::inflate(p + data, compressed, contents)) {
                return false;
            }
        } else {
            return false;
        }
        files[name] = std::move(contents);
    }
    return true;
}
