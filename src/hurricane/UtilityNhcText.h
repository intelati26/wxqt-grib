// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYNHCTEXT_H
#define UTILITYNHCTEXT_H

#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;

// NHC's text products (nhc.noaa.gov/text/MIATCPAT4.shtml and the like) are a web page around one <pre> block holding the bulletin. Pure text work.
class UtilityNhcText {
public:
    static string bulletin(const string& html);       // the text of the <pre> block, entities decoded; empty when the page has none
    // the sections of a public advisory or discussion: "SUMMARY OF 400 AM CDT...0900 UTC...INFORMATION" and the like are headed by a line followed by a row of dashes
    static vector<std::pair<string, string>> sections(const string& bulletin);   // heading, body
    static string headline(const string& bulletin);   // the lines between "..." marks at the top of an advisory, joined: "DEPRESSION BECOMES TROPICAL STORM ISAIAS"
};

#endif  // UTILITYNHCTEXT_H
