// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EARTHDATA_H
#define EARTHDATA_H

#include <string>
#include <QByteArray>

// The person's own NASA Earthdata Login token (urs.earthdata.nasa.gov: the account is free, a token is made on its page and lasts about 60 days). Some data (PO.DAAC's ASCAT winds) is only
// given to a logged-in request. The token is kept in the program's settings file, which stays through upgrades (Settings > Accounts), and is sent only to NASA's Earthdata hosts.
namespace Earthdata {
    std::string token();                         // "" when there is none
    void setToken(const std::string&);
    bool configured();
    // "Bearer ..." for a request to an Earthdata host (earthdata.nasa.gov and its subdomains), empty for any other address or with no token
    QByteArray authorizationFor(const std::string& url);
    // asks PO.DAAC for a few bytes of the newest ASCAT coastal file with the token: "" when it is accepted, the reason when it is not (no network, no token, refused)
    std::string test();
}

#endif  // EARTHDATA_H
