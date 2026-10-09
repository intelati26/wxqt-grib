// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NETPRIORITY_H
#define NETPRIORITY_H

// The priority the network requests made by this thread get (0 what the user is looking at, 1 read ahead, 2 background): set by NetManager::Scope, and passed on by code that starts
// threads of its own to make the requests (so a fetch that fans out over a pool keeps its priority). No dependencies, so the model code can carry it without the network client.
namespace NetPriority {
    inline thread_local int current = 0;
    struct Carry {   // for a thread started by one that has a priority: the same one for as long as this lives
        explicit Carry(int value) : before{current} { current = value; }
        ~Carry() { current = before; }
        Carry(const Carry&) = delete;
        Carry& operator=(const Carry&) = delete;
        int before;
    };
}

#endif  // NETPRIORITY_H
