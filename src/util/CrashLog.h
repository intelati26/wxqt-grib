// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CRASHLOG_H
#define CRASHLOG_H

#include <string>

// A plain-text log file, wxqt.log, next to the executable (or in the user's local app-data folder when that folder
// is not writable). It gets the app's own debug lines (UtilityLog), Qt's warnings and errors, any uncaught C++
// exception with its message, and - on Windows - the code, address and module of a hard crash, so a closed
// window leaves something behind. The file is started afresh once it passes 1 MB (the old one is kept as
// wxqt.log.old).
namespace CrashLog {
    void install();                          // call once, early, after the QApplication exists
    void write(const std::string& line);     // timestamped; safe from any thread
    std::string path();
}

#endif  // CRASHLOG_H
