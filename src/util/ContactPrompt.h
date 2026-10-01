// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CONTACTPROMPT_H
#define CONTACTPROMPT_H

#include <QWidget>

// NWS / NOAA ask programs that fetch their data to identify themselves with a contact address. wxqt puts the address saved in
// Settings > General ("Contact email") in the User-Agent of its requests. Until one is saved, this asks for it at every start.
namespace ContactPrompt {
    bool valid(const QString& email);   // a plausible address: something@something.tld
    // Shows the question when no address is saved; "Not now" keeps it for the next start. Returns true if an address was saved.
    bool askIfMissing(QWidget * parent);
}

#endif  // CONTACTPROMPT_H
