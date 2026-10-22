// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef OFFICEAREAS_H
#define OFFICEAREAS_H

#include <string>
#include <vector>
#include <QPolygonF>

// The county warning areas of the National Weather Service's forecast offices (the NWS reference map service: one polygon or more for each of the 125 offices), read once (about a megabyte,
// simplified to 0.02 degrees) and kept on disk. Blocking: call it from a thread, or where a wait is fine.
namespace OfficeAreas {
    struct Office {
        std::string id;                 // "SGF"
        std::string name;               // "Springfield, MO"
        std::vector<QPolygonF> rings;   // x = longitude, y = latitude
    };
    // the areas of a kind: "cwa" the forecast offices, "rfc" the river forecast centers, "state" the states; sorted by id; empty (with the reason in `problem`) when the service cannot be read and nothing is kept
    const std::vector<Office>& all(std::string& problem, const std::string& kind = "cwa");
    // the area named "CWA:SGF" / "RFC:ABRFC" / "ST:MO" (a bare id is an office), or null; and what it is called ("SGF Springfield, MO")
    const Office * find(const std::string& full, std::string& label);
}

#endif  // OFFICEAREAS_H
