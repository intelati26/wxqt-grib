// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef APPSTATE_H
#define APPSTATE_H

#include <atomic>

// Set once the application starts to close. The program waits for every background job - running or still queued - before it
// exits, so with slow or heavy jobs queued (MRMS decodes, model renders, ...) closing took minutes. Jobs that have not started
// yet check this and skip their work.
namespace AppState {
    inline std::atomic<bool> quitting{false};
}

#endif  // APPSTATE_H
