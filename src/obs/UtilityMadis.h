// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYMADIS_H
#define UTILITYMADIS_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "obs/SurfaceStation.h"

using std::string;
using std::vector;

// NOAA's MADIS public mesonet files (madis-data.ncep.noaa.gov/madisPublic1/data/LDAD/mesonet/netCDF/yyyymmdd_hh00.gz): every report the
// mesonets and other non-federal networks sent in that hour (state mesonets, RAWS, HADS, APRS weather stations, road weather ...), a gzip
// of a netCDF "classic" file with one record per report, about 30 MB packed and 400 MB unpacked for a full hour. So the file is read as a
// stream: the unpacked bytes are fed to `MesonetReader::push` in pieces, a record at a time is decoded, and only the newest report of each
// station is kept. Pure decoding, no network (UtilityGzip::gunzipStream does the unpacking).
class UtilityMadis {
public:
    class MesonetReader {
    public:
        // false once the data cannot be a netCDF classic file with the variables below
        bool push(const char * data, size_t size);
        bool failed() const { return bad; }
        size_t records() const { return count; }
        // the newest report of each station, in no particular order
        vector<SurfaceStation> stations() const;
        // reports older than this many seconds before the newest in the file are skipped (0 keeps all)
        void setMaxAge(long seconds) { maxAge = seconds; }

    private:
        struct Var {
            string name;
            int type{0};              // 1 byte, 2 char, 3 short, 4 int, 5 float, 6 double
            vector<int> dims;
            uint64_t size{0};         // bytes per record
            uint64_t begin{0};        // file offset of its first value
            bool record{false};
        };
        bool parseHeader();
        void decode(const char * record);
        const Var * find(const char * name) const;
        string buffer;                // unread bytes
        uint64_t bufferStart{0};      // file offset of buffer[0]
        bool haveHeader{false};
        bool bad{false};
        uint64_t numrecs{0};          // from the header, 0xffffffff while streaming
        uint64_t firstRecord{0};
        uint64_t recordSize{0};
        size_t count{0};
        long maxAge{0};
        vector<Var> vars;
        std::map<string, SurfaceStation> latest;   // by provider id
        // byte offsets of the fields inside a record, -1 when the file lacks one
        struct Offsets {
            int providerId{-1}, stationId{-1}, stationName{-1}, dataProvider{-1}, latitude{-1}, longitude{-1}, elevation{-1}, observationTime{-1};
            int temperature{-1}, temperatureDD{-1}, dewpoint{-1}, relHumidity{-1}, seaLevelPressure{-1}, altimeter{-1}, windDir{-1}, windSpeed{-1}, windGust{-1}, visibility{-1};
            int providerIdLength{0}, stationIdLength{0}, stationNameLength{0}, dataProviderLength{0};
        } at;
        long newest{0};
    };
    // the file names in an Apache directory listing, oldest first ("20261008_1100.gz")
    static vector<string> listFiles(const string& html);
    // big-endian reads
    static float bigFloat(const char * p);
    static double bigDouble(const char * p);
    static uint32_t bigInt(const char * p);
    static bool valid(double v) { return v > -9990.0 && v < 1.0e30; }
};

#endif  // UTILITYMADIS_H
