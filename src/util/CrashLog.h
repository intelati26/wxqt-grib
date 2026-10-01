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
//
// Debug output (the app's network / render tracing and Qt's debug messages) is kept apart in wxqt-debug.log, and is
// written only when debugging is switched on: `wxqt.exe --debug` or the environment variable WXQT_DEBUG=1 (the
// Windows launcher script has an option for it).
namespace CrashLog {
    void install(bool debug);                // call once, early, after the QApplication exists
    void write(const std::string& line);     // timestamped, into wxqt.log; safe from any thread
    void writeDebug(const std::string& line);   // timestamped, into wxqt-debug.log only if debugging is on
    bool debugEnabled();
    std::string path();
}

#endif  // CRASHLOG_H
