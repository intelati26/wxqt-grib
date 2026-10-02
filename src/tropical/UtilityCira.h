// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCIRA_H
#define UTILITYCIRA_H

#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

// CIRA / RAMMB "TC Real-Time" (https://rammb-data.cira.colostate.edu/tc_realtime/): satellite and intensity-guidance products for
// every active tropical cyclone worldwide, as plain web pages and image files (experimental products). The storm identifier is the
// ATCF one in lower case ("ep182026"), the same one NHC uses.
namespace UtilityCira {
    struct Storm {
        string id;       // "ep182026"
        string title;    // "EP182026 - Major Hurricane RACHEL"
        string name;     // "RACHEL" (empty for an invest)
        string basin;    // "Eastern Pacific"
    };

    struct Product {
        string key;      // the folder name: "4kmirimg"
        string label;
        bool loop;       // an image every few hours or better: worth a loop
    };

    // every product this program shows, in display order
    const vector<Product>& products();

    // the storms on the front page (all basins), grouped by basin in the order of basinOrder()
    bool activeStorms(vector<Storm>& out, string& error);
    const vector<string>& basinOrder();
    string basinOf(const string& id);

    struct StormPage {
        std::map<string, string> imageUrl;   // product key -> the newest file's full address
        string forecastTime;                 // "2026-10-02 06:00"
        string forecastTrack;                // the forecast table as text
        string trackHistory;
        string rapidIntensificationTableUrl; // the text table of rapid-intensification probabilities (empty when none)
    };
    bool stormPage(const string& id, StormPage& out, string& error);

    // addresses of the last `count` images of a product, oldest first (from the product's archive page)
    vector<string> frameUrls(const string& id, const string& product, int count);
}

#endif  // UTILITYCIRA_H
