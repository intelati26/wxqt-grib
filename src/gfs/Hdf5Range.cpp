// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/Hdf5Range.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <vector>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace {
    constexpr quint64 undefinedAddress = ~0ULL;

    quint64 le(const char * p, int n) {
        quint64 v = 0;
        for (int i = n - 1; i >= 0; i--) {
            v = (v << 8) | static_cast<unsigned char>(p[i]);
        }
        return v;
    }

    struct Failure {
        std::string why;
    };

    // The file as it is asked for: ranges of it in blocks that are kept (the metadata is small and scattered)
    class Source {
    public:
        explicit Source(Hdf5Range::Fetch f) : fetch{std::move(f)} {}
        QByteArray read(quint64 offset, quint64 length) {
            if (length == 0) {
                return {};
            }
            if (length > 4 * blockSize) {   // the chunks of a variable: read as they are
                const auto bytes = fetch(static_cast<long long>(offset), static_cast<long long>(offset + length - 1));
                if (static_cast<quint64>(bytes.size()) != length) {
                    throw Failure{"the file's bytes could not be read (" + std::to_string(offset) + " + " + std::to_string(length) + ")"};
                }
                return bytes;
            }
            QByteArray out;
            out.reserve(static_cast<qsizetype>(length));
            for (quint64 b = offset / blockSize; b <= (offset + length - 1) / blockSize; b++) {
                const auto& block = blockAt(b);
                const quint64 from = std::max(offset, b * blockSize) - b * blockSize;
                const quint64 to = std::min(offset + length, (b + 1) * blockSize) - b * blockSize;
                if (to > static_cast<quint64>(block.size())) {
                    throw Failure{"the file ends before " + std::to_string(offset + length)};
                }
                out.append(block.constData() + from, static_cast<qsizetype>(to - from));
            }
            return out;
        }
    private:
        static constexpr quint64 blockSize = 1 << 17;
        const QByteArray& blockAt(quint64 b) {
            auto found = blocks.find(b);
            if (found == blocks.end()) {
                auto bytes = fetch(static_cast<long long>(b * blockSize), static_cast<long long>((b + 1) * blockSize - 1));
                if (bytes.isEmpty()) {
                    throw Failure{"the file's bytes could not be read (block " + std::to_string(b) + ")"};
                }
                found = blocks.emplace(b, std::move(bytes)).first;
            }
            return found->second;
        }
        Hdf5Range::Fetch fetch;
        std::map<quint64, QByteArray> blocks;
    };

    struct Datatype {
        int cls{-1};        // 0 integer, 1 floating point, 3 string, 7 reference, 9 variable length
        int size{0};
        bool bigEndian{false};
        bool isSigned{true};
    };

    struct Attribute {
        std::string name;
        Datatype type;
        std::vector<quint64> dims;
        QByteArray data;
    };

    struct Filter {
        int id{0};
        std::vector<quint32> values;
    };

    struct Dataset {
        std::string name;
        quint64 address{0};
        std::vector<quint64> dims;
        Datatype type;
        int layout{-1};   // 0 compact, 1 contiguous, 2 chunked
        quint64 dataAddress{undefinedAddress}, dataSize{0};
        QByteArray compact;
        std::vector<quint64> chunkDims;   // the rank's, without the element size
        quint64 btree{undefinedAddress};
        std::vector<Filter> filters;
        std::vector<Attribute> attributes;
        quint64 symbolTreeAddress{undefinedAddress}, symbolHeapAddress{undefinedAddress};
        std::map<std::string, quint64> links;   // the members of a group that are link messages in its header
        quint64 linkHeap{undefinedAddress}, attributeHeap{undefinedAddress};   // the fractal heaps that hold more links and attributes than fit in a header
        const Attribute * attribute(const std::string& n) const {
            for (const auto& a : attributes) {
                if (a.name == n) {
                    return &a;
                }
            }
            return nullptr;
        }
        quint64 count() const {
            quint64 n = 1;
            for (const auto d : dims) {
                n *= d;
            }
            return n;
        }
    };

    Datatype parseDatatype(const char * p, size_t available) {
        Datatype t;
        if (available < 8) {
            return t;
        }
        t.cls = static_cast<unsigned char>(p[0]) & 0x0F;
        const unsigned char bits = static_cast<unsigned char>(p[1]);
        t.size = static_cast<int>(le(p + 4, 4));
        if (t.cls == 0) {
            t.bigEndian = (bits & 1) != 0;
            t.isSigned = (bits & 8) != 0;
        } else if (t.cls == 1) {
            t.bigEndian = (bits & 1) != 0;
        }
        return t;
    }

    std::vector<quint64> parseDataspace(const char * p, size_t size) {
        std::vector<quint64> dims;
        if (size < 4) {
            return dims;
        }
        const int version = static_cast<unsigned char>(p[0]);
        const int rank = static_cast<unsigned char>(p[1]);
        size_t at = version == 1 ? 8 : 4;
        if (version == 2 && static_cast<unsigned char>(p[3]) == 2) {   // a null dataspace
            return dims;
        }
        for (int i = 0; i < rank && at + 8 <= size; i++, at += 8) {
            dims.push_back(le(p + at, 8));
        }
        return dims;
    }

    size_t pad8(size_t n) { return (n + 7) & ~static_cast<size_t>(7); }

    class File {
    public:
        explicit File(Hdf5Range::Fetch f) : source{std::move(f)} {
            const auto sb = source.read(0, 96);
            static const char signature[8] = {'\x89', 'H', 'D', 'F', '\r', '\n', '\x1a', '\n'};
            if (sb.size() < 96 || std::memcmp(sb.constData(), signature, 8) != 0) {
                throw Failure{"not an HDF5 file"};
            }
            if (sb[8] != 0 && sb[8] != 1) {
                throw Failure{"an HDF5 superblock of version " + std::to_string(static_cast<int>(sb[8])) + " (only the original format is read)"};
            }
            if (sb[13] != 8 || sb[14] != 8) {
                throw Failure{"an HDF5 file with offsets that are not 8 bytes"};
            }
            const size_t entry = sb[8] == 0 ? 56 : 60;   // the root group's symbol table entry follows the addresses
            base = le(sb.constData() + (sb[8] == 0 ? 24 : 28), 8);
            const auto rootHeader = le(sb.constData() + entry + 8, 8);
            root = readObject("/", rootHeader + base);
        }
        Dataset root;
        std::map<std::string, quint64> children;   // the root group's members: name -> object header address
        std::map<quint64, std::string> namesOf;

        void listRoot() {
            if (root.symbolTreeAddress == undefinedAddress || root.symbolHeapAddress == undefinedAddress) {   // the newer format: link messages, in the header and in a fractal heap
                for (const auto& [name, address] : root.links) {
                    children[name] = address;
                    namesOf[address] = name;
                }
                if (root.linkHeap != undefinedAddress) {
                    for (const auto& block : heapBlocks(root.linkHeap)) {
                        size_t at = 0;
                        while (at + 4 < static_cast<size_t>(block.size())) {
                            Dataset scratch;
                            const size_t used = link(scratch, block.constData() + at, static_cast<size_t>(block.size()) - at);
                            if (used == 0) {
                                break;
                            }
                            at += used;
                            for (const auto& [name, address] : scratch.links) {
                                children[name] = address;
                                namesOf[address] = name;
                            }
                        }
                    }
                }
                if (children.empty()) {
                    throw Failure{"the root group has no members that are read"};
                }
                return;
            }
            const auto heap = source.read(root.symbolHeapAddress, 32);
            if (heap.size() < 32 || std::memcmp(heap.constData(), "HEAP", 4) != 0) {
                throw Failure{"the root group's name heap is not there"};
            }
            const quint64 segment = le(heap.constData() + 24, 8);
            walkGroupTree(root.symbolTreeAddress, segment);
        }

        Dataset readObject(const std::string& name, quint64 address) {
            Dataset d;
            d.name = name;
            d.address = address;
            const auto header = source.read(address, 32);
            if (header.size() >= 5 && std::memcmp(header.constData(), "OHDR", 4) == 0) {
                readObjectV2(d, address, header);
                denseAttributes(d);
                return d;
            }
            if (header[0] != 1) {
                throw Failure{"an object header of a version that is not read (" + std::to_string(static_cast<int>(header[0])) + ")"};
            }
            std::vector<std::pair<quint64, quint64>> blocks{{address + 16, le(header.constData() + 8, 4)}};
            struct Dense {
                File * file;
                Dataset * d;
                ~Dense() { file->denseAttributes(*d); }
            } dense{this, &d};
            for (size_t b = 0; b < blocks.size(); b++) {
                const auto data = source.read(blocks[b].first, blocks[b].second);
                size_t at = 0;
                while (at + 8 <= static_cast<size_t>(data.size())) {
                    const int type = static_cast<int>(le(data.constData() + at, 2));
                    const size_t size = static_cast<size_t>(le(data.constData() + at + 2, 2));
                    const size_t from = at + 8;
                    if (from + size > static_cast<size_t>(data.size())) {
                        break;
                    }
                    message(d, type, data.constData() + from, size, blocks);
                    at = from + size;
                }
            }
            return d;
        }

        // an object header of the newer format ("OHDR"): the messages in chunks, each with a checksum at its end
        void readObjectV2(Dataset& d, quint64 address, const QByteArray& header) {
            const int flags = static_cast<unsigned char>(header[5]);
            size_t at = 6;
            if (flags & 0x20) {
                at += 16;   // the times
            }
            if (flags & 0x10) {
                at += 4;    // the numbers of messages kept compact and dense
            }
            const int sizeBytes = 1 << (flags & 3);
            const quint64 firstSize = le(header.constData() + at, sizeBytes);
            at += static_cast<size_t>(sizeBytes);
            const bool order = (flags & 0x04) != 0;
            // (the chunks: where the messages start, how many bytes of them)
            std::vector<std::pair<quint64, quint64>> chunks{{address + at, firstSize}};
            std::vector<std::pair<quint64, quint64>> blocks;   // (unused for v2: the continuation chunks come back through here)
            for (size_t c = 0; c < chunks.size(); c++) {
                const auto data = source.read(chunks[c].first, chunks[c].second);
                size_t p = 0;
                while (p + 4 <= static_cast<size_t>(data.size())) {
                    const int type = static_cast<unsigned char>(data[static_cast<qsizetype>(p)]);
                    const size_t size = static_cast<size_t>(le(data.constData() + p + 1, 2));
                    p += 4 + (order ? 2 : 0);
                    if (p + size > static_cast<size_t>(data.size())) {
                        break;
                    }
                    if (type == 0x10 && size >= 16) {   // a continuation: that chunk starts with "OCHK" and ends with a checksum
                        const quint64 where = le(data.constData() + p, 8) + base, length = le(data.constData() + p + 8, 8);
                        if (length > 8) {
                            chunks.push_back({where + 4, length - 8});
                        }
                    } else {
                        message(d, type, data.constData() + p, size, blocks);
                    }
                    p += size;
                }
            }
        }

        // the attributes a header does not hold (many of them: a fractal heap), read from it
        void denseAttributes(Dataset& d) {
            if (d.attributeHeap == undefinedAddress) {
                return;
            }
            const auto heap = d.attributeHeap;
            d.attributeHeap = undefinedAddress;
            for (const auto& block : heapBlocks(heap)) {
                size_t at = 0;
                while (at + 9 < static_cast<size_t>(block.size()) && block[static_cast<qsizetype>(at)] == 3) {
                    const size_t used = attribute(d, block.constData() + at, static_cast<size_t>(block.size()) - at, true);
                    if (used == 0) {
                        break;
                    }
                    at += used;
                }
            }
        }

        QByteArray read(quint64 offset, quint64 length) { return source.read(offset, length); }
        quint64 baseAddress() const { return base; }

        // the object header address the dimension of a variable refers to (its DIMENSION_LIST attribute's `index`th dimension)
        quint64 dimensionScale(const Attribute& list, size_t index) {
            if (list.data.size() < static_cast<qsizetype>((index + 1) * 16)) {
                return undefinedAddress;
            }
            const char * entry = list.data.constData() + index * 16;
            const quint32 count = static_cast<quint32>(le(entry, 4));
            const quint64 collection = le(entry + 4, 8);
            const quint32 object = static_cast<quint32>(le(entry + 12, 4));
            if (count < 1) {
                return undefinedAddress;
            }
            const auto head = source.read(collection + base, 16);
            if (head.size() < 16 || std::memcmp(head.constData(), "GCOL", 4) != 0) {
                return undefinedAddress;
            }
            const quint64 total = le(head.constData() + 8, 8);
            const auto all = source.read(collection + base, std::min<quint64>(total, 1 << 20));
            size_t at = 16;
            while (at + 16 <= static_cast<size_t>(all.size())) {
                const quint32 id = static_cast<quint32>(le(all.constData() + at, 2));
                const quint64 size = le(all.constData() + at + 8, 8);
                if (id == 0) {
                    break;
                }
                if (id == object) {
                    return at + 16 + 8 <= static_cast<size_t>(all.size()) ? le(all.constData() + at + 16, 8) + base : undefinedAddress;
                }
                at += 16 + pad8(static_cast<size_t>(size));
            }
            return undefinedAddress;
        }

    private:
        void message(Dataset& d, int type, const char * p, size_t size, std::vector<std::pair<quint64, quint64>>& blocks) {
            switch (type) {
                case 0x0001: d.dims = parseDataspace(p, size); break;
                case 0x0003: d.type = parseDatatype(p, size); break;
                case 0x0008: layout(d, p, size); break;
                case 0x000B: pipeline(d, p, size); break;
                case 0x000C: attribute(d, p, size); break;
                case 0x0010:
                    if (size >= 16) {
                        blocks.push_back({le(p, 8) + base, le(p + 8, 8)});
                    }
                    break;
                case 0x0002:   // link info: where the links are when there are many
                    if (size >= 18) {
                        const int f = static_cast<unsigned char>(p[1]);
                        const size_t at = 2 + ((f & 1) ? 8 : 0);
                        if (at + 8 <= size) {
                            d.linkHeap = le(p + at, 8);
                            if (d.linkHeap != undefinedAddress) {
                                d.linkHeap += base;
                            }
                        }
                    }
                    break;
                case 0x0006: link(d, p, size); break;
                case 0x0015:   // attribute info: where the attributes are when there are many
                    if (size >= 18) {
                        const int f = static_cast<unsigned char>(p[1]);
                        const size_t at = 2 + ((f & 1) ? 2 : 0);
                        if (at + 8 <= size) {
                            d.attributeHeap = le(p + at, 8);
                            if (d.attributeHeap != undefinedAddress) {
                                d.attributeHeap += base;
                            }
                        }
                    }
                    break;
                case 0x0011:
                    if (size >= 16) {
                        d.symbolTreeAddress = le(p, 8) + base;
                        d.symbolHeapAddress = le(p + 8, 8) + base;
                    }
                    break;
                default: break;
            }
        }

        // one link message (a hard link: the name and the address of the object); the length it took, 0 when it is not one that is read
        size_t link(Dataset& d, const char * p, size_t size) {
            if (size < 3 || p[0] != 1) {
                return 0;
            }
            const int f = static_cast<unsigned char>(p[1]);
            size_t at = 2;
            int type = 0;
            if (f & 0x08) {
                type = static_cast<unsigned char>(p[at++]);
            }
            if (f & 0x04) {
                at += 8;
            }
            if (f & 0x10) {
                at += 1;
            }
            const int lengthBytes = 1 << (f & 3);
            if (at + static_cast<size_t>(lengthBytes) > size) {
                return 0;
            }
            const size_t nameLength = static_cast<size_t>(le(p + at, lengthBytes));
            at += static_cast<size_t>(lengthBytes);
            if (at + nameLength > size) {
                return 0;
            }
            const std::string name{p + at, nameLength};
            at += nameLength;
            if (type == 0 && at + 8 <= size) {
                d.links[name] = le(p + at, 8) + base;
                at += 8;
                return at;
            }
            return 0;
        }

        // the data of the direct blocks of a fractal heap (the objects are packed from the start of each)
        std::vector<QByteArray> heapBlocks(quint64 address) {
            std::vector<QByteArray> out;
            const auto h = source.read(address, 148);
            if (h.size() < 148 || std::memcmp(h.constData(), "FRHP", 4) != 0) {
                throw Failure{"a fractal heap is not there"};
            }
            const bool filtered = le(h.constData() + 7, 2) != 0;
            if (filtered) {
                throw Failure{"a fractal heap with filters"};
            }
            const bool checksummed = (static_cast<unsigned char>(h[9]) & 2) != 0;
            const quint64 width = le(h.constData() + 110, 2), start = le(h.constData() + 112, 8), maxDirect = le(h.constData() + 120, 8);
            const int heapBits = static_cast<int>(le(h.constData() + 128, 2));
            const quint64 rootAddress = le(h.constData() + 132, 8);
            const quint64 rows = le(h.constData() + 140, 2);
            const size_t offsetBytes = static_cast<size_t>((heapBits + 7) / 8);
            const size_t directHeader = 5 + 8 + offsetBytes + (checksummed ? 4 : 0);
            std::function<void(quint64, quint64, bool, quint64)> block = [&] (quint64 at, quint64 size, bool direct, quint64 knownRows) {
                if (at == undefinedAddress) {
                    return;
                }
                if (direct) {
                    const auto data = source.read(at + base, size);
                    if (data.size() < static_cast<qsizetype>(directHeader) || std::memcmp(data.constData(), "FHDB", 4) != 0) {
                        throw Failure{"a fractal heap's block is not there"};
                    }
                    out.push_back(data.mid(static_cast<qsizetype>(directHeader)));
                    return;
                }
                const quint64 inside = knownRows > 0 ? knownRows : static_cast<quint64>(std::max(1.0, std::log2(static_cast<double>(size) / static_cast<double>(start * width)) + 1.0));
                const auto head = source.read(at + base, 5 + 8 + offsetBytes + inside * width * 8);
                if (head.size() < 5 || std::memcmp(head.constData(), "FHIB", 4) != 0) {
                    throw Failure{"a fractal heap's indirect block is not there"};
                }
                size_t entry = 5 + 8 + offsetBytes;
                for (quint64 row = 0; row < inside; row++) {
                    const quint64 childSize = row == 0 ? start : start << (row - 1);
                    for (quint64 w = 0; w < width; w++, entry += 8) {
                        if (entry + 8 > static_cast<size_t>(head.size())) {
                            return;
                        }
                        block(le(head.constData() + entry, 8), childSize, childSize <= maxDirect, 0);
                    }
                }
            };
            if (rows == 0) {
                block(rootAddress, start, true, 0);
            } else {
                block(rootAddress, 0, false, rows);
            }
            return out;
        }

        void layout(Dataset& d, const char * p, size_t size) {
            if (size < 2) {
                return;
            }
            if (p[0] != 3) {
                throw Failure{"a data layout of version " + std::to_string(static_cast<int>(p[0])) + " (only version 3 is read)"};
            }
            d.layout = static_cast<unsigned char>(p[1]);
            if (d.layout == 1 && size >= 18) {
                d.dataAddress = le(p + 2, 8);
                d.dataSize = le(p + 10, 8);
                if (d.dataAddress != undefinedAddress) {
                    d.dataAddress += base;
                }
            } else if (d.layout == 0 && size >= 4) {
                const size_t n = static_cast<size_t>(le(p + 2, 2));
                if (4 + n <= size) {
                    d.compact = QByteArray{p + 4, static_cast<qsizetype>(n)};
                }
            } else if (d.layout == 2 && size >= 11) {
                const int dimensionality = static_cast<unsigned char>(p[2]);
                d.btree = le(p + 3, 8);
                if (d.btree != undefinedAddress) {
                    d.btree += base;
                }
                for (int i = 0; i + 1 < dimensionality && 11 + static_cast<size_t>(i) * 4 + 4 <= size; i++) {
                    d.chunkDims.push_back(le(p + 11 + i * 4, 4));
                }
            }
        }

        void pipeline(Dataset& d, const char * p, size_t size) {
            if (size < 2) {
                return;
            }
            const int version = static_cast<unsigned char>(p[0]);
            const int count = static_cast<unsigned char>(p[1]);
            size_t at = version == 1 ? 8 : 2;
            for (int i = 0; i < count && at + 6 <= size; i++) {
                Filter f;
                f.id = static_cast<int>(le(p + at, 2));
                at += 2;
                size_t nameLength = 0;
                if (version == 1 || f.id >= 256) {
                    nameLength = static_cast<size_t>(le(p + at, 2));
                    at += 2;
                }
                at += 2;   // the flags
                const size_t values = static_cast<size_t>(le(p + at, 2));
                at += 2;
                at += version == 1 ? pad8(nameLength) : nameLength;
                for (size_t v = 0; v < values && at + 4 <= size; v++, at += 4) {
                    f.values.push_back(static_cast<quint32>(le(p + at, 4)));
                }
                if (version == 1 && values % 2 == 1) {
                    at += 4;
                }
                d.filters.push_back(f);
            }
        }

        size_t attribute(Dataset& d, const char * p, size_t size, bool exact = false) {
            if (size < 8) {
                return 0;
            }
            const int version = static_cast<unsigned char>(p[0]);
            size_t nameSize = static_cast<size_t>(le(p + 2, 2)), typeSize = static_cast<size_t>(le(p + 4, 2)), spaceSize = static_cast<size_t>(le(p + 6, 2));
            size_t at = 8;
            if (version == 3) {
                at = 9;   // the name's encoding
            }
            const bool padded = version == 1;
            const auto step = [padded] (size_t n) { return padded ? pad8(n) : n; };
            if (at + step(nameSize) + step(typeSize) + step(spaceSize) > size) {
                return 0;
            }
            Attribute a;
            a.name = std::string{p + at, strnlen(p + at, nameSize)};
            at += step(nameSize);
            a.type = parseDatatype(p + at, typeSize);
            at += step(typeSize);
            a.dims = parseDataspace(p + at, spaceSize);
            at += step(spaceSize);
            size_t dataBytes = size - at;
            if (exact) {   // inside a heap the messages follow one another: the data is as long as the dataspace and the type say
                quint64 n = 1;
                for (const auto dim : a.dims) {
                    n *= dim;
                }
                dataBytes = static_cast<size_t>(n * static_cast<quint64>(std::max(a.type.size, 0)));
                if (at + dataBytes > size) {
                    return 0;
                }
            }
            a.data = QByteArray{p + at, static_cast<qsizetype>(dataBytes)};
            d.attributes.push_back(std::move(a));
            return at + dataBytes;
        }

        void walkGroupTree(quint64 address, quint64 segment) {
            const auto head = source.read(address, 24);
            if (head.size() < 24 || std::memcmp(head.constData(), "TREE", 4) != 0) {
                throw Failure{"a group's B-tree is not there"};
            }
            const int level = static_cast<unsigned char>(head[5]);
            const size_t used = static_cast<size_t>(le(head.constData() + 6, 2));
            const auto body = source.read(address + 24, used * 16 + 8);
            for (size_t i = 0; i < used; i++) {
                const quint64 child = le(body.constData() + 8 + i * 16, 8) + base;
                if (level > 0) {
                    walkGroupTree(child, segment);
                    continue;
                }
                const auto node = source.read(child, 8);
                if (node.size() < 8 || std::memcmp(node.constData(), "SNOD", 4) != 0) {
                    throw Failure{"a group's symbol table node is not there"};
                }
                const size_t symbols = static_cast<size_t>(le(node.constData() + 6, 2));
                const auto entries = source.read(child + 8, symbols * 40);
                for (size_t s = 0; s < symbols; s++) {
                    const quint64 nameOffset = le(entries.constData() + s * 40, 8);
                    const quint64 objectAddress = le(entries.constData() + s * 40 + 8, 8) + base;
                    const auto name = source.read(segment + base + nameOffset, 64);
                    const std::string text{name.constData(), strnlen(name.constData(), static_cast<size_t>(name.size()))};
                    children[text] = objectAddress;
                    namesOf[objectAddress] = text;
                }
            }
        }

        Source source;
        quint64 base{0};
    };

    // ---- the data of a dataset

    void unshuffle(QByteArray& bytes, size_t elementSize) {
        const size_t n = static_cast<size_t>(bytes.size()) / elementSize;
        if (elementSize < 2 || n == 0) {
            return;
        }
        QByteArray out = bytes;
        for (size_t k = 0; k < elementSize; k++) {
            for (size_t i = 0; i < n; i++) {
                out[static_cast<qsizetype>(i * elementSize + k)] = bytes[static_cast<qsizetype>(k * n + i)];
            }
        }
        bytes = out;
    }

    QByteArray unfilter(QByteArray bytes, const Dataset& d, quint32 mask, size_t chunkBytes, size_t elementSize) {
        for (size_t i = d.filters.size(); i-- > 0;) {
            if ((mask >> i) & 1U) {
                continue;
            }
            const auto& f = d.filters[i];
            if (f.id == 1) {   // deflate: Qt's qUncompress wants the size in front, big endian
                QByteArray framed(4, '\0');
                for (int k = 0; k < 4; k++) {
                    framed[k] = static_cast<char>((chunkBytes >> (8 * (3 - k))) & 0xFF);
                }
                framed.append(bytes);
                bytes = qUncompress(framed);
                if (bytes.isEmpty()) {
                    throw Failure{"a chunk did not inflate"};
                }
            } else if (f.id == 2) {   // shuffle
                unshuffle(bytes, f.values.empty() ? elementSize : static_cast<size_t>(f.values[0]));
            } else if (f.id == 3) {   // fletcher32: a checksum after the data
                bytes.chop(4);
            } else {
                throw Failure{"a filter that is not read (" + std::to_string(f.id) + ")"};
            }
        }
        return bytes;
    }

    // the dataset's values, as the bytes of the file's own type and order, in the order of the dimensions
    QByteArray readData(File& file, const Dataset& d) {
        const size_t elementSize = static_cast<size_t>(d.type.size);
        const quint64 total = d.count() * elementSize;
        QByteArray out(static_cast<qsizetype>(total), '\0');
        if (d.layout == 0) {
            std::memcpy(out.data(), d.compact.constData(), std::min<size_t>(static_cast<size_t>(d.compact.size()), static_cast<size_t>(total)));
        } else if (d.layout == 1) {
            if (d.dataAddress != undefinedAddress && total > 0) {
                out = file.read(d.dataAddress, total);
            }
        } else if (d.layout == 2) {
            if (d.btree == undefinedAddress || d.chunkDims.size() != d.dims.size()) {
                return out;
            }
            const size_t rank = d.dims.size();
            quint64 chunkElements = 1;
            for (const auto c : d.chunkDims) {
                chunkElements *= c;
            }
            const size_t chunkBytes = static_cast<size_t>(chunkElements * elementSize);
            struct Chunk {
                std::vector<quint64> offsets;
                quint64 size;
                quint32 mask;
                quint64 address;
            };
            std::vector<Chunk> chunks;
            const size_t keySize = 8 + 8 * (rank + 1);
            std::vector<quint64> pending{d.btree};
            while (!pending.empty()) {
                const quint64 at = pending.back();
                pending.pop_back();
                const auto head = file.read(at, 24);
                if (head.size() < 24 || std::memcmp(head.constData(), "TREE", 4) != 0 || head[4] != 1) {
                    throw Failure{"a chunk index is not a B-tree of version 1"};
                }
                const int level = static_cast<unsigned char>(head[5]);
                const size_t used = static_cast<size_t>(le(head.constData() + 6, 2));
                const auto body = file.read(at + 24, used * (keySize + 8) + keySize);
                for (size_t i = 0; i < used; i++) {
                    const char * key = body.constData() + i * (keySize + 8);
                    const quint64 child = le(key + keySize, 8) + file.baseAddress();
                    if (level > 0) {
                        pending.push_back(child);
                        continue;
                    }
                    Chunk c;
                    c.size = le(key, 4);
                    c.mask = static_cast<quint32>(le(key + 4, 4));
                    for (size_t r = 0; r < rank; r++) {
                        c.offsets.push_back(le(key + 8 + r * 8, 8));
                    }
                    c.address = child;
                    chunks.push_back(std::move(c));
                }
            }
            for (const auto& c : chunks) {
                QByteArray bytes = unfilter(file.read(c.address, c.size), d, c.mask, chunkBytes, elementSize);
                if (static_cast<size_t>(bytes.size()) < chunkBytes) {
                    throw Failure{"a chunk is smaller than it should be"};
                }
                // the rows of the chunk (its last dimension is contiguous) into the whole
                std::vector<quint64> index(rank, 0);
                const quint64 rowLength = d.chunkDims[rank - 1];
                quint64 rows = 1;
                for (size_t r = 0; r + 1 < rank; r++) {
                    rows *= d.chunkDims[r];
                }
                for (quint64 row = 0; row < rows; row++) {
                    quint64 rest = row;
                    bool inside = true;
                    quint64 position = 0;
                    for (size_t r = 0; r + 1 < rank; r++) {
                        const size_t k = rank - 2 - r;   // the dimensions from the one before the last, outward
                        const quint64 within = rest % d.chunkDims[k];
                        rest /= d.chunkDims[k];
                        index[k] = c.offsets[k] + within;
                        if (index[k] >= d.dims[k]) {
                            inside = false;
                        }
                    }
                    if (!inside) {
                        continue;
                    }
                    for (size_t r = 0; r + 1 < rank; r++) {
                        position = position * d.dims[r] + index[r];
                    }
                    position = position * d.dims[rank - 1] + c.offsets[rank - 1];
                    const quint64 length = std::min<quint64>(rowLength, d.dims[rank - 1] - c.offsets[rank - 1]);
                    std::memcpy(out.data() + position * elementSize, bytes.constData() + row * rowLength * elementSize, static_cast<size_t>(length * elementSize));
                }
            }
        }
        return out;
    }

    // ---- the classic NetCDF file (the 64 bit offset format)

    struct OutAttribute {
        std::string name;
        int ncType{2};
        QByteArray values;   // big endian, not padded
        quint32 count{0};
    };
    struct OutVariable {
        std::string name;
        int ncType{5};
        std::vector<quint32> dimensions;
        std::vector<OutAttribute> attributes;
        QByteArray data;   // big endian, not padded
    };

    int ncTypeOf(const Datatype& t, bool& ok) {
        ok = true;
        if (t.cls == 1) {
            return t.size == 4 ? 5 : t.size == 8 ? 6 : (ok = false, 0);
        }
        if (t.cls == 0) {
            return t.size == 1 ? 1 : t.size == 2 ? 3 : t.size == 4 ? 4 : (ok = false, 0);
        }
        if (t.cls == 3) {
            return 2;
        }
        ok = false;
        return 0;
    }

    QByteArray toBigEndian(QByteArray bytes, const Datatype& t) {
        if (!t.bigEndian && t.size > 1 && t.cls != 3) {
            for (qsizetype i = 0; i + t.size <= bytes.size(); i += t.size) {
                std::reverse(bytes.begin() + i, bytes.begin() + i + t.size);
            }
        }
        return bytes;
    }

    void put32(QByteArray& out, quint32 v) {
        for (int i = 3; i >= 0; i--) {
            out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
        }
    }
    void put64(QByteArray& out, quint64 v) {
        for (int i = 7; i >= 0; i--) {
            out.append(static_cast<char>((v >> (8 * i)) & 0xFF));
        }
    }
    void padTo4(QByteArray& out) {
        while (out.size() % 4 != 0) {
            out.append('\0');
        }
    }
    void putName(QByteArray& out, const std::string& name) {
        put32(out, static_cast<quint32>(name.size()));
        out.append(name.data(), static_cast<qsizetype>(name.size()));
        padTo4(out);
    }
    void putAttributes(QByteArray& out, const std::vector<OutAttribute>& list) {
        if (list.empty()) {
            put32(out, 0);
            put32(out, 0);
            return;
        }
        put32(out, 0x0C);
        put32(out, static_cast<quint32>(list.size()));
        for (const auto& a : list) {
            putName(out, a.name);
            put32(out, static_cast<quint32>(a.ncType));
            put32(out, a.count);
            out.append(a.values);
            padTo4(out);
        }
    }

    bool attributeToOut(const Attribute& a, OutAttribute& out) {
        bool ok = false;
        const int type = ncTypeOf(a.type, ok);
        if (!ok) {
            return false;
        }
        out.name = a.name;
        out.ncType = type;
        if (a.type.cls == 3) {   // text: the characters, without the padding of nulls at the end
            QByteArray text = a.data;
            const auto zero = text.indexOf('\0');
            if (zero >= 0) {
                text.truncate(zero);
            }
            out.values = text;
            out.count = static_cast<quint32>(text.size());
            return out.count > 0;
        }
        quint64 n = 1;
        for (const auto d : a.dims) {
            n *= d;
        }
        const qsizetype bytes = static_cast<qsizetype>(n * static_cast<quint64>(a.type.size));
        if (a.data.size() < bytes) {
            return false;
        }
        out.values = toBigEndian(a.data.left(bytes), a.type);
        out.count = static_cast<quint32>(n);
        return true;
    }

    // GDAL's NetCDF driver gives longitudes that run past 180 as west ones (360 less) when it reads a file whole; the 2D longitudes it places a rotated grid by are kept that way here (big endian floats)
    void westLongitudes(QByteArray& bigEndianFloats) {
        const auto n = static_cast<size_t>(bigEndianFloats.size() / 4);
        const auto value = [&bigEndianFloats] (size_t i) {
            quint32 w = 0;
            std::memcpy(&w, bigEndianFloats.constData() + i * 4, 4);
            w = ((w & 0xFF) << 24) | ((w & 0xFF00) << 8) | ((w >> 8) & 0xFF00) | (w >> 24);
            float f;
            std::memcpy(&f, &w, 4);
            return f;
        };
        bool past = false;
        for (size_t i = 0; i < n && !past; i++) {
            past = value(i) > 180.0f;
        }
        if (!past) {
            return;
        }
        for (size_t i = 0; i < n; i++) {
            float f = value(i) - 360.0f;
            quint32 w;
            std::memcpy(&w, &f, 4);
            w = ((w & 0xFF) << 24) | ((w & 0xFF00) << 8) | ((w >> 8) & 0xFF00) | (w >> 24);
            std::memcpy(bigEndianFloats.data() + i * 4, &w, 4);
        }
    }

    bool skipped(const std::string& name) {
        return name == "CLASS" || name == "NAME" || name == "DIMENSION_LIST" || name == "REFERENCE_LIST" || name == "_Netcdf4Dimid" || name == "_Netcdf4Coordinates" || name == "_NCProperties" || name == "coordinates";
    }
}

bool Hdf5Range::extract(const Fetch& fetch, const std::string& variable, const QString& outPath, std::string& error, const QString& cacheFolder) {
    try {
        File file{fetch};
        file.listRoot();
        const auto found = file.children.find(variable);
        if (found == file.children.end()) {
            throw Failure{"the file has no variable " + variable};
        }
        Dataset main = file.readObject(variable, found->second);
        bool ok = false;
        const int mainType = ncTypeOf(main.type, ok);
        if (!ok || main.layout < 0) {
            throw Failure{"the variable " + variable + " is not of a type that is read"};
        }
        std::vector<std::pair<std::string, quint32>> dimensions;   // name, size
        std::vector<OutVariable> variables;
        const auto dimensionOf = [&dimensions] (const std::string& name, quint64 size) {
            for (size_t i = 0; i < dimensions.size(); i++) {
                if (dimensions[i].first == name) {
                    return static_cast<quint32>(i);
                }
            }
            dimensions.push_back({name, static_cast<quint32>(size)});
            return static_cast<quint32>(dimensions.size() - 1);
        };
        bool rotated = false;   // a rotated pole grid: GDAL places it by the 2D latitude and longitude
        QByteArray gridKey;     // what tells the grid: its coordinates and the mapping's attributes
        const auto makeVariable = [&] (const Dataset& d, const std::vector<std::string>& dimensionNames, const QString& cacheName = QString{}) {
            OutVariable v;
            v.name = d.name;
            bool typeOk = false;
            v.ncType = ncTypeOf(d.type, typeOk);
            if (!typeOk) {
                throw Failure{"the variable " + d.name + " is not of a type that is read"};
            }
            for (size_t i = 0; i < d.dims.size(); i++) {
                v.dimensions.push_back(dimensionOf(dimensionNames[i], d.dims[i]));
            }
            for (const auto& a : d.attributes) {
                OutAttribute out;
                if ((!skipped(a.name) || (a.name == "coordinates" && rotated)) && !(a.name.size() > 0 && a.name[0] == '_' && a.name != "_FillValue" && a.name != "_Unsigned") && attributeToOut(a, out)) {
                    v.attributes.push_back(std::move(out));
                }
            }
            const QString cachePath = cacheName.isEmpty() || cacheFolder.isEmpty() ? QString{} : cacheFolder + "/hdf5coord_" + cacheName + ".bin";
            if (!cachePath.isEmpty()) {
                QFile cached{cachePath};
                if (cached.open(QIODevice::ReadOnly)) {
                    v.data = cached.readAll();
                }
            }
            if (v.data.isEmpty()) {
                v.data = toBigEndian(readData(file, d), d.type);
                if (!cacheName.isEmpty() && cacheName.endsWith("_longitude") && d.type.cls == 1 && d.type.size == 4) {
                    westLongitudes(v.data);
                }
                if (!cachePath.isEmpty()) {
                    QSaveFile keep{cachePath};
                    if (keep.open(QIODevice::WriteOnly)) {
                        keep.write(v.data);
                        keep.commit();
                    }
                }
            }
            return v;
        };
        // the dimensions' names: the coordinate datasets the DIMENSION_LIST refers to (or dim0, dim1 ... when there is none)
        std::vector<std::string> names;
        std::vector<Dataset> scales;
        const auto * list = main.attribute("DIMENSION_LIST");
        for (size_t i = 0; i < main.dims.size(); i++) {
            std::string name = "dim" + std::to_string(i);
            if (list != nullptr) {
                const auto address = file.dimensionScale(*list, i);
                const auto known = file.namesOf.find(address);
                if (known != file.namesOf.end()) {
                    name = known->second;
                    scales.push_back(file.readObject(name, address));
                }
            }
            names.push_back(name);
        }
        // the dimensions first (in the order of the variable), then the coordinate variables that are variables, the grid mapping, and the variable
        for (size_t i = 0; i < names.size(); i++) {
            dimensionOf(names[i], main.dims[i]);
        }
        for (const auto& scale : scales) {
            const auto * attrName = scale.attribute("NAME");
            if (attrName != nullptr && QByteArray{attrName->data}.startsWith("This is a netCDF dimension but not a netCDF variable")) {
                continue;
            }
            if (scale.dims.size() == 1 && scale.layout >= 0) {
                variables.push_back(makeVariable(scale, {scale.name}));
            }
        }
        if (const auto * mapping = main.attribute("grid_mapping")) {
            const std::string name{mapping->data.constData(), strnlen(mapping->data.constData(), static_cast<size_t>(mapping->data.size()))};
            const auto at = file.children.find(name);
            if (at != file.children.end()) {
                const auto grid = file.readObject(name, at->second);
                if (grid.dims.empty()) {
                    if (const auto * kind = grid.attribute("grid_mapping_name")) {
                        rotated = QByteArray{kind->data}.startsWith("rotated_latitude_longitude");
                    }
                    for (const auto& a : grid.attributes) {
                        gridKey.append(a.name.c_str()).append(a.data);
                    }
                    variables.push_back(makeVariable(grid, {}));
                }
            }
        }
        if (rotated) {   // the coordinates of the grid (small) tell which grid it is, for the cache of its latitude and longitude
            for (const auto& v : variables) {
                if (v.dimensions.size() == 1) {
                    gridKey.append(v.name.c_str()).append(v.data);
                }
            }
        }
        const QString gridHash = QString::fromLatin1(QCryptographicHash::hash(gridKey, QCryptographicHash::Sha1).toHex());
        if (rotated) {
            if (const auto * coordinates = main.attribute("coordinates")) {
                const auto words = QString::fromLatin1(coordinates->data.constData(), static_cast<qsizetype>(strnlen(coordinates->data.constData(), static_cast<size_t>(coordinates->data.size())))).split(' ', Qt::SkipEmptyParts);
                for (const auto& word : words) {
                    const auto at = file.children.find(word.toStdString());
                    if (at == file.children.end() || word == "time") {
                        continue;
                    }
                    const auto coordinate = file.readObject(word.toStdString(), at->second);
                    if (coordinate.dims == main.dims && coordinate.layout >= 0) {
                        variables.push_back(makeVariable(coordinate, names, gridHash + "_" + word));
                    }
                }
            }
        }
        variables.push_back(makeVariable(main, names));
        (void) mainType;
        // the header
        const auto buildHeader = [&] (const std::vector<quint64>& begins) {
            QByteArray h;
            h.append("CDF\x02", 4);
            put32(h, 0);   // no records
            if (dimensions.empty()) {
                put32(h, 0);
                put32(h, 0);
            } else {
                put32(h, 0x0A);
                put32(h, static_cast<quint32>(dimensions.size()));
                for (const auto& d : dimensions) {
                    putName(h, d.first);
                    put32(h, d.second);
                }
            }
            std::vector<OutAttribute> global;
            if (const auto * conventions = file.root.attribute("Conventions")) {
                OutAttribute a;
                if (attributeToOut(*conventions, a)) {
                    global.push_back(a);
                }
            }
            putAttributes(h, global);
            put32(h, 0x0B);
            put32(h, static_cast<quint32>(variables.size()));
            for (size_t i = 0; i < variables.size(); i++) {
                const auto& v = variables[i];
                putName(h, v.name);
                put32(h, static_cast<quint32>(v.dimensions.size()));
                for (const auto d : v.dimensions) {
                    put32(h, d);
                }
                putAttributes(h, v.attributes);
                put32(h, static_cast<quint32>(v.ncType));
                put32(h, static_cast<quint32>((v.data.size() + 3) & ~3));
                put64(h, begins[i]);
            }
            return h;
        };
        const size_t headerSize = static_cast<size_t>(buildHeader(std::vector<quint64>(variables.size(), 0)).size());
        std::vector<quint64> begins;
        quint64 at = headerSize;
        for (const auto& v : variables) {
            begins.push_back(at);
            at += static_cast<quint64>((v.data.size() + 3) & ~3);
        }
        QSaveFile out{outPath};
        if (!out.open(QIODevice::WriteOnly)) {
            throw Failure{"the file " + outPath.toStdString() + " cannot be written"};
        }
        out.write(buildHeader(begins));
        for (const auto& v : variables) {
            QByteArray data = v.data;
            padTo4(data);
            out.write(data);
        }
        if (!out.commit()) {
            throw Failure{"the file " + outPath.toStdString() + " cannot be written"};
        }
        return true;
    } catch (const Failure& failure) {
        error = failure.why;
        return false;
    }
}
