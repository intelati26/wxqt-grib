// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYZIP_H
#define UTILITYZIP_H

#include <map>
#include <string>

// Reads the files of a .zip archive held in memory: stored and deflated entries (zip64 and encryption are not understood). Self-contained, builds the same
// on every platform; the deflate part is UtilityGzip's.
class UtilityZip {
public:
    // name -> contents; false when the archive is damaged or uses something not supported (what could be read stays in `files`)
    static bool read(const std::string& zip, std::map<std::string, std::string>& files);
};

#endif  // UTILITYZIP_H
