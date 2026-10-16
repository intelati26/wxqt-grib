// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/LoadingTicker.h"
#include <QTimer>

void LoadingTicker::start(MapHost& host, const std::shared_ptr<bool>& alive, const std::function<bool()>& loading, const std::function<std::string()>& text) {
    auto tick = std::make_shared<std::function<void()>>();
    *tick = [&host, alive, loading, text, tick] {
        if (*alive && loading()) {
            host.status(text());
            QTimer::singleShot(500, [tick] { (*tick)(); });
        } else {
            tick->operator=(nullptr);   // done: let the closure go
        }
    };
    QTimer::singleShot(500, [tick] { if (*tick) { (*tick)(); } });
}
