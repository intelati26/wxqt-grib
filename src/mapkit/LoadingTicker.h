// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef LOADINGTICKER_H
#define LOADINGTICKER_H

#include <functional>
#include <memory>
#include <string>
#include "mapkit/MapLayer.h"

// A progress line for a layer that takes a while to load: twice a second, while `loading()` is true, the text goes to the screen's status line. `alive` is a flag the layer clears when it is
// destroyed (the tick must not touch a layer that is gone).
namespace LoadingTicker {
    void start(MapHost& host, const std::shared_ptr<bool>& alive, const std::function<bool()>& loading, const std::function<std::string()>& text);
}

#endif  // LOADINGTICKER_H
