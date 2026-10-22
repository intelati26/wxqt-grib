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

    // A quick estimate of how far a pass can be trusted, worked out from the geometry and the age of the element set (no measurement: a guide)
    struct Quality {
        enum class Grade { Good, Fair, Poor, Possible } grade{Grade::Good};   // Possible: a tasked radar, where it could look
        string summary;           // "Good", "Fair: pixels about 3 times wider", "Poor: element set 9 days old"
        string detail;            // the numbers behind it, one per line
        double ageDays{0.0};      // of the element set at the time of the pass
        double positionKm{0.0};   // how far off the satellite may be then
        double timeSeconds{0.0};  // the same as a time along the track
        double pixelGrowth{1.0};  // how much wider a pixel is across the scan than at the middle (1: no growth)
    };
    Quality quality(const Instrument& instrument, const SatelliteOrbit::Tle& elements, const SatelliteOrbit::Pass& pass);

    struct Row {
        string place;
        bool storm{false};
        string satellite;
        string instrument;
        Kind kind;
        SatelliteOrbit::Pass pass;
        Quality quality;
        double lat{0}, lon{0};   // of the place
    };
    // every pass of the satellites chosen (all when empty: by name) over the places in the next hours, soonest first
    vector<Row> passList(const vector<Place>& at, const vector<string>& satellitesWanted, const vector<Kind>& kindsWanted, double hours, long long from, string& problem);
}

#endif  // SATELLITEPASSES_H
