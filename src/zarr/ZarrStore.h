// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ZARRSTORE_H
#define ZARRSTORE_H

#include <map>
#include <string>
#include <vector>
#include <QJsonObject>

// A read-only Zarr (version 2) store served over HTTP, as SPC's REFS data is: the consolidated metadata (.zmetadata) is
// read once, then each chunk is one file ("<array>/<i>.<j>.<k>") that is downloaded, blosc-decoded (Blosc.h) and turned
// into floats. The "quantize" filter is lossy at write time and does nothing when reading.
class ZarrStore {
public:
    struct Array {
        std::vector<int> shape;
        std::vector<int> chunks;
        std::string dtype;            // "<f2", "<f4", "<f8", "<i4", "<i8", "|u1", "<U9", ...
        double fillValue{0.0};        // NaN when the store says so
        bool hasFill{false};
        bool compressed{false};       // a blosc compressor (otherwise the chunk is the raw bytes)
        std::vector<std::string> dims;
        QJsonObject attrs;
    };

    // reads <baseUrl>/.zmetadata; false with a reason in `error`
    bool open(const std::string& baseUrl, std::string& error);

    const Array * array(const std::string& name) const;
    std::vector<std::string> arrayNames() const;
    const QJsonObject& groupAttributes() const { return groupAttrs; }
    const std::string& baseUrl() const { return base; }

    // One chunk as floats in C order (chunk shape of the array). A chunk the server does not have (404) is filled with the
    // array's fill value. `status` is the HTTP status of the download (0: no response).
    bool readChunk(const std::string& name, const std::vector<int>& chunkIndex, std::vector<float>& out, std::string& error) const;
    // a whole small 1-D array (a time or member coordinate) of numbers; every chunk is fetched
    bool readNumbers(const std::string& name, std::vector<double>& out, std::string& error) const;
    // a 1-D array of fixed-width unicode strings (member names)
    bool readStrings(const std::string& name, std::vector<std::string>& out, std::string& error) const;

    static float halfToFloat(unsigned short h);

private:
    bool fetchDecoded(const std::string& name, const std::vector<int>& chunkIndex, const Array& a, std::vector<unsigned char>& bytes,
                      bool& missing, std::string& error) const;
    std::string base;
    std::map<std::string, Array> arrays;
    QJsonObject groupAttrs;
};

#endif  // ZARRSTORE_H
