// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/UtilityEcmwfTracks.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>

namespace {
    struct Element {
        int scale;
        int reference;
        int width;
        bool text;
    };

    int pack(int f, int x, int y) {
        return f * 100000 + x * 1000 + y;
    }

    // WMO BUFR Table B, version 35: the elements of template 3 16 082
    const std::map<int, Element>& tableB() {
        static const std::map<int, Element> table{
            {1030, {0, 0, 128, true}},   {1033, {0, 0, 8, false}},   {1034, {0, 0, 8, false}},    {1032, {0, 0, 8, false}},   {1025, {0, 0, 24, true}},
            {1027, {0, 0, 80, true}},   {1090, {0, 0, 8, false}},    {1091, {0, 0, 10, false}},  {1092, {0, 0, 8, false}},
            {4001, {0, 0, 12, false}},  {4002, {0, 0, 4, false}},    {4003, {0, 0, 6, false}},   {4004, {0, 0, 5, false}},
            {4005, {0, 0, 6, false}},   {8005, {0, 0, 4, false}},    {5002, {2, -9000, 15, false}}, {6002, {2, -18000, 16, false}},
            {10051, {-1, 0, 14, false}}, {11012, {1, 0, 12, false}}, {19003, {0, 0, 8, false}},  {5021, {2, 0, 16, false}},
            {19004, {-2, 0, 12, false}}, {31001, {0, 0, 8, false}},  {8021, {0, 0, 5, false}},   {4024, {0, -2048, 12, false}},
        };
        return table;
    }

    // WMO BUFR Table D, version 35: the sequences the template uses
    const std::map<int, vector<int>>& tableD() {
        static const std::map<int, vector<int>> table{
            {pack(3, 1, 11), {pack(0, 4, 1), pack(0, 4, 2), pack(0, 4, 3)}},
            {pack(3, 1, 12), {pack(0, 4, 4), pack(0, 4, 5)}},
            {pack(3, 1, 23), {pack(0, 5, 2), pack(0, 6, 2)}},
            {pack(3, 16, 82), {1033, 1034, 1032, 1025, 1027, 1090, 1091, 1092, pack(3, 1, 11), pack(3, 1, 12),
                               8005, pack(3, 1, 23), 8005, pack(3, 1, 23), 10051, 8005, pack(3, 1, 23), 11012, pack(1, 7, 3), 19003,
                               pack(1, 5, 4), 5021, 5021, pack(2, 1, 131), 19004, pack(2, 1, 0), pack(1, 16, 0), 31001, 8021, 4024,
                               8005, pack(3, 1, 23), 10051, 8005, pack(3, 1, 23), 11012, pack(1, 7, 3), 19003, pack(1, 5, 4), 5021,
                               5021, pack(2, 1, 131), 19004, pack(2, 1, 0)}},
        };
        return table;
    }

    struct Field {
        int code;                     // F = 0 descriptor as packed (0 x y -> x * 1000 + y)
        vector<double> values;        // per subset, UtilityEcmwfTracks::missing where the bits were all ones
        vector<string> texts;
    };

    struct Bits {
        const unsigned char * data;
        size_t bitSize;
        size_t pos{0};
        bool failed{false};

        uint64_t get(int n) {
            uint64_t value = 0;
            if (pos + static_cast<size_t>(n) > bitSize) {
                failed = true;
                return 0;
            }
            for (int i = 0; i < n; i++, pos++) {
                value = (value << 1) | ((data[pos >> 3] >> (7 - (pos & 7))) & 1u);
            }
            return value;
        }
    };

    struct Decoder {
        Bits bits;
        int subsets;
        vector<Field> fields;
        string error;
        int widthAdd{0};
        bool compressed{true};   // a storm with one member is not compressed: its values follow one after another

        bool run(const vector<int>& list) {
            for (size_t i = 0; i < list.size(); i++) {
                const int d = list[i];
                const int f = d / 100000;
                const int x = d / 1000 % 100;
                const int y = d % 1000;
                if (f == 0) {
                    if (!element(x * 1000 + y)) {
                        return false;
                    }
                } else if (f == 1) {
                    int count = y;
                    size_t first = i + 1;
                    if (y == 0) {   // delayed: the next descriptor is the replication factor, itself a (compressed) element
                        if (first >= list.size() || !element(31001)) {
                            error = error.empty() ? "replication factor missing" : error;
                            return false;
                        }
                        const auto& factor = fields.back();
                        count = static_cast<int>(factor.values[0]);
                        for (const auto v : factor.values) {
                            if (static_cast<int>(v) != count) {
                                error = "the subsets differ in their replication";
                                return false;
                            }
                        }
                        first++;
                    }
                    if (first + static_cast<size_t>(x) > list.size()) {
                        error = "replication runs past its sequence";
                        return false;
                    }
                    const vector<int> body(list.begin() + static_cast<long>(first), list.begin() + static_cast<long>(first) + x);
                    for (int n = 0; n < count; n++) {
                        if (!run(body)) {
                            return false;
                        }
                    }
                    i = first + static_cast<size_t>(x) - 1;
                } else if (f == 2) {
                    if (x != 1) {
                        error = "unsupported operator " + std::to_string(d);
                        return false;
                    }
                    widthAdd = y == 0 ? 0 : y - 128;   // 2 01 YYY: add YYY - 128 bits to the width of the numbers that follow
                } else {
                    const auto found = tableD().find(d);
                    if (found == tableD().end()) {
                        error = "unknown sequence " + std::to_string(d);
                        return false;
                    }
                    if (!run(found->second)) {
                        return false;
                    }
                }
            }
            return true;
        }

        // one element of a compressed message: the lowest value (or the string), the bits of each subset's increment
        bool element(int code) {
            const auto found = tableB().find(code);
            if (found == tableB().end()) {
                error = "unknown element " + std::to_string(code);
                return false;
            }
            const auto& e = found->second;
            Field field;
            field.code = code;
            if (!compressed) {
                if (e.text) {
                    string text;
                    for (int i = 0; i < e.width / 8; i++) {
                        text.push_back(static_cast<char>(bits.get(8)));
                    }
                    field.texts.push_back(text);
                } else {
                    const int width = e.width + widthAdd;
                    const uint64_t raw = bits.get(width);
                    const bool isMissing = raw == (width >= 64 ? ~0ull : (1ull << width) - 1ull);
                    field.values.push_back(isMissing ? UtilityEcmwfTracks::missing
                                                     : (static_cast<double>(static_cast<int64_t>(raw) + e.reference)) * std::pow(10.0, -e.scale));
                }
            } else if (e.text) {
                const int bytes = e.width / 8;
                string base;
                for (int i = 0; i < bytes; i++) {
                    base.push_back(static_cast<char>(bits.get(8)));
                }
                const int increment = static_cast<int>(bits.get(6));
                for (int s = 0; s < subsets; s++) {
                    string text = base;
                    if (increment > 0) {
                        text.clear();
                        for (int i = 0; i < increment; i++) {
                            text.push_back(static_cast<char>(bits.get(8)));
                        }
                    }
                    field.texts.push_back(text);
                }
            } else {
                const int width = e.width + widthAdd;
                const uint64_t allOnes = width >= 64 ? ~0ull : (1ull << width) - 1ull;
                const uint64_t lowest = bits.get(width);
                const int increment = static_cast<int>(bits.get(6));
                for (int s = 0; s < subsets; s++) {
                    uint64_t raw = lowest;
                    bool isMissing = false;
                    if (increment == 0) {
                        isMissing = lowest == allOnes;
                    } else {
                        const uint64_t inc = bits.get(increment);
                        isMissing = inc == ((1ull << increment) - 1ull);
                        raw = lowest + inc;
                    }
                    field.values.push_back(isMissing ? UtilityEcmwfTracks::missing
                                                     : (static_cast<double>(static_cast<int64_t>(raw) + e.reference)) * std::pow(10.0, -e.scale));
                }
            }
            if (bits.failed) {
                error = "the message ends early";
                return false;
            }
            fields.push_back(std::move(field));
            return true;
        }
    };

    unsigned long be(const unsigned char * p, int n) {
        unsigned long value = 0;
        for (int i = 0; i < n; i++) {
            value = (value << 8) | p[i];
        }
        return value;
    }

    // the descriptor of a field in a position, cursor style
    struct Cursor {
        const vector<Field>& fields;
        size_t at{0};
        bool expect(int code) {
            return at < fields.size() && fields[at].code == code;
        }
    };

    double knots(double metersPerSecond) {
        return UtilityEcmwfTracks::has(metersPerSecond) ? metersPerSecond * 1.943844 : UtilityEcmwfTracks::missing;
    }

    double hectopascal(double pascal) {
        return UtilityEcmwfTracks::has(pascal) ? pascal / 100.0 : UtilityEcmwfTracks::missing;
    }
}

namespace {
    // the ordered fields of one message -> a storm (the order is that of the template, checked as it is read)
    bool interpret(const vector<Field>& fields, int subsets, UtilityEcmwfTracks::Storm& storm, string& error) {
        Cursor c{fields};
        const auto take = [&] (int code) -> const Field * {
            if (!c.expect(code)) {
                return nullptr;
            }
            return &fields[c.at++];
        };
        // 001030 numerical model identifier (the AIFS files lead with it, the IFS ones do not), then the header of the template
        take(1030);
        if (take(1033) == nullptr || take(1034) == nullptr || take(1032) == nullptr) {
            error = "unexpected header";
            return false;
        }
        const auto * stormId = take(1025);
        const auto * stormName = take(1027);
        take(1090);
        const auto * memberNumber = take(1091);
        const auto * memberType = take(1092);
        if (stormId == nullptr || stormName == nullptr || memberNumber == nullptr || memberType == nullptr) {
            error = "unexpected header fields";
            return false;
        }
        auto trim = [] (string s) {
            const auto a = s.find_first_not_of(' ');
            const auto b = s.find_last_not_of(' ');
            return a == string::npos ? string{} : s.substr(a, b - a + 1);
        };
        storm.id = trim(stormId->texts[0]);
        storm.name = trim(stormName->texts[0]);
        const auto * year = take(4001);
        const auto * month = take(4002);
        const auto * day = take(4003);
        const auto * hour = take(4004);
        take(4005);
        if (year == nullptr || month == nullptr || day == nullptr || hour == nullptr) {
            error = "unexpected time fields";
            return false;
        }
        char cycle[16];
        std::snprintf(cycle, sizeof cycle, "%04d%02d%02d%02d", static_cast<int>(year->values[0]), static_cast<int>(month->values[0]),
                      static_cast<int>(day->values[0]), static_cast<int>(hour->values[0]));
        storm.cycle = cycle;
        storm.members.assign(static_cast<size_t>(subsets), {});
        for (int s = 0; s < subsets; s++) {
            storm.members[static_cast<size_t>(s)].number = static_cast<int>(memberNumber->values[static_cast<size_t>(s)]);
            storm.members[static_cast<size_t>(s)].type = static_cast<int>(memberType->values[static_cast<size_t>(s)]);
        }
        const auto group = [&] (int hourOfGroup, bool withCentre) -> bool {
            vector<int> codes{8005, 5002, 6002, 10051, 8005, 5002, 6002, 11012};   // pressure minimum (position, pressure), strongest wind (position, speed)
            if (withCentre) {
                codes.insert(codes.begin(), {8005, 5002, 6002});   // the analysis group starts with the feature centre
            }
            for (const int code : codes) {
                if (!c.expect(code)) {
                    error = "unexpected field " + std::to_string(code) + " at " + std::to_string(c.at);
                    return false;
                }
                c.at++;
            }
            const auto at = [&] (size_t back) -> const Field & { return fields[c.at - back]; };
            for (int s = 0; s < subsets; s++) {
                const auto u = static_cast<size_t>(s);
                UtilityEcmwfTracks::Step step;
                step.hour = hourOfGroup;
                step.lat = at(7).values[u];
                step.lon = at(6).values[u];
                step.pressure = hectopascal(at(5).values[u]);
                step.windLat = at(3).values[u];
                step.windLon = at(2).values[u];
                step.wind = knots(at(1).values[u]);
                storm.members[u].steps.push_back(step);
            }
            c.at += 3 * 13;   // the wind-radii thresholds, not used here
            return c.at <= fields.size();
        };
        if (!group(0, true)) {
            return false;
        }
        const auto * count = take(31001);
        if (count == nullptr) {
            error = "no forecast steps";
            return false;
        }
        const int steps = static_cast<int>(count->values[0]);
        for (int i = 0; i < steps; i++) {
            take(8021);
            const auto * period = take(4024);
            if (period == nullptr) {
                error = "unexpected step";
                return false;
            }
            if (!group(static_cast<int>(period->values[0]), false)) {
                return false;
            }
        }
        return true;
    }
}

bool UtilityEcmwfTracks::parse(const string& bytes, vector<Storm>& storms, string& error) {
    storms.clear();
    error.clear();
    const auto * data = reinterpret_cast<const unsigned char *>(bytes.data());
    size_t offset = 0;
    while (offset + 8 <= bytes.size()) {
        if (bytes.compare(offset, 4, "BUFR") != 0) {
            offset++;
            continue;
        }
        const size_t total = be(data + offset + 4, 3);
        if (data[offset + 7] != 4 || offset + total > bytes.size() || total < 40) {
            error = "not a BUFR edition 4 message";
            return false;
        }
        const auto * p = data + offset;
        size_t s = 8;   // section 1: octet n is p[s + n - 1]
        const size_t len1 = be(p + s, 3);
        const bool section2 = (p[s + 9] & 0x80) != 0;
        const int tableVersion = p[s + 13];
        s += len1;
        if (section2) {
            s += be(p + s, 3);
        }
        const size_t len3 = be(p + s, 3);
        const int subsets = static_cast<int>(be(p + s + 4, 2));
        const int flags = p[s + 6];
        vector<int> descriptors;
        for (size_t i = s + 7; i + 1 < s + len3; i += 2) {
            descriptors.push_back(pack(p[i] >> 6, p[i] & 0x3f, p[i + 1]));
        }
        s += len3;
        const size_t len4 = be(p + s, 3);
        const bool compressed = (flags & 0x40) != 0;
        if (tableVersion != 35 || (!compressed && subsets != 1)) {
            error = "only BUFR with master table version 35 is understood (this one: version " + std::to_string(tableVersion) + ")";
            return false;
        }
        Decoder decoder{Bits{p + s + 4, (len4 - 4) * 8}, subsets, {}, {}};
        decoder.compressed = compressed;
        if (!decoder.run(descriptors)) {
            error = decoder.error.empty() ? "could not decode a message" : decoder.error;
            return false;
        }
        Storm storm;
        if (!interpret(decoder.fields, subsets, storm, error)) {
            return false;
        }
        storms.push_back(std::move(storm));
        offset += total;
    }
    if (storms.empty()) {
        error = "no tropical cyclone tracks in the file";
        return false;
    }
    return true;
}
