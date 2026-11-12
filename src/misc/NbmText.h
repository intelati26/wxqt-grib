// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NBMTEXT_H
#define NBMTEXT_H

#include <QByteArray>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "misc/UtilityForecastPoint.h"

// The National Blend of Models text bulletins (NBS short range, NBE extended, NBX super-extended) for one station, and the table of a place made of them and the NWS gridded forecast for the first hours.
// A bulletin file holds all of the 9,589 stations (10 to 30 MB): a station is found with a few range reads (see findBlock), never the whole file.
namespace NbmText {
    constexpr int missing = -99;

    struct Column {
        qint64 time{0};                       // valid time, UTC seconds
        std::string source;                   // "API", "NBS", "NBE", "NBX"
        std::map<std::string, int> value;     // the bulletin's code (TMP, DPT, SKY, WDR, WSP, GST, P12 ...) -> its value, in its own unit; a missing value is not there
        bool has(const std::string& code) const { return value.count(code) > 0; }
        int get(const std::string& code, int otherwise = missing) const { const auto f = value.find(code); return f == value.end() ? otherwise : f->second; }
    };

    struct Block {
        std::string station, kind;            // "KOKC", "NBX"
        qint64 cycle{0};                      // the run, UTC seconds
        std::vector<Column> columns;          // by time; `source` is the kind
    };

    // the block of text of one station -> its columns (empty when it is not a bulletin). Values are read by position under the FHR line, not by splitting on blanks.
    Block parse(const std::string& text, const std::string& source = "");

    // the first header in a piece of a file: where it starts in the piece and the station; false when there is none
    bool firstHeader(const std::string& piece, size_t from, size_t& at, std::string& station);

    // A file of the bucket: its url and size. Reads go through `range` (url, first byte, last byte inclusive), which is URL::getBytesRangeManaged in the program and a fake in the tests.
    struct File {
        std::string url, kind;
        long long size{0};
    };
    using Range = std::function<QByteArray(const std::string&, long long, long long)>;

    // the station's block of text in a file ("" when the file does not have it). The file is several sorted runs one after another: see the notes in the code.
    std::string findBlock(const File& file, const std::string& station, const Range& range);

    // the newest file of a kind ("nbstx", "nbetx", "nbxtx"), asked of the bucket's listing hour by hour back from `now` (UTC seconds)
    bool newestFile(const std::string& kind, qint64 now, File& out);

    // The table of a place: the gap columns (from the NWS gridded forecast), NBS, NBE after the last NBS, NBX after the last NBE, each only after the earlier; empty columns dropped.
    struct Table {
        bool ok{false};
        std::string error, station, name;
        std::vector<Column> columns;
        std::map<std::string, qint64> cycles;   // "NBS" -> the run of the file read
        qint64 gridUpdated{0};
    };
    std::vector<Column> merge(const std::vector<Column>& api, const std::vector<Column>& nbs, const std::vector<Column>& nbe, const std::vector<Column>& nbx, qint64 fromEpoch);
    std::vector<Column> gapColumns(const UtilityForecastPoint::Data& grid, qint64 from, qint64 to);   // the NWS grid every 3 hours (UTC multiples of 3) in the bulletin's codes and units
    Table fetch(const UtilityForecastPoint::Data& grid);   // (blocking: from a worker thread)
}

#endif  // NBMTEXT_H
