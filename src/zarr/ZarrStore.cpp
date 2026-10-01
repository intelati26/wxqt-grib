// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "zarr/ZarrStore.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <QJsonArray>
#include <QJsonDocument>
#include "objects/URL.h"
#include "zarr/Blosc.h"

namespace {
    size_t elementCount(const std::vector<int>& shape) {
        size_t n = 1;
        for (const int s : shape) {
            n *= static_cast<size_t>(std::max(s, 0));
        }
        return n;
    }

    std::string chunkKey(const std::vector<int>& index) {
        std::string key;
        for (size_t i = 0; i < index.size(); i += 1) {
            key += (i == 0 ? "" : ".") + std::to_string(index[i]);
        }
        return key.empty() ? std::string{"0"} : key;
    }
}

float ZarrStore::halfToFloat(unsigned short h) {
    const uint32_t sign = (h & 0x8000u) << 16;
    uint32_t exponent = (h >> 10) & 0x1Fu;
    uint32_t mantissa = h & 0x3FFu;
    uint32_t bits;
    if (exponent == 0) {
        if (mantissa == 0) {
            bits = sign;
        } else {   // subnormal: normalise
            exponent = 127 - 15 + 1;
            while ((mantissa & 0x400u) == 0) {
                mantissa <<= 1;
                exponent -= 1;
            }
            mantissa &= 0x3FFu;
            bits = sign | (exponent << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        bits = sign | 0x7F800000u | (mantissa << 13);
    } else {
        bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
    }
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}

bool ZarrStore::open(const std::string& baseUrl, std::string& error) {
    base = baseUrl;
    arrays.clear();
    int status = 0;
    const auto bytes = URL::getBytesWithStatus(base + "/.zmetadata", status);
    if (status != 200 || bytes.isEmpty()) {
        error = "no Zarr metadata at " + base + " (HTTP " + std::to_string(status) + ")";
        return false;
    }
    const auto doc = QJsonDocument::fromJson(bytes);
    const auto metadata = doc.object().value("metadata").toObject();
    if (metadata.isEmpty()) {
        error = "the Zarr metadata could not be read";
        return false;
    }
    groupAttrs = metadata.value(".zattrs").toObject();
    for (auto it = metadata.begin(); it != metadata.end(); ++it) {
        const auto key = it.key();
        const QString suffix{"/.zarray"};
        if (!key.endsWith(suffix)) {
            continue;
        }
        const auto name = key.left(key.size() - suffix.size());
        const auto z = it.value().toObject();
        Array a;
        for (const auto& v : z.value("shape").toArray()) {
            a.shape.push_back(v.toInt());
        }
        for (const auto& v : z.value("chunks").toArray()) {
            a.chunks.push_back(v.toInt());
        }
        a.dtype = z.value("dtype").toString().toStdString();
        const auto fill = z.value("fill_value");
        if (fill.isDouble()) {
            a.fillValue = fill.toDouble();
            a.hasFill = true;
        } else if (fill.isString() && fill.toString() == "NaN") {
            a.fillValue = std::numeric_limits<double>::quiet_NaN();
            a.hasFill = true;
        }
        const auto compressor = z.value("compressor");
        a.compressed = compressor.isObject() && compressor.toObject().value("id").toString() == "blosc";
        if (compressor.isObject() && !a.compressed) {
            error = "array " + name.toStdString() + " uses an unsupported compressor";
            return false;
        }
        a.attrs = metadata.value(name + "/.zattrs").toObject();
        for (const auto& v : a.attrs.value("_ARRAY_DIMENSIONS").toArray()) {
            a.dims.push_back(v.toString().toStdString());
        }
        arrays.emplace(name.toStdString(), std::move(a));
    }
    return true;
}

const ZarrStore::Array * ZarrStore::array(const std::string& name) const {
    const auto it = arrays.find(name);
    return it == arrays.end() ? nullptr : &it->second;
}

std::vector<std::string> ZarrStore::arrayNames() const {
    std::vector<std::string> names;
    for (const auto& [name, a] : arrays) {
        names.push_back(name);
    }
    return names;
}

bool ZarrStore::fetchDecoded(const std::string& name, const std::vector<int>& chunkIndex, const Array& a, std::vector<unsigned char>& bytes,
                             bool& missing, std::string& error) const {
    missing = false;
    int status = 0;
    QByteArray raw;
    for (int attempt = 0; attempt < 2; attempt += 1) {   // one retry: a busy server drops the odd request
        raw = URL::getBytesWithStatus(base + "/" + name + "/" + chunkKey(chunkIndex), status);
        if (status == 200 || status == 404 || status == 403) {
            break;
        }
    }
    if (status == 404 || status == 403) {   // a chunk that was never written
        missing = true;
        return true;
    }
    if (status != 200 || raw.isEmpty()) {
        error = "chunk " + name + "/" + chunkKey(chunkIndex) + " could not be downloaded (HTTP " + std::to_string(status) + ")";
        return false;
    }
    if (!a.compressed) {
        bytes.assign(raw.begin(), raw.end());
        return true;
    }
    return Blosc::decompress(reinterpret_cast<const unsigned char *>(raw.constData()), static_cast<size_t>(raw.size()), bytes, error);
}

bool ZarrStore::readChunk(const std::string& name, const std::vector<int>& chunkIndex, std::vector<float>& out, std::string& error) const {
    const auto * a = array(name);
    if (a == nullptr) {
        error = "no array " + name;
        return false;
    }
    std::vector<unsigned char> bytes;
    bool missing = false;
    if (!fetchDecoded(name, chunkIndex, *a, bytes, missing, error)) {
        return false;
    }
    const size_t count = elementCount(a->chunks);
    out.assign(count, a->hasFill ? static_cast<float>(a->fillValue) : 0.0f);
    if (missing) {
        return true;
    }
    const auto& t = a->dtype;
    size_t width = 0;
    if (t.size() >= 3) {
        width = static_cast<size_t>(std::stoi(t.substr(2)));
    }
    if (width == 0 || bytes.size() < count * width) {
        error = "chunk " + name + " has " + std::to_string(bytes.size()) + " bytes, expected " + std::to_string(count * width);
        return false;
    }
    const unsigned char * p = bytes.data();
    const char kind = t[1];
    for (size_t i = 0; i < count; i += 1, p += width) {
        float v = 0.0f;
        if (kind == 'f' && width == 2) {
            uint16_t h;
            std::memcpy(&h, p, 2);
            v = halfToFloat(h);
        } else if (kind == 'f' && width == 4) {
            std::memcpy(&v, p, 4);
        } else if (kind == 'f' && width == 8) {
            double d;
            std::memcpy(&d, p, 8);
            v = static_cast<float>(d);
        } else if (kind == 'i' && width == 4) {
            int32_t n;
            std::memcpy(&n, p, 4);
            v = static_cast<float>(n);
        } else if (kind == 'i' && width == 8) {
            int64_t n;
            std::memcpy(&n, p, 8);
            v = static_cast<float>(n);
        } else if (kind == 'u' && width == 1) {
            v = static_cast<float>(*p);
        } else if (kind == 'i' && width == 1) {
            v = static_cast<float>(static_cast<int8_t>(*p));
        } else if (kind == 'u' && width == 2) {
            uint16_t n;
            std::memcpy(&n, p, 2);
            v = static_cast<float>(n);
        } else if (kind == 'i' && width == 2) {
            int16_t n;
            std::memcpy(&n, p, 2);
            v = static_cast<float>(n);
        } else {
            error = "unsupported data type " + t + " in " + name;
            return false;
        }
        out[i] = v;
    }
    return true;
}

bool ZarrStore::readNumbers(const std::string& name, std::vector<double>& out, std::string& error) const {
    const auto * a = array(name);
    if (a == nullptr || a->shape.size() != 1) {
        error = "no 1-D array " + name;
        return false;
    }
    out.clear();
    const int chunk = std::max(1, a->chunks[0]);
    for (int start = 0, index = 0; start < a->shape[0]; start += chunk, index += 1) {
        std::vector<unsigned char> bytes;
        bool missing = false;
        if (!fetchDecoded(name, {index}, *a, bytes, missing, error)) {
            return false;
        }
        const size_t take = static_cast<size_t>(std::min(chunk, a->shape[0] - start));
        for (size_t i = 0; i < take; i += 1) {
            double v = std::numeric_limits<double>::quiet_NaN();
            if (!missing) {
                if (a->dtype == "<f8" && bytes.size() >= (i + 1) * 8) {
                    std::memcpy(&v, bytes.data() + i * 8, 8);
                } else if (a->dtype == "<i8" && bytes.size() >= (i + 1) * 8) {
                    int64_t n;
                    std::memcpy(&n, bytes.data() + i * 8, 8);
                    v = static_cast<double>(n);
                } else if (a->dtype == "<f4" && bytes.size() >= (i + 1) * 4) {
                    float f;
                    std::memcpy(&f, bytes.data() + i * 4, 4);
                    v = f;
                }
            }
            out.push_back(v);
        }
    }
    return true;
}

bool ZarrStore::readStrings(const std::string& name, std::vector<std::string>& out, std::string& error) const {
    const auto * a = array(name);
    if (a == nullptr || a->shape.size() != 1 || a->dtype.size() < 3 || a->dtype[1] != 'U') {
        error = "no string array " + name;
        return false;
    }
    const size_t chars = static_cast<size_t>(std::stoi(a->dtype.substr(2)));
    out.clear();
    std::vector<unsigned char> bytes;
    bool missing = false;
    if (!fetchDecoded(name, {0}, *a, bytes, missing, error)) {
        return false;
    }
    for (int i = 0; i < a->shape[0] && !missing; i += 1) {
        std::string text;
        for (size_t c = 0; c < chars; c += 1) {
            const size_t at = (static_cast<size_t>(i) * chars + c) * 4;
            if (at + 4 > bytes.size()) {
                break;
            }
            uint32_t code;
            std::memcpy(&code, bytes.data() + at, 4);
            if (code == 0) {
                break;
            }
            text += code < 128 ? static_cast<char>(code) : '?';
        }
        out.push_back(text);
    }
    return true;
}
