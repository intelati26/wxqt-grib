// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "obs/UtilityMadis.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

namespace {
    // NetCDF classic (CDF-1, or CDF-2 with 64-bit offsets): the header, then the values of the variables without the record dimension, then
    // the records one after another, each holding a slab of every record variable (each padded to four bytes).
    struct Cursor {
        const string& data;
        size_t pos{0};
        bool short_{false};
        uint32_t u32() {
            if (pos + 4 > data.size()) {
                short_ = true;
                return 0;
            }
            const auto v = UtilityMadis::bigInt(data.data() + pos);
            pos += 4;
            return v;
        }
        uint64_t u64() {
            const uint64_t high = u32();
            return (high << 32) | u32();
        }
        string name() {
            const uint32_t n = u32();
            if (short_ || pos + n > data.size()) {
                short_ = true;
                return {};
            }
            string s = data.substr(pos, n);
            pos += (n + 3u) & ~3u;
            return s;
        }
        void skip(uint64_t n) {
            if (pos + n > data.size()) {
                short_ = true;
            } else {
                pos += n;
            }
        }
    };

    int typeSize(int type) {
        static const int sizes[] = {0, 1, 1, 2, 4, 4, 8};
        return type >= 1 && type <= 6 ? sizes[type] : 0;
    }

    void skipAttributes(Cursor& c) {
        const uint32_t tag = c.u32();
        const uint32_t n = c.u32();
        if (tag == 0 && n == 0) {
            return;
        }
        for (uint32_t i = 0; i < n && !c.short_; i++) {
            c.name();
            const int type = static_cast<int>(c.u32());
            const uint32_t count = c.u32();
            c.skip((static_cast<uint64_t>(typeSize(type)) * count + 3u) & ~3ull);
        }
    }

    string text(const char * p, int length) {   // a fixed-width, zero- or space-padded character field
        string s{p, static_cast<size_t>(length)};
        const auto end = s.find('\0');
        if (end != string::npos) {
            s.resize(end);
        }
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
            s.pop_back();
        }
        size_t first = 0;
        while (first < s.size() && s[first] == ' ') {
            first++;
        }
        return s.substr(first);
    }

    double mercatorOf(double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    }
}

float UtilityMadis::bigFloat(const char * p) {
    const uint32_t v = bigInt(p);
    float f;
    std::memcpy(&f, &v, 4);
    return f;
}

double UtilityMadis::bigDouble(const char * p) {
    uint64_t v = (static_cast<uint64_t>(bigInt(p)) << 32) | bigInt(p + 4);
    double d;
    std::memcpy(&d, &v, 8);
    return d;
}

uint32_t UtilityMadis::bigInt(const char * p) {
    const auto * b = reinterpret_cast<const unsigned char *>(p);
    return (static_cast<uint32_t>(b[0]) << 24) | (static_cast<uint32_t>(b[1]) << 16) | (static_cast<uint32_t>(b[2]) << 8) | b[3];
}

bool UtilityMadis::MesonetReader::parseHeader() {
    if (buffer.size() < 12) {
        return true;   // wait for more
    }
    if (buffer.compare(0, 3, "CDF") != 0 || (buffer[3] != 1 && buffer[3] != 2)) {
        return false;
    }
    const bool wide = buffer[3] == 2;
    Cursor c{buffer};
    c.pos = 4;
    numrecs = c.u32();
    // dimensions
    const uint32_t dimTag = c.u32();
    const uint32_t dimCount = c.u32();
    vector<uint64_t> dimLength;
    vector<bool> dimRecord;
    if (dimTag == 0xA) {
        for (uint32_t i = 0; i < dimCount && !c.short_; i++) {
            c.name();
            const uint32_t length = c.u32();
            dimLength.push_back(length);
            dimRecord.push_back(length == 0);
        }
    }
    skipAttributes(c);
    const uint32_t varTag = c.u32();
    const uint32_t varCount = c.u32();
    vector<Var> found;
    if (varTag == 0xB) {
        for (uint32_t i = 0; i < varCount && !c.short_; i++) {
            Var v;
            v.name = c.name();
            const uint32_t nd = c.u32();
            for (uint32_t d = 0; d < nd && !c.short_; d++) {
                const uint32_t id = c.u32();
                if (id >= dimLength.size()) {
                    return false;
                }
                v.dims.push_back(static_cast<int>(id));
            }
            skipAttributes(c);
            v.type = static_cast<int>(c.u32());
            v.size = c.u32();
            v.begin = wide ? c.u64() : c.u32();
            v.record = !v.dims.empty() && dimRecord[static_cast<size_t>(v.dims[0])];
            if (v.record) {   // the slab of one record: the product of the other dimensions
                uint64_t n = 1;
                for (size_t d = 1; d < v.dims.size(); d++) {
                    n *= dimLength[static_cast<size_t>(v.dims[d])];
                }
                v.size = (n * static_cast<uint64_t>(typeSize(v.type)) + 3u) & ~3ull;
            }
            found.push_back(std::move(v));
        }
    }
    if (c.short_) {
        return true;   // the header is not all here yet
    }
    vars = std::move(found);
    firstRecord = ~0ull;
    recordSize = 0;
    for (const auto& v : vars) {
        if (v.record) {
            firstRecord = std::min(firstRecord, v.begin);
            recordSize += v.size;
        }
    }
    if (firstRecord == ~0ull || recordSize == 0) {
        return false;
    }
    const auto offsetOf = [this] (const char * name) {
        const auto * v = find(name);
        return v == nullptr || !v->record ? -1 : static_cast<int>(v->begin - firstRecord);
    };
    at.providerId = offsetOf("providerId");
    at.stationId = offsetOf("stationId");
    at.stationName = offsetOf("stationName");
    at.dataProvider = offsetOf("dataProvider");
    at.latitude = offsetOf("latitude");
    at.longitude = offsetOf("longitude");
    at.elevation = offsetOf("elevation");
    at.observationTime = offsetOf("observationTime");
    at.temperature = offsetOf("temperature");
    at.temperatureDD = offsetOf("temperatureDD");
    at.dewpoint = offsetOf("dewpoint");
    at.relHumidity = offsetOf("relHumidity");
    at.seaLevelPressure = offsetOf("seaLevelPressure");
    at.altimeter = offsetOf("altimeter");
    at.windDir = offsetOf("windDir");
    at.windSpeed = offsetOf("windSpeed");
    at.windGust = offsetOf("windGust");
    at.visibility = offsetOf("visibility");
    // the character fields' widths are their last dimension
    const auto width = [this, &dimLength] (const char * name) {
        const auto * v = find(name);
        return v == nullptr || v->dims.size() < 2 ? 0 : static_cast<int>(dimLength[static_cast<size_t>(v->dims.back())]);
    };
    at.providerIdLength = width("providerId");
    at.stationIdLength = width("stationId");
    at.stationNameLength = width("stationName");
    at.dataProviderLength = width("dataProvider");
    if (at.latitude < 0 || at.longitude < 0 || at.observationTime < 0 || at.stationId < 0) {
        return false;
    }
    haveHeader = true;
    return true;
}

const UtilityMadis::MesonetReader::Var * UtilityMadis::MesonetReader::find(const char * name) const {
    for (const auto& v : vars) {
        if (v.name == name) {
            return &v;
        }
    }
    return nullptr;
}

bool UtilityMadis::MesonetReader::push(const char * data, size_t size) {
    if (bad) {
        return false;
    }
    buffer.append(data, size);
    if (!haveHeader) {
        if (!parseHeader()) {
            bad = true;
            return false;
        }
        if (!haveHeader) {
            return true;
        }
    }
    // up to the first record: the values of the variables without the record dimension
    if (bufferStart < firstRecord) {
        const uint64_t skip = std::min<uint64_t>(firstRecord - bufferStart, buffer.size());
        buffer.erase(0, static_cast<size_t>(skip));
        bufferStart += skip;
        if (bufferStart < firstRecord) {
            return true;
        }
    }
    size_t used = 0;
    while (buffer.size() - used >= recordSize && (numrecs == 0xffffffffull || count < numrecs)) {
        decode(buffer.data() + used);
        used += static_cast<size_t>(recordSize);
        count++;
    }
    buffer.erase(0, used);
    bufferStart += used;
    return true;
}

void UtilityMadis::MesonetReader::decode(const char * r) {
    const double lat = bigFloat(r + at.latitude);
    const double lon = bigFloat(r + at.longitude);
    const double time = bigDouble(r + at.observationTime);
    if (!valid(lat) || !valid(lon) || !valid(time) || std::abs(lat) > 89.0 || std::abs(lon) > 180.0 || time <= 0.0) {
        return;
    }
    SurfaceStation s;
    s.id = text(r + at.stationId, at.stationIdLength);
    string key = at.providerId >= 0 ? text(r + at.providerId, at.providerIdLength) : s.id;
    if (key.empty()) {
        key = s.id;
    }
    if (s.id.empty() || key.empty()) {
        return;
    }
    s.seconds = static_cast<long>(time);
    const auto found = latest.find(key);
    if (found != latest.end() && found->second.seconds >= s.seconds) {
        return;
    }
    if (at.stationName >= 0) {
        s.name = text(r + at.stationName, at.stationNameLength);
    }
    if (at.dataProvider >= 0) {
        s.network = text(r + at.dataProvider, at.dataProviderLength);
    }
    s.lat = lat;
    s.lon = lon;
    s.mercator = mercatorOf(lat);
    const auto get = [r] (int offset) { return offset < 0 ? SurfaceStation::missing : static_cast<double>(bigFloat(r + offset)); };
    const auto elevation = get(at.elevation);
    s.elevation = valid(elevation) ? elevation : SurfaceStation::missing;
    const auto temperature = get(at.temperature);
    if (valid(temperature) && temperature > 150.0) {
        s.temperature = temperature - 273.15;
    }
    const auto dew = get(at.dewpoint);
    if (valid(dew) && dew > 150.0) {
        s.dewPoint = dew - 273.15;
    }
    const auto humidity = get(at.relHumidity);
    if (valid(humidity) && humidity >= 0.0 && humidity <= 100.5) {
        s.humidity = humidity;
    }
    const auto direction = get(at.windDir);
    if (valid(direction) && direction >= 0.0 && direction <= 360.0) {
        s.windDirection = direction;
    }
    const auto speed = get(at.windSpeed);
    if (valid(speed) && speed >= 0.0) {
        s.windSpeed = speed * 1.943844;
    }
    const auto gust = get(at.windGust);
    if (valid(gust) && gust >= 0.0) {
        s.windGust = gust * 1.943844;
    }
    const auto visibility = get(at.visibility);
    if (valid(visibility) && visibility >= 0.0) {
        s.visibility = visibility / 1609.344;
    }
    const auto altimeter = get(at.altimeter);
    if (valid(altimeter) && altimeter > 50000.0) {
        s.altimeter = altimeter / 3386.389;
    }
    const auto seaLevel = get(at.seaLevelPressure);
    if (valid(seaLevel) && seaLevel > 50000.0) {
        s.seaLevel = seaLevel / 100.0;
    }
    if (at.temperatureDD >= 0) {
        s.quality = r[at.temperatureDD];
    }
    if (s.quality == 'X' || s.quality == 'B') {   // rejected by the quality checks
        s.temperature = SurfaceStation::missing;
    }
    // a report with nothing to show is not a station worth a mark
    if (!SurfaceStation::has(s.temperature) && !SurfaceStation::has(s.windSpeed) && !SurfaceStation::has(s.altimeter) && !SurfaceStation::has(s.dewPoint)) {
        return;
    }
    newest = std::max(newest, s.seconds);
    latest[key] = std::move(s);
}

vector<SurfaceStation> UtilityMadis::MesonetReader::stations() const {
    vector<SurfaceStation> all;
    all.reserve(latest.size());
    for (const auto& [key, s] : latest) {
        if (maxAge <= 0 || newest - s.seconds <= maxAge) {
            all.push_back(s);
        }
    }
    return all;
}

vector<string> UtilityMadis::listFiles(const string& html) {
    vector<string> names;
    size_t pos = 0;
    while ((pos = html.find("href=\"", pos)) != string::npos) {
        pos += 6;
        const auto end = html.find('"', pos);
        if (end == string::npos) {
            break;
        }
        const string name = html.substr(pos, end - pos);
        if (name.size() == 16 && name.compare(8, 1, "_") == 0 && name.compare(13, 3, ".gz") == 0 &&
            std::all_of(name.begin(), name.begin() + 8, [] (char c) { return c >= '0' && c <= '9'; })) {
            names.push_back(name);
        }
        pos = end;
    }
    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}
