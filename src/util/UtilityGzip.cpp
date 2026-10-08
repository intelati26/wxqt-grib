// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/UtilityGzip.h"
#include <array>
#include <functional>
#include <cstdint>

namespace {
    struct Bits {
        const unsigned char * data;
        size_t size;
        size_t pos{0};
        uint32_t buffer{0};
        int count{0};
        bool failed{false};
        // streaming: the decoded bytes leave through the sink once `limit` of them wait, keeping the 32 kB the back references can reach
        const std::function<bool(const char *, size_t)> * sink{nullptr};
        size_t limit{0};
        size_t start{0};     // the first byte of `out` the sink has not seen
        bool stopped{false};

        int get(int n) {   // n bits, least significant first
            while (count < n) {
                if (pos >= size) {
                    failed = true;
                    return 0;
                }
                buffer |= static_cast<uint32_t>(data[pos++]) << count;
                count += 8;
            }
            const int value = static_cast<int>(buffer & ((1u << n) - 1u));
            buffer >>= n;
            count -= n;
            return value;
        }
    };

    const size_t window = 32768;

    // true while decoding may go on
    bool flush(Bits& bits, std::string& out, bool final) {
        if (bits.sink == nullptr) {
            return true;
        }
        if (final) {
            if (out.size() > bits.start && !(*bits.sink)(out.data() + bits.start, out.size() - bits.start)) {
                bits.stopped = true;
                return false;
            }
            bits.start = out.size();
            return true;
        }
        if (out.size() < bits.limit + window) {
            return true;
        }
        const size_t end = out.size() - window;
        if (end > bits.start && !(*bits.sink)(out.data() + bits.start, end - bits.start)) {
            bits.stopped = true;
            return false;
        }
        out.erase(0, end);
        bits.start = 0;
        return true;
    }

    // canonical Huffman code: counts of codes per length and the symbols in code order
    struct Huffman {
        std::array<int, 16> counts{};
        std::array<int, 288> symbols{};

        void build(const int * lengths, int n) {
            counts.fill(0);
            for (int i = 0; i < n; i++) {
                counts[lengths[i]]++;
            }
            counts[0] = 0;
            std::array<int, 16> offsets{};
            for (int len = 1; len < 15; len++) {
                offsets[len + 1] = offsets[len] + counts[len];
            }
            for (int i = 0; i < n; i++) {
                if (lengths[i] != 0) {
                    symbols[offsets[lengths[i]]++] = i;
                }
            }
        }

        int decode(Bits& bits) const {
            int code = 0;
            int first = 0;
            int index = 0;
            for (int len = 1; len <= 15; len++) {
                code |= bits.get(1);
                if (bits.failed) {
                    return -1;
                }
                const int count = counts[len];
                if (code - count < first) {
                    return symbols[index + (code - first)];
                }
                index += count;
                first += count;
                first <<= 1;
                code <<= 1;
            }
            return -1;
        }
    };

    const int lengthBase[] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    const int lengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    const int distBase[] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    const int distExtra[] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    bool codes(Bits& bits, std::string& out, const Huffman& lit, const Huffman& dist) {
        while (true) {
            const int symbol = lit.decode(bits);
            if (symbol < 0) {
                return false;
            }
            if (symbol < 256) {
                out.push_back(static_cast<char>(symbol));
                if (bits.sink != nullptr && !flush(bits, out, false)) {
                    return false;
                }
            } else if (symbol == 256) {
                return true;
            } else {
                const int s = symbol - 257;
                if (s >= 29) {
                    return false;
                }
                const int length = lengthBase[s] + bits.get(lengthExtra[s]);
                const int d = dist.decode(bits);
                if (d < 0 || d >= 30) {
                    return false;
                }
                const size_t distance = static_cast<size_t>(distBase[d] + bits.get(distExtra[d]));
                if (bits.failed || distance > out.size()) {
                    return false;
                }
                for (int i = 0; i < length; i++) {
                    out.push_back(out[out.size() - distance]);
                }
                if (bits.sink != nullptr && !flush(bits, out, false)) {
                    return false;
                }
            }
        }
    }

    bool inflateBlock(Bits& bits, std::string& out, int type) {
        Huffman lit;
        Huffman dist;
        if (type == 1) {
            int lengths[288];
            for (int i = 0; i < 144; i++) lengths[i] = 8;
            for (int i = 144; i < 256; i++) lengths[i] = 9;
            for (int i = 256; i < 280; i++) lengths[i] = 7;
            for (int i = 280; i < 288; i++) lengths[i] = 8;
            lit.build(lengths, 288);
            int dl[30];
            for (int& l : dl) l = 5;
            dist.build(dl, 30);
        } else {
            const int nlen = bits.get(5) + 257;
            const int ndist = bits.get(5) + 1;
            const int ncode = bits.get(4) + 4;
            if (nlen > 286 || ndist > 30) {
                return false;
            }
            static const int order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            int lengths[320] = {};
            for (int i = 0; i < ncode; i++) {
                lengths[order[i]] = bits.get(3);
            }
            Huffman codeLengths;
            codeLengths.build(lengths, 19);
            int index = 0;
            int all[320] = {};
            while (index < nlen + ndist) {
                const int symbol = codeLengths.decode(bits);
                if (symbol < 0) {
                    return false;
                }
                if (symbol < 16) {
                    all[index++] = symbol;
                } else {
                    int previous = 0;
                    int repeat;
                    if (symbol == 16) {
                        if (index == 0) {
                            return false;
                        }
                        previous = all[index - 1];
                        repeat = 3 + bits.get(2);
                    } else if (symbol == 17) {
                        repeat = 3 + bits.get(3);
                    } else {
                        repeat = 11 + bits.get(7);
                    }
                    if (index + repeat > nlen + ndist) {
                        return false;
                    }
                    while (repeat-- > 0) {
                        all[index++] = previous;
                    }
                }
            }
            lit.build(all, nlen);
            dist.build(all + nlen, ndist);
        }
        return codes(bits, out, lit, dist) && !bits.failed;
    }
}

namespace {
    // where the deflate data starts, 0 when this is not gzip
    size_t gzipBody(const std::string& in) {
        const auto * p = reinterpret_cast<const unsigned char *>(in.data());
        const size_t size = in.size();
        if (size < 18 || p[0] != 0x1f || p[1] != 0x8b || p[2] != 8) {
            return 0;
        }
        const int flags = p[3];
        size_t pos = 10;
        if (flags & 4) {   // extra field
            if (pos + 2 > size) return 0;
            pos += 2 + (p[pos] | (p[pos + 1] << 8));
        }
        for (const int bit : {8, 16}) {   // file name, comment: zero-terminated
            if (flags & bit) {
                while (pos < size && p[pos] != 0) pos++;
                pos++;
            }
        }
        if (flags & 2) {
            pos += 2;
        }
        return pos >= size ? 0 : pos;
    }

    bool inflateInto(const unsigned char * p, size_t size, std::string& out, const std::function<bool(const char *, size_t)> * sink, size_t chunk);
}

bool UtilityGzip::gunzip(const std::string& in, std::string& out) {
    out.clear();
    const size_t pos = gzipBody(in);
    return pos != 0 && inflate(reinterpret_cast<const unsigned char *>(in.data()) + pos, in.size() - pos, out);
}

bool UtilityGzip::gunzipStream(const std::string& in, size_t chunk, const std::function<bool(const char *, size_t)>& sink) {
    const size_t pos = gzipBody(in);
    std::string out;
    return pos != 0 && inflateInto(reinterpret_cast<const unsigned char *>(in.data()) + pos, in.size() - pos, out, &sink, chunk);
}

namespace {
bool inflateInto(const unsigned char * p, size_t size, std::string& out, const std::function<bool(const char *, size_t)> * sink, size_t chunk) {
    out.clear();
    Bits bits{p, size};
    bits.sink = sink;
    bits.limit = chunk;
    bool last = false;
    while (!last) {
        last = bits.get(1) != 0;
        const int type = bits.get(2);
        if (bits.failed) {
            return false;
        }
        if (type == 0) {
            bits.buffer = 0;   // stored block: to a byte boundary, then length and its complement
            bits.count = 0;
            if (bits.pos + 4 > bits.size) return false;
            const unsigned len = bits.data[bits.pos] | (bits.data[bits.pos + 1] << 8);
            const unsigned nlen = bits.data[bits.pos + 2] | (bits.data[bits.pos + 3] << 8);
            bits.pos += 4;
            if ((len ^ 0xffffu) != nlen || bits.pos + len > bits.size) return false;
            out.append(reinterpret_cast<const char *>(bits.data + bits.pos), len);
            bits.pos += len;
            if (!flush(bits, out, false)) {
                return false;
            }
        } else if (type == 1 || type == 2) {
            if (!inflateBlock(bits, out, type)) {
                return false;
            }
        } else {
            return false;
        }
    }
    return flush(bits, out, true);
}
}

bool UtilityGzip::inflate(const unsigned char * p, size_t size, std::string& out) {
    return inflateInto(p, size, out, nullptr, 0);
}
