// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HOMELAYOUT_H
#define HOMELAYOUT_H

#include <string>
#include <vector>

using std::string;
using std::vector;

// Where the four large home screen sections (Severe dashboard, thumbnails, forecast, text) go. The user picks a
// template - a set of zones on a small grid, like a window-snapping layout - and drags each section into a zone;
// a zone can hold several sections, stacked top to bottom in the order saved. What goes inside a section (which
// thumbnails, their order, ...) is still chosen under the other Settings headings. Saved as two prefs.
namespace HomeLayout {
    inline const string sectionSevere{"SEVERE"};
    inline const string sectionImages{"IMAGES"};
    inline const string sectionForecast{"FORECAST"};
    inline const string sectionText{"TEXT"};

    struct Zone {
        int row;
        int col;
        int rowSpan;
        int colSpan;
    };

    struct Template {
        string name;
        vector<int> columnStretch;   // relative column widths
        int rows;                    // zone rows (all the same height in the picture; content decides on screen)
        vector<Zone> zones;
    };

    const vector<Template>& templates();
    const vector<string>& sections();            // every section token, in the order the editor lists them
    string sectionLabel(const string& token);

    void load();                                 // from the saved prefs (call once at start-up)
    int templateIndex();
    void setTemplate(int index);                 // sections in zones that no longer exist move to the last zone
    int zoneOf(const string& section);
    vector<string> sectionsIn(int zone);         // in stacking order
    // puts `section` into `zone` before the `position`-th section already there (past the end = last)
    void move(const string& section, int zone, int position);
    string signature();                          // changes whenever the template or an assignment does
}

#endif  // HOMELAYOUT_H
