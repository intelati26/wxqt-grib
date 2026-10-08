// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TORNADOYEARSVIEWER_H
#define TORNADOYEARSVIEWER_H

#include <memory>
#include <string>
#include "tornado/TornadoData.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/Text.h"

class QTableWidget;

// The years of the tornado database in a table to sort: click a heading to rank them by the number of tornadoes, the strong ones, the fatalities, the injuries or the longest track.
// The tornadoes counted are those the charts window is showing (its rating and state).
class TornadoYearsViewer : public Window {
public:
    TornadoYearsViewer(Window * parent, const std::shared_ptr<const TornadoData::Database>& db, int rating, const std::string& state);

private:
    VBox box;
    Text textHint;
    QTableWidget * table{};
};

#endif  // TORNADOYEARSVIEWER_H
