// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYVDM_H
#define UTILITYVDM_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// The reconnaissance Vortex Data Message (WMO URNT12 / URPN12, "REPNT2" in the NHC recon archive), in the format in use since June 2018 (NWS Service
// Change Notice 18-19): lettered lines A to U with the time and position of the centre fix, the minimum pressure, the wind maxima inbound and
// outbound, temperatures and the aircraft. "NA" is read as missing. Pure text work, no network.
class UtilityVdm {
public:
    static constexpr double missing = -9999.0;
    struct Wind {                 // a maximum wind with where it was seen
        double kt{missing};
        double direction{missing};   // of the wind (J, N), degrees
        double bearing{missing};     // from the centre to where it was observed, degrees
        double rangeNm{missing};
        string time;                 // hh:mm:ssZ
    };
    struct Vdm {
        string stormId;           // AL022026
        long seconds{0};          // UTC seconds since 1970-01-01 of the fix (item A)
        string fixTime;           // "22/00:11:26Z" as written
        double lat{missing};
        double lon{missing};      // east positive
        double heightM{missing};  // minimum height at the standard level (item C)
        int levelMb{0};           // 700
        double pressure{missing}; // minimum sea-level pressure, mb (D)
        bool extrapolated{false}; // D says EXTRAP: from the flight level, not a dropsonde
        double centerWindDir{missing};   // surface wind from the dropsonde at the centre (E)
        double centerWindKt{missing};
        string eyeCharacter;      // F  CLOSED, OPEN SW ...
        string eyeShape;          // G  C20, E12/30/15 ...
        Wind inboundSurface;      // H, I
        Wind inboundFlight;       // J, K
        Wind outboundSurface;     // L, M
        Wind outboundFlight;      // N, O
        double tempOutsideC{missing};   // P
        double tempInsideC{missing};    // Q
        double dewPointInsideC{missing};   // R
        double seaSurfaceC{missing};
        string fixedBy;           // S  "1345 / 7"
        string accuracy;          // T  "0.01 / .1 nm"
        string aircraft;          // U  "NOAA3 0802A BERTHA OB 17"
        string remarks;
        bool test{false};         // a communications check, not a mission
        double maxFlightWind() const;   // the larger of the inbound and outbound flight-level maxima
    };
    // `fileStamp` is yyyymmddhhmm from the archive file name: it supplies the month and year (the message gives only the day)
    static bool parse(const string& text, const string& fileStamp, Vdm& out);
    static bool has(double value) { return value > missing + 1.0; }
};

#endif  // UTILITYVDM_H
