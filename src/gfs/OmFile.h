// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OMFILE_H
#define OMFILE_H

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

// A reader of Open-Meteo's .om files (open-meteo/om-file-format, the C decoder in src/external/omfile): the file is read by byte ranges, so a variable of a 100 MB
// file costs the few hundred kilobytes it holds. The file's end says where its root is, the root's children are the variables, and a read of a region fetches the lookup table
// and then the chunks that region touches.
class OmFile {
public:
    // The two reads the file needs, supplied by the caller (a url and an HTTP range, or a file).
    struct Io {
        std::function<bool(uint64_t offset, uint64_t size, std::string& out)> range;   // `size` bytes from `offset` (fewer at the end of the file are an error)
        std::function<bool(uint64_t size, std::string& out)> tail;                      // the last `size` bytes
        // How reads are merged: chunks closer than `merge` bytes are read in one request, and one request is no larger than `max`. The decoder's defaults (512 and 64 KB) are for a local file; for
        // a url a request costs far more than the bytes between chunks.
        uint64_t merge{512};
        uint64_t max{65536};
    };
    struct Variable {
        std::string name;
        std::vector<uint64_t> dimensions;
        std::vector<uint64_t> chunks;
        std::string meta;                 // the variable's metadata, which the decoder points into
    };

    // Reads the end of the file and the variables' metadata (two or three requests).
    bool open(const Io& io, std::string& error);
    bool has(const std::string& name) const { return variables_.count(name) > 0; }
    const Variable* variable(const std::string& name) const;
    std::vector<std::string> names() const;
    // A region of a float variable: `offset` and `count` for each dimension. The values come back in the region's own row-major order, NaN where the file has none.
    bool read(const std::string& name, const std::vector<uint64_t>& offset, const std::vector<uint64_t>& count, std::vector<float>& out, std::string& error) const;
    // The whole variable.
    bool readAll(const std::string& name, std::vector<float>& out, std::string& error) const;

private:
    Io io_;
    std::map<std::string, Variable> variables_;
};

#endif  // OMFILE_H
