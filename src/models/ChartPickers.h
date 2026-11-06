// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTPICKERS_H
#define CHARTPICKERS_H

#include <functional>
#include <string>
#include <vector>
#include "models/ProductPicker.h"

// The pieces every screen that picks a model chart or an area has in common, in one place: the Model Viewer, its 15 minute and NWS analysis windows, the map's model chart layer, the master map.
// The favorites (kept per model, as MODELFAV_<model> and SECTORFAV_<model>), what an area is called, how the entries are grouped and ordered, and the two picker dialogs. A change here changes
// them all (the picker itself, ProductPicker, is shared already).
namespace ChartPickers {
    std::vector<std::string> splitList(const std::string& text);        // "a,b,c"
    std::string joinList(const std::vector<std::string>& list);
    std::vector<std::string> favorites(const std::string& key);         // the ids kept under a preference
    void saveFavorites(const std::string& key, const std::vector<std::string>& ids);

    // what an area is called in a list: the areas round a saved location say the place and the kilometers a side, a model's own domain its limits, the storms their names
    std::string areaLabel(const std::string& id);

    // the groups of an area list in the order they are listed (the Model Viewer's), and the stable sort by them
    const std::vector<std::string>& areaGroupOrder();
    void sortByGroup(std::vector<ProductPicker::Entry>& entries, const std::vector<std::string>& order);
    std::vector<ProductPicker::Entry> areaEntries(const std::vector<std::string>& ids);   // label and group of each, in the group order
    // the charts of a model for the picker: the id, the label and the category of the chart
    std::vector<ProductPicker::Entry> chartEntries(const std::string& model, const std::vector<std::string>& ids, const std::vector<std::string>& labels);

    // the chart picker of a model (favorites kept for it, the lines and barbs `extras` that can be ticked on, those `ticked` now): the pick goes to onPick. Not shown: the caller may add its own
    // callbacks and a note, then calls show() (a caller that does not keep the pointer sets WA_DeleteOnClose).
    ProductPicker * chartPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current, const std::function<void(const std::string&)>& onPick,
                                const std::vector<GfsChart::OverlayChoice>& extras = {}, const std::vector<std::string>& ticked = {});
    // the area picker (favorites kept per model, titled "areas"): the pick goes to onPick. Not shown.
    ProductPicker * areaPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current, const std::function<void(const std::string&)>& onPick);
}

#endif  // CHARTPICKERS_H
