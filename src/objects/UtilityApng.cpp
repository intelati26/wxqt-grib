// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityApng.h"
#include <array>
#include <cstdint>
#include <QBuffer>
#include <QImage>

namespace {
    // standard PNG / zlib CRC-32
    uint32_t crc32(const QByteArray& data) {
        static std::array<uint32_t, 256> table = [] {
            std::array<uint32_t, 256> t{};
            for (uint32_t n = 0; n < 256; n += 1) {
                uint32_t c = n;
                for (int k = 0; k < 8; k += 1) {
                    c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
                }
                t[n] = c;
            }
            return t;
        }();
        uint32_t c = 0xFFFFFFFFu;
        for (const auto byte : data) {
            c = table[(c ^ static_cast<uint8_t>(byte)) & 0xFF] ^ (c >> 8);
        }
        return c ^ 0xFFFFFFFFu;
    }

    void appendU32(QByteArray& out, uint32_t value) {
        out.append(static_cast<char>((value >> 24) & 0xFF));
        out.append(static_cast<char>((value >> 16) & 0xFF));
        out.append(static_cast<char>((value >> 8) & 0xFF));
        out.append(static_cast<char>(value & 0xFF));
    }

    void appendU16(QByteArray& out, uint16_t value) {
        out.append(static_cast<char>((value >> 8) & 0xFF));
        out.append(static_cast<char>(value & 0xFF));
    }

    uint32_t readU32(const QByteArray& data, int offset) {
        return (static_cast<uint8_t>(data[offset]) << 24)
             | (static_cast<uint8_t>(data[offset + 1]) << 16)
             | (static_cast<uint8_t>(data[offset + 2]) << 8)
             | static_cast<uint8_t>(data[offset + 3]);
    }

    // write one PNG chunk (length, type, data, crc)
    void appendChunk(QByteArray& out, const char* type, const QByteArray& payload) {
        appendU32(out, static_cast<uint32_t>(payload.size()));
        QByteArray typed{type, 4};
        QByteArray body = typed + payload;
        out.append(body);
        appendU32(out, crc32(body));
    }

    const std::array<char, 8> pngSignature{
        static_cast<char>(0x89), 'P', 'N', 'G', '\r', '\n', static_cast<char>(0x1A), '\n'};

    // re-encode an image to a canonical ARGB32 PNG at the requested size
    QByteArray encodeCanonical(const QImage& source, const QSize& size) {
        QImage frame = source.convertToFormat(QImage::Format_ARGB32);
        if (frame.size() != size) {
            frame = frame.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        QByteArray bytes;
        QBuffer buffer{&bytes};
        buffer.open(QIODevice::WriteOnly);
        frame.save(&buffer, "PNG");
        return bytes;
    }
}

QByteArray UtilityApng::fromFrames(const std::vector<QByteArray>& frames, int delayMs) {
    std::vector<QImage> images;
    for (const auto& bytes : frames) {
        QImage image = QImage::fromData(bytes);
        if (!image.isNull()) {
            images.push_back(image);
        }
    }
    if (images.size() < 2) {
        return {};
    }
    const auto size = images.front().size();

    // pull IHDR + IDAT payloads out of each canonical PNG
    struct Decoded {
        QByteArray ihdr;
        std::vector<QByteArray> idat;
    };
    std::vector<Decoded> decoded;
    for (const auto& image : images) {
        const auto png = encodeCanonical(image, size);
        if (png.size() < 8 || !std::equal(pngSignature.begin(), pngSignature.end(), png.constData())) {
            return {};
        }
        Decoded entry;
        int offset = 8;
        while (offset + 8 <= png.size()) {
            const auto length = static_cast<int>(readU32(png, offset));
            const auto type = QByteArray{png.constData() + offset + 4, 4};
            const auto dataOffset = offset + 8;
            if (dataOffset + length + 4 > png.size()) {
                break;
            }
            if (type == "IHDR") {
                entry.ihdr = QByteArray{png.constData() + dataOffset, length};
            } else if (type == "IDAT") {
                entry.idat.push_back(QByteArray{png.constData() + dataOffset, length});
            }
            offset = dataOffset + length + 4;
        }
        if (entry.ihdr.isEmpty() || entry.idat.empty()) {
            return {};
        }
        decoded.push_back(std::move(entry));
    }

    QByteArray out;
    out.append(pngSignature.data(), static_cast<int>(pngSignature.size()));
    appendChunk(out, "IHDR", decoded.front().ihdr);

    QByteArray actl;
    appendU32(actl, static_cast<uint32_t>(decoded.size()));   // num_frames
    appendU32(actl, 0);                                       // num_plays (loop forever)
    appendChunk(out, "acTL", actl);

    uint32_t sequence = 0;
    auto appendFctl = [&] {
        QByteArray fctl;
        appendU32(fctl, sequence);
        sequence += 1;
        appendU32(fctl, static_cast<uint32_t>(size.width()));
        appendU32(fctl, static_cast<uint32_t>(size.height()));
        appendU32(fctl, 0);                                   // x_offset
        appendU32(fctl, 0);                                   // y_offset
        appendU16(fctl, static_cast<uint16_t>(delayMs));      // delay_num
        appendU16(fctl, 1000);                                // delay_den
        fctl.append(static_cast<char>(0));                    // dispose_op = NONE
        fctl.append(static_cast<char>(0));                    // blend_op = SOURCE
        appendChunk(out, "fcTL", fctl);
    };

    // frame 0: fcTL then the real IDAT chunks
    appendFctl();
    for (const auto& idat : decoded.front().idat) {
        appendChunk(out, "IDAT", idat);
    }

    // later frames: fcTL then fdAT (sequence number + IDAT payload)
    for (size_t i = 1; i < decoded.size(); i += 1) {
        appendFctl();
        for (const auto& idat : decoded[i].idat) {
            QByteArray fdat;
            appendU32(fdat, sequence);
            sequence += 1;
            fdat.append(idat);
            appendChunk(out, "fdAT", fdat);
        }
    }

    appendChunk(out, "IEND", QByteArray{});
    return out;
}
