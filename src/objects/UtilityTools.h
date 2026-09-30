// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYTOOLS_H
#define UTILITYTOOLS_H

#include <QString>

// Locating optional helper programs (cjxl, avifenc, ffmpeg) that the app can
// use but does not ship. Looked for, in order, in a "tools" folder next to the
// executable (so a portable install can simply have the files dropped in),
// beside the executable itself, then on PATH.
class UtilityTools {
public:
    // full path of the tool, or an empty string if it is not installed
    static QString find(const QString& name);
    // the "tools" folder next to the executable (may not exist yet)
    static QString folder();
};

#endif  // UTILITYTOOLS_H
