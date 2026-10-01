// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYCAMS_H
#define UTILITYCAMS_H

#include <map>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>

using std::string;
using std::vector;

// Client for NSSL's CAMs viewer (https://cams.nssl.noaa.gov/): experimental convection-allowing models, one
// transparent PNG per model / run / product / sector / forecast time. Nothing about the models or products is
// built in - what exists is asked of the site's own query.php (the same calls its web page makes), so a new
// model or product shows up here by itself. Every call blocks on the network: use them off the UI thread.
// A failure is returned as a message, never swallowed.
namespace UtilityCams {
    struct Model {
        string id;
        string name;
        bool discontinued{false};
        vector<string> sectors;
        vector<string> dailyRuns;                    // "0000", "1200"
        std::map<string, vector<int>> timesByRun;    // run hour -> forecast seconds; "" holds a single list for every run
    };

    struct Run {
        string date;   // yyyymmdd
        string time;   // hhmm
        string key() const { return date + time; }
    };

    struct Product {
        string id;
        string label;
        string category;                  // category id, e.g. "det_ref"
        vector<string> otherCategories;
        vector<string> underlays;         // layer ids drawn beneath / above by default
        vector<string> overlays;
        vector<int> times;                // forecast seconds this product exists at for the model
    };

    struct Catalog {
        vector<Product> products;
        std::map<string, string> categoryLabel;                    // id -> "Reflectivity"
        vector<std::pair<string, vector<string>>> groups;          // "Severe" -> category ids, in the site's order
    };

    // [{id, display name}] of every model the site serves
    bool models(vector<std::pair<string, string>>& out, string& error);
    bool model(const string& id, Model& out, string& error);
    // the model's most recent runs, newest first (at most `limit`)
    bool recentRuns(const string& modelId, size_t limit, vector<Run>& out, string& error);
    // sector id -> display name
    bool sectorNames(std::map<string, string>& out, string& error);
    // what the model offers for this run: products with their forecast times and the category tree
    bool catalog(const Model& model, const Run& run, const string& sector, Catalog& out, string& error);
    // the largest forecast time (seconds) already published for the run; -1 if the site does not say
    int latestAvailableSeconds(const string& modelId, const Run& run, const string& product, const string& sector);

    // The URL of one product image. `seconds` is the forecast time.
    string imageUrl(const string& modelId, const Run& run, const string& layer, const string& sector, int seconds);
    string baseMapUrl(const string& sector);
    // The stack for one picture, bottom to top: underlays, the product, overlays, the sector's outline map,
    // joined with '|' (a layer spec). The product is the only required layer.
    string layerSpec(const string& modelId, const Run& run, const Product& product, const string& sector, int seconds);
    // downloads and composites a layer spec onto white; empty bytes and `error` set when the product image itself
    // cannot be had (the optional layers are skipped quietly)
    QByteArray composite(const string& spec, string& error);
    // "F06" from a layer spec's product image name; "" if it has none
    string frameLabel(const string& spec);
    // "Thu 10/01 06Z" for a run plus forecast seconds
    string validLabel(const Run& run, int seconds);
}

#endif  // UTILITYCAMS_H
