// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SATELLITEPASSES_H
#define SATELLITEPASSES_H

#include <map>
#include <string>
#include <vector>
#include "space/SatelliteOrbit.h"

using std::string;
using std::vector;

// The weather and radar satellites of low orbit with the instruments that matter to a storm, their element sets from CelesTrak (kept on disk, read again after half a day), the places the passes
// are made for (the active storms, the saved locations, places of the person's own) and the list of passes. Blocking: call it from a thread.
namespace SatellitePasses {
    enum class Kind { Imagery, Microwave, Wind, Radar };
    struct Instrument {
        string label;            // "VIIRS"
        Kind kind;
        double halfWidthKm;
        double innerKm{0.0};     // a gap under the satellite (ASCAT)
        int side{0};             // 0 both sides of the track, 1 looking right only (Sentinel-1)
        string note;             // what it is good for
    };
    struct Satellite {
        string name;             // "NOAA-20"
        int catalog;
        vector<Instrument> instruments;
    };
    const vector<Satellite>& satellites();
    string kindName(Kind);

    // the element set of a satellite: from the disk when it is under 12 hours old, else downloaded (the older one when there is no answer); invalid when there is none
    SatelliteOrbit::Tle elements(int catalog, string& problem);

    struct Place {
        string name;
        double lat{0}, lon{0};
        bool storm{false};
        string detail;           // "Typhoon CHAMPI, 150 kt"
    };
    vector<Place> places();                                    // the places of the person's own (setting SATPASS_PLACES) and the saved locations
    void savePlaces(const vector<Place>& own);                 // the person's own
    vector<Place> own();
    vector<Place> stormPlaces();                               // the active storms of every basin, where they are now (CIRA / RAMMB)

    struct Row {
        string place;
        bool storm{false};
        string satellite;
        string instrument;
        Kind kind;
        SatelliteOrbit::Pass pass;
    };
    // every pass of the satellites chosen (all when empty: by name) over the places in the next hours, soonest first
    vector<Row> passList(const vector<Place>& at, const vector<string>& satellitesWanted, const vector<Kind>& kindsWanted, double hours, long long from, string& problem);
}

#endif  // SATELLITEPASSES_H
