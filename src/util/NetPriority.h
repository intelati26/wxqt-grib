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
    inline thread_local const void * owner = nullptr;   // who the requests of this thread are for (the screen whose task it is): when it closes, what it still waits for is dropped
    struct Carry {   // for a thread started by one that has a priority and an owner: the same ones for as long as this lives
        explicit Carry(int value, const void * who = nullptr) : before{current}, was{owner} {
            current = value;
            owner = who;
        }
        ~Carry() {
            current = before;
            owner = was;
        }
        Carry(const Carry&) = delete;
        Carry& operator=(const Carry&) = delete;
        int before;
        const void * was;
    };
}

#endif  // NETPRIORITY_H
