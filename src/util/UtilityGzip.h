// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYGZIP_H
#define UTILITYGZIP_H

#include <string>

// Reads a .gz file (RFC 1952 wrapper around RFC 1951 deflate data) into text. Self-contained so it builds the same on every platform.
class UtilityGzip {
public:
    // false when the data is not gzip or is damaged; out holds what was decoded up to that point
    static bool gunzip(const std::string& in, std::string& out);
    // the bare deflate stream (RFC 1951): what a zip file keeps for each entry
    static bool inflate(const unsigned char * data, size_t size, std::string& out);
};

#endif  // UTILITYGZIP_H
