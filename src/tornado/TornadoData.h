// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TORNADODATA_H
#define TORNADODATA_H

#include <memory>
#include <string>
#include <vector>
#include "tornado/UtilityTornado.h"

// The download behind the tornado screens (blocks: call it off the GUI thread). The SPC file (about 9 MB, one row per tornado since 1950) is found by its name on
// the SPC "WCM" page, kept on disk for a week and in memory while the program runs.
class TornadoData {
public:
    struct Database {
        std::vector<UtilityTornado::Tornado> tornadoes;
        std::string file;               // 1950-2025_actual_tornadoes.csv
        int firstYear{0};
        int lastYear{0};
        int lastMonth{0};               // the date of the newest tornado in the file (the file runs to about the middle of the year it is named for)
        int lastDay{0};
        std::string error;
    };
    static std::shared_ptr<const Database> load();
    static long ordinal(int year, int month, int day);   // days since 1970-01-01
};

#endif  // TORNADODATA_H
