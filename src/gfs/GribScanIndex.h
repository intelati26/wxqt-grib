// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GRIBSCANINDEX_H
#define GRIBSCANINDEX_H

#include <string>
#include <QByteArray>

// An index for any GRIB2 file that has none. A GRIB2 message begins with a header that says how long the message is, and its next few sections say which field it is (the parameter, the level, the
// reference time, the forecast time or the interval it covers), so the file is its own table of contents: read the head of the first message, add its length to get the next one's offset, read
// that head, and so on to the end of the file. Each stop is one small ranged read (a few hundred bytes of a message of some megabytes), and the lines made are in NOAA's form
// ("12:5483620:d=2026111812:PMTF:1 sigma level:0-1 hour ave fcst:") so that everything that reads a `.idx` reads them. A file that is complete (the server's last modified time is not recent)
// has its index kept for good: a published file does not change.
// A model file asks for it with `index_suffix = .gribscan`: the "index" of a file is then that file's url + ".gribscan", which no server has and this answers.
namespace GribScanIndex {
    const char * const suffix = ".gribscan";
    bool handles(const std::string& url);        // an index url that ends in the suffix
    QByteArray index(const std::string& url);    // the lines for the file the url names (without the suffix); empty when the file could not be read
    // the lines of the messages of a file in memory (no network: the part that can be tested); `from` is the offset the first message of `file` has in the whole file
    std::string scan(const QByteArray& file, long long from = 0);
}

#endif  // GRIBSCANINDEX_H
