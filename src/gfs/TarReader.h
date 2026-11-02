// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TARREADER_H
#define TARREADER_H

#include <string>
#include <QString>

// One member of a tar on a web server, by range: the entries' 512 byte headers are walked a file at a time (what was seen is kept in a small index file for the next ask), and only the
// bytes of the member wanted are fetched. NSSL posts each run as one tar of gzipped GRIB2 files, one an hour.
namespace TarReader {
    // The member `name` (compared by its file name, without the directories) of the tar at `url`, made into the file `path`: fetched by range and gunzipped when its name ends in ".gz", left
    // as it is when not. An existing file is kept. `indexPath` is where the walk's results are kept. false with the reason when the tar does not have it (yet) or cannot be read.
    // `expect` ("GRIB"): the first bytes the member must have once unpacked.
    bool extract(const std::string& url, const std::string& name, const QString& indexPath, const QString& path, const std::string& expect, std::string& error);
}

#endif  // TARREADER_H
