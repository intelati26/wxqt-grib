// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef AHDVIEWER_H
#define AHDVIEWER_H

#include <string>
#include <vector>
#include <QComboBox>
#include <QLabel>
#include <utility>
#include <QPlainTextEdit>
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// The National Water Center's experimental Area Hydrologic Discussion: the graphic of the areas that may see rapid-onset flooding (the newest, or one of the archive since June 2022) and the
// text of the newest discussion (NWS products, code AHD). It is issued when there is a threat, not on a schedule. The site button opens water.noaa.gov's page of the product.
class AhdViewer : public Window {
public:
    explicit AhdViewer(Window * parent);
    // the newest graphic's file and its time when it is recent enough to be worth a place on a dashboard (within `hours`); false when it is older or could not be read
    static bool recent(int hours, string& imageUrl);
    static const string site;

private:
    void showImage(int index);
    void closeEventCustom() override { closed = true; }
    VBox box;
    QComboBox * times{};
    QLabel * note{};
    QPlainTextEdit * text{};
    ZoomImage image;
    vector<string> files;     // the graphics, newest first
    vector<long> stampList;   // the time of each
    vector<std::pair<long, string>> history;   // the text products the NWS still has: issued, text
    string newestIssued;
    bool closed{false};
};

#endif  // AHDVIEWER_H
