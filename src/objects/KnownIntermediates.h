// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef KNOWNINTERMEDIATES_H
#define KNOWNINTERMEDIATES_H

#include <QSslConfiguration>
#include <QUrl>

// Some servers send only their own certificate and leave out the intermediate CA that links it to a root.
// Browsers quietly fetch the missing piece; Qt's OpenSSL backend (the Linux/AppImage build) does not, so the
// request fails and, before this, did so silently with an empty reply. For the few hosts known to do this the
// public intermediate is supplied here, so the normal verification still runs - it is never switched off.
namespace KnownIntermediates {
    // true when `url`'s host is one that needs a supplied intermediate
    bool needed(const QUrl& url);
    // the default configuration plus the intermediate(s) for such hosts
    QSslConfiguration configuration();
}

#endif  // KNOWNINTERMEDIATES_H
