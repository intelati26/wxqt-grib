// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef PALETTESTORE_H
#define PALETTESTORE_H

#include <map>
#include <string>
#include "gfs/GfsChart.h"

// The user's own color scales for the model charts, kept in the preferences by the key of the default scale they replace (GfsChart::rampKey): every chart that
// uses that default is drawn with the user's. Safe to read from the drawing threads.
namespace PaletteStore {
    std::map<std::string, GfsChart::Ramp> all();
    bool has(const std::string& key);
    void set(const std::string& key, const GfsChart::Ramp& ramp);
    void clear(const std::string& key);
}

#endif  // PALETTESTORE_H
