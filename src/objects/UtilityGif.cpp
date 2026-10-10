// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityGif.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <QPainter>

namespace {
    // a 15 bit color (5 bits each of red, green and blue) with how many pixels have it
    struct Bin {
        int red, green, blue;   // 0..31
        std::uint64_t count;
    };

    struct Box {
        std::vector<Bin> bins;
        int range(int channel, int& low, int& high) const {
            low = 31;
            high = 0;
            for (const auto& b : bins) {
                const int v = channel == 0 ? b.red : channel == 1 ? b.green : b.blue;
                low = std::min(low, v);
                high = std::max(high, v);
            }
            return high - low;
        }
    };

    // median cut: split the box with the widest spread at the weighted median until there are `colors` boxes
    std::vector<std::array<std::uint8_t, 3>> medianCut(const std::vector<Bin>& histogram, int colors) {
        std::vector<Box> boxes(1);
        boxes[0].bins = histogram;
        while (static_cast<int>(boxes.size()) < colors) {
            size_t pick = boxes.size();
            int widest = 0, channel = 0;
            for (size_t i = 0; i < boxes.size(); i++) {
                if (boxes[i].bins.size() < 2) {
                    continue;
                }
                for (int c = 0; c < 3; c++) {
                    int low, high;
                    const int spread = boxes[i].range(c, low, high);
                    if (spread > widest) {
                        widest = spread;
                        channel = c;
                        pick = i;
                    }
                }
            }
            if (pick == boxes.size()) {
                break;   // every box is one color
            }
            auto& box = boxes[pick];
            std::sort(box.bins.begin(), box.bins.end(), [channel] (const Bin& a, const Bin& b) {
                return (channel == 0 ? a.red : channel == 1 ? a.green : a.blue) < (channel == 0 ? b.red : channel == 1 ? b.green : b.blue);
            });
            std::uint64_t total = 0;
            for (const auto& b : box.bins) {
                total += b.count;
            }
            std::uint64_t running = 0;
            size_t cut = 1;
            for (size_t i = 0; i < box.bins.size(); i++) {
                running += box.bins[i].count;
                if (running * 2 >= total) {
                    cut = std::clamp<size_t>(i + 1, 1, box.bins.size() - 1);
                    break;
                }
            }
            Box other;
            other.bins.assign(box.bins.begin() + static_cast<std::ptrdiff_t>(cut), box.bins.end());
            box.bins.resize(cut);
            boxes.push_back(std::move(other));
        }
        std::vector<std::array<std::uint8_t, 3>> palette;
        for (const auto& box : boxes) {
            double r = 0, g = 0, b = 0, n = 0;
            for (const auto& bin : box.bins) {
                r += bin.red * static_cast<double>(bin.count);
                g += bin.green * static_cast<double>(bin.count);
                b += bin.blue * static_cast<double>(bin.count);
                n += static_cast<double>(bin.count);
            }
            if (n <= 0) {
                continue;
            }
            const auto expand = [n] (double sum) { return static_cast<std::uint8_t>(std::clamp(std::lround(sum / n * 255.0 / 31.0), 0L, 255L)); };
            palette.push_back({expand(r), expand(g), expand(b)});
        }
        return palette;
    }

    // GIF's variable width LZW, minimum code size 8, a clear code when the 4096 entry table is full
    class LzwWriter {
    public:
        explicit LzwWriter(QByteArray& out) : out{out} {}
        void encode(const std::uint8_t * pixels, size_t count) {
            const int clear = 256, end = 257;
            int width = 9, next = 258;
            std::unordered_map<std::uint32_t, int> table;   // (prefix << 8 | byte) -> code
            put(clear, width);
            if (count == 0) {
                put(end, width);
                flush();
                return;
            }
            int prefix = pixels[0];
            for (size_t i = 1; i < count; i++) {
                const std::uint32_t key = (static_cast<std::uint32_t>(prefix) << 8) | pixels[i];
                const auto found = table.find(key);
                if (found != table.end()) {
                    prefix = found->second;
                    continue;
                }
                put(prefix, width);
                if (next < 4096) {
                    table[key] = next++;
                    if (next - 1 == (1 << width) && width < 12) {   // the code just made needs another bit from the next one on
                        width++;
                    }
                } else {
                    put(clear, width);
                    table.clear();
                    width = 9;
                    next = 258;
                }
                prefix = pixels[i];
            }
            put(prefix, width);
            put(end, width);
            flush();
        }

    private:
        void put(int code, int width) {
            bits |= static_cast<std::uint32_t>(code) << used;
            used += width;
            while (used >= 8) {
                block.push_back(static_cast<char>(bits & 0xFF));
                bits >>= 8;
                used -= 8;
                if (block.size() == 255) {
                    writeBlock();
                }
            }
        }
        void writeBlock() {
            out.append(static_cast<char>(block.size()));
            out.append(block.data(), static_cast<qsizetype>(block.size()));
            block.clear();
        }
        void flush() {
            if (used > 0) {
                block.push_back(static_cast<char>(bits & 0xFF));
                bits = 0;
                used = 0;
            }
            if (!block.empty()) {
                writeBlock();
            }
            out.append('\0');   // the empty block that ends the image data
        }
        QByteArray& out;
        std::string block;
        std::uint32_t bits{0};
        int used{0};
    };

    void put16(QByteArray& out, int value) {
        out.append(static_cast<char>(value & 0xFF));
        out.append(static_cast<char>((value >> 8) & 0xFF));
    }
}

QByteArray UtilityGif::fromImages(const std::vector<QImage>& frames, int delayMs) {
    if (frames.empty() || frames.front().isNull()) {
        return {};
    }
    const int width = frames.front().width(), height = frames.front().height();
    if (width > 65535 || height > 65535) {
        return {};
    }
    // every frame as opaque RGB32 of the first frame's size
    std::vector<QImage> rgb;
    for (const auto& frame : frames) {
        QImage flat{width, height, QImage::Format_RGB32};
        flat.fill(Qt::white);
        QPainter painter{&flat};
        painter.drawImage(0, 0, frame.width() == width && frame.height() == height ? frame : frame.scaled(width, height, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        painter.end();
        rgb.push_back(std::move(flat));
    }
    // the colors in use over all the frames (every second pixel of every second row of the later frames: the first is read whole)
    std::vector<std::uint64_t> counts(32768, 0);
    for (size_t f = 0; f < rgb.size(); f++) {
        const int step = f == 0 ? 1 : 2;
        for (int y = 0; y < height; y += step) {
            const auto * line = reinterpret_cast<const QRgb *>(rgb[f].constScanLine(y));
            for (int x = 0; x < width; x += step) {
                const QRgb c = line[x];
                counts[static_cast<size_t>(((qRed(c) >> 3) << 10) | ((qGreen(c) >> 3) << 5) | (qBlue(c) >> 3))]++;
            }
        }
    }
    std::vector<Bin> histogram;
    for (size_t i = 0; i < counts.size(); i++) {
        if (counts[i] > 0) {
            histogram.push_back({static_cast<int>(i >> 10), static_cast<int>((i >> 5) & 31), static_cast<int>(i & 31), counts[i]});
        }
    }
    auto palette = medianCut(histogram, 256);
    if (palette.empty()) {
        palette.push_back({255, 255, 255});
    }
    // every 15 bit color's nearest palette entry, made when first met
    std::vector<int> lookup(32768, -1);
    const auto indexOf = [&] (QRgb c) {
        const auto key = static_cast<size_t>(((qRed(c) >> 3) << 10) | ((qGreen(c) >> 3) << 5) | (qBlue(c) >> 3));
        if (lookup[key] >= 0) {
            return lookup[key];
        }
        const int r = qRed(c) & 0xF8, g = qGreen(c) & 0xF8, b = qBlue(c) & 0xF8;
        int best = 0;
        long bestDistance = -1;
        for (size_t i = 0; i < palette.size(); i++) {
            const long dr = r - palette[i][0], dg = g - palette[i][1], db = b - palette[i][2];
            const long d = dr * dr * 2 + dg * dg * 4 + db * db * 3;   // the eye is most sensitive to green
            if (bestDistance < 0 || d < bestDistance) {
                bestDistance = d;
                best = static_cast<int>(i);
            }
        }
        lookup[key] = best;
        return best;
    };

    QByteArray out;
    out.append("GIF89a");
    put16(out, width);
    put16(out, height);
    out.append(static_cast<char>(0xF7));   // a global table of 256 entries, 8 bits of color
    out.append('\0');                      // background color index
    out.append('\0');                      // square pixels
    for (int i = 0; i < 256; i++) {
        const auto& c = palette[std::min<size_t>(static_cast<size_t>(i), palette.size() - 1)];
        out.append(static_cast<char>(c[0]));
        out.append(static_cast<char>(c[1]));
        out.append(static_cast<char>(c[2]));
    }
    // play forever
    out.append("\x21\xFF\x0B" "NETSCAPE2.0" "\x03\x01\x00\x00\x00", 3 + 11 + 5);
    const int delay = std::max(2, (delayMs + 5) / 10);
    std::vector<std::uint8_t> previous(static_cast<size_t>(width) * static_cast<size_t>(height), 0);
    std::vector<std::uint8_t> current(previous.size(), 0);
    for (size_t f = 0; f < rgb.size(); f++) {
        for (int y = 0; y < height; y++) {
            const auto * line = reinterpret_cast<const QRgb *>(rgb[f].constScanLine(y));
            auto * row = current.data() + static_cast<size_t>(y) * static_cast<size_t>(width);
            for (int x = 0; x < width; x++) {
                row[x] = static_cast<std::uint8_t>(indexOf(line[x]));
            }
        }
        // the rectangle that differs from the picture on screen (the first frame is the whole picture)
        int left = 0, top = 0, right = width - 1, bottom = height - 1;
        if (f > 0) {
            left = width;
            top = height;
            right = -1;
            bottom = -1;
            for (int y = 0; y < height; y++) {
                const auto * a = current.data() + static_cast<size_t>(y) * static_cast<size_t>(width);
                const auto * b = previous.data() + static_cast<size_t>(y) * static_cast<size_t>(width);
                for (int x = 0; x < width; x++) {
                    if (a[x] != b[x]) {
                        left = std::min(left, x);
                        right = std::max(right, x);
                        top = std::min(top, y);
                        bottom = std::max(bottom, y);
                    }
                }
            }
            if (right < left) {   // nothing changed: one pixel keeps the delay
                left = right = 0;
                top = bottom = 0;
            }
        }
        out.append("\x21\xF9\x04\x04", 4);   // graphic control: leave in place, no transparency
        put16(out, delay);
        out.append('\0');
        out.append('\0');
        out.append('\x2C');
        put16(out, left);
        put16(out, top);
        put16(out, right - left + 1);
        put16(out, bottom - top + 1);
        out.append('\0');   // no local table
        out.append(static_cast<char>(8));   // LZW minimum code size
        std::vector<std::uint8_t> sub;
        sub.reserve(static_cast<size_t>(right - left + 1) * static_cast<size_t>(bottom - top + 1));
        for (int y = top; y <= bottom; y++) {
            const auto * row = current.data() + static_cast<size_t>(y) * static_cast<size_t>(width);
            sub.insert(sub.end(), row + left, row + right + 1);
        }
        LzwWriter{out}.encode(sub.data(), sub.size());
        previous = current;
    }
    out.append('\x3B');
    return out;
}
