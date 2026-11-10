// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/OmFile.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

extern "C" {
#include "external/omfile/om_decoder.h"
#include "external/omfile/om_file.h"
#include "external/omfile/om_variable.h"
}

namespace {
    // what a file's own numbers are held to (they come from the server): the root, the count of variables, the values of one read (1 GB of floats) and one range of bytes
    const uint64_t maxMeta = 1u << 24;
    const uint32_t maxVariables = 100000;
    const uint64_t maxValues = 256ull * 1024 * 1024;
    const uint64_t maxRange = 512ull * 1024 * 1024;
}

bool OmFile::open(const Io& io, std::string& error) {
    io_ = io;
    variables_.clear();
    std::string tail;
    if (!io.tail(om_trailer_size(), tail) || tail.size() < om_trailer_size()) {
        error = "could not read the end of the file";
        return false;
    }
    uint64_t rootOffset = 0, rootSize = 0;
    if (!om_trailer_read(tail.data(), &rootOffset, &rootSize) || rootSize == 0 || rootSize > (1u << 24)) {
        error = "not an Open-Meteo file (or an older kind)";
        return false;
    }
    if (rootSize > maxMeta) {   // (the sizes are the file's own word: none is believed past what such a file can have)
        error = "the file's root has a size that can not be right";
        return false;
    }
    std::string root;
    if (!io.range(rootOffset, rootSize, root) || root.size() < rootSize) {
        error = "could not read the file's root";
        return false;
    }
    const auto * rootVariable = om_variable_init(root.data());
    const uint32_t count = om_variable_get_children_count(rootVariable);
    if (count > maxVariables) {
        error = "the file names more variables than a file can have";
        return false;
    }
    std::vector<uint64_t> offsets(count), sizes(count);
    if (count == 0 || !om_variable_get_children(rootVariable, 0, count, offsets.data(), sizes.data())) {
        error = "the file has no variables";
        return false;
    }
    // the children are written one after another: one read for all of their metadata
    uint64_t first = std::numeric_limits<uint64_t>::max(), last = 0;
    for (uint32_t i = 0; i < count; i++) {
        first = std::min(first, offsets[i]);
        last = std::max(last, offsets[i] + sizes[i]);
    }
    std::string span;
    if (last - first > (64u << 20) || !io.range(first, last - first, span) || span.size() < last - first) {
        error = "could not read the variables' metadata";
        return false;
    }
    for (uint32_t i = 0; i < count; i++) {
        Variable v;
        v.meta = span.substr(offsets[i] - first, sizes[i]);
        if (om_variable_validate(v.meta.data(), v.meta.size()) != ERROR_OK) {
            continue;
        }
        const auto * variable = om_variable_init(v.meta.data());
        if (om_variable_get_type(variable) != DATA_TYPE_FLOAT_ARRAY) {   // the coordinate system and other text are not fields
            continue;
        }
        uint16_t length = 0;
        const char * name = om_variable_get_name(variable, &length);
        v.name.assign(name, length);
        const uint64_t n = om_variable_get_dimensions_count(variable);
        v.dimensions.assign(om_variable_get_dimensions(variable), om_variable_get_dimensions(variable) + n);
        v.chunks.assign(om_variable_get_chunks(variable), om_variable_get_chunks(variable) + n);
        variables_[v.name] = std::move(v);
    }
    if (variables_.empty()) {
        error = "the file has no float arrays";
        return false;
    }
    return true;
}

const OmFile::Variable * OmFile::variable(const std::string& name) const {
    const auto it = variables_.find(name);
    return it == variables_.end() ? nullptr : &it->second;
}

std::vector<std::string> OmFile::names() const {
    std::vector<std::string> out;
    for (const auto& [name, v] : variables_) {
        out.push_back(name);
    }
    return out;
}

bool OmFile::read(const std::string& name, const std::vector<uint64_t>& offset, const std::vector<uint64_t>& count, std::vector<float>& out, std::string& error) const {
    const auto * v = variable(name);
    if (!v) {
        error = "no variable " + name;
        return false;
    }
    if (offset.size() != v->dimensions.size() || count.size() != offset.size()) {
        error = name + " has " + std::to_string(v->dimensions.size()) + " dimensions";
        return false;
    }
    uint64_t total = 1;
    for (const auto c : count) {
        if (c == 0 || c > maxValues || total > maxValues / c) {   // (no product that wraps around, no grid past what is ever read)
            error = name + " has a size that can not be right";
            return false;
        }
        total *= c;
    }
    out.assign(total, std::nanf(""));
    const std::vector<uint64_t> cubeOffset(offset.size(), 0);
    OmDecoder_t decoder;
    const auto result = om_decoder_init(&decoder, om_variable_init(v->meta.data()), offset.size(), offset.data(), count.data(), cubeOffset.data(), count.data(), io_.merge, io_.max);
    if (result != ERROR_OK) {
        error = std::string{"cannot read "} + name + ": " + om_error_string(result);
        return false;
    }
    if (om_decoder_read_buffer_size(&decoder) > maxRange) {
        error = name + " has chunks of a size that can not be right";
        return false;
    }
    std::string buffer(om_decoder_read_buffer_size(&decoder), '\0');
    OmDecoder_indexRead_t indexRead;
    om_decoder_init_index_read(&decoder, &indexRead);
    while (om_decoder_next_index_read(&decoder, &indexRead)) {
        std::string index;
        if (indexRead.count > maxRange || !io_.range(indexRead.offset, indexRead.count, index) || index.size() < indexRead.count) {
            error = "could not read " + name + "'s lookup table";
            return false;
        }
        OmDecoder_dataRead_t dataRead;
        om_decoder_init_data_read(&dataRead, &indexRead);
        OmError_t err = ERROR_OK;
        while (om_decoder_next_data_read(&decoder, &dataRead, index.data(), indexRead.count, &err)) {
            std::string data;
            if (dataRead.count > maxRange || !io_.range(dataRead.offset, dataRead.count, data) || data.size() < dataRead.count) {
                error = "could not read " + name + "'s data";
                return false;
            }
            if (!om_decoder_decode_chunks(&decoder, dataRead.chunkIndex, data.data(), dataRead.count, out.data(), buffer.data(), &err)) {
                error = std::string{"could not decode "} + name + ": " + om_error_string(err);
                return false;
            }
        }
        if (err != ERROR_OK) {
            error = std::string{"could not read "} + name + ": " + om_error_string(err);
            return false;
        }
    }
    return true;
}

bool OmFile::readAll(const std::string& name, std::vector<float>& out, std::string& error) const {
    const auto * v = variable(name);
    if (!v) {
        error = "no variable " + name;
        return false;
    }
    return read(name, std::vector<uint64_t>(v->dimensions.size(), 0), v->dimensions, out, error);
}
