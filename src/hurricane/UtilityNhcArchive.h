// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYNHCARCHIVE_H
#define UTILITYNHCARCHIVE_H

#include <QDateTime>
#include <string>
#include <vector>
#include "hurricane/UtilityAtcf.h"
#include "hurricane/UtilityNhcGis.h"

using std::string;
using std::vector;

// NHC's archive of the advisory cones (nhc.noaa.gov/gis/forecast/archive/<id>_5day_<NNN>[A].zip, 2008 on) and the best track that followed (ATCF btk/ while the storm is
// recent, atcf/archive/<year>/ after). Network and disk: a published advisory never changes, so each zip is kept for good in the archive folder (about 30 KB each). The
// directory page of the archive is 19 MB, so advisories are found by asking for the next number instead of listing.
class UtilityNhcArchive {
public:
    struct Advisory {
        string number;               // "012", "012A"
        QDateTime issued;            // UTC
        UtilityNhcGis::Cone cone;
    };
    struct StormRef {
        string id;                   // "ep212026"
        string name;                 // "Trudy"
        QDateTime first;             // issued, UTC
    };
    struct Storm {
        string id;
        string name;
        vector<Advisory> advisories; // by time
        vector<UtilityAtcf::Fix> best;
        QDateTime last() const { return advisories.empty() ? QDateTime{} : advisories.back().issued; }
    };
    // the storms of a year that had a cone (Atlantic, East and Central Pacific), found by asking for each number's first advisory; the answer is kept
    static vector<StormRef> stormsOfYear(int year);
    // every advisory of a storm and its best track (the best track only when `withBest`)
    static Storm load(const string& id, bool withBest);
    // "300 AM CST Fri Oct 16 2026" -> UTC (invalid when it cannot be read)
    static QDateTime parseIssued(const string& text);
    // where the storm was at a time: the best track interpolated, false before it starts or after it ends
    static bool observedAt(const vector<UtilityAtcf::Fix>& best, const QDateTime& utc, double& lat, double& lon, int& wind);
    static double distanceNm(double lat1, double lon1, double lat2, double lon2);
};

#endif  // UTILITYNHCARCHIVE_H
