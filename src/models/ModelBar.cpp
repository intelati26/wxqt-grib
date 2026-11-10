#include "models/ModelBar.h"
#include <algorithm>
#include <map>
#include <QRegularExpression>
#include "gfs/GfsChart.h"
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
#include "models/PaletteEditor.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

using std::string;

std::vector<ProductPicker::Entry> ModelBar::modelEntries(const std::vector<string>& ids) {
    std::vector<ProductPicker::Entry> entries;
    // The models the program has no description for (the charts of NCEP's model guidance site are pictures)
    static const std::map<string, string> pictureModels{
        {"ESTOFS", "NOAA/NOS ESTOFS storm surge and tide"}, {"FIREWX", "NOAA/NCEP NAM fire weather nest, retired Nov 2026"}, {"GEFS-MEAN-SPRD", "NOAA/NCEP GEFS ensemble mean and spread"},
        {"GEFS-SPAG", "NOAA/NCEP GEFS spaghetti charts"}, {"HRW-ARW", "NOAA/NCEP HiResW ARW, 3 km, retired Nov 2026"}, {"HRW-ARW2", "NOAA/NCEP HiResW ARW, second run, 3 km, retired Nov 2026"}, {"HRW-FV3", "NOAA/NCEP HiResW FV3, 3 km, retired Nov 2026"},
        {"NAEFS", "North American Ensemble Forecast System (GEFS with Canada's GEPS)"}, {"NAM", "NOAA/NCEP NAM, 12 km, retired Nov 2026"}, {"NAM-HIRES", "NOAA/NCEP NAM CONUS nest, 3 km, retired Nov 2026"},
        {"POLAR", "Polar charts of NCEP's model guidance site"}, {"RAP", "NOAA/NCEP Rapid Refresh, 13 km"}, {"WW3", "NOAA/NCEP WAVEWATCH III, global"},
        {"WW3-ENP", "NOAA/NCEP WAVEWATCH III, eastern North Pacific"}, {"WW3-WNA", "NOAA/NCEP WAVEWATCH III, western North Atlantic"}};
    // the label of the list: what the model is, without the notes of licence and dates (those are in the hover text and the search)
    static const QRegularExpression notes{"\\s*\\([^)]*\\)"};
    for (const auto& id : ids) {
        auto group = GfsModels::pickerGroup(id);
        if (group == "Global" || group == "Regional" || group == "Convection allowing") {
            group = "Deterministic: " + group;   // (the ensembles are the other list)
        }
        const auto * def = GfsModels::find(id);   // every model says what it is, so a model is found by who makes it ("ECMWF", "NASA") as well as by its short name
        string full = def && def->label != id ? def->label : string{};
        if (full.empty()) {
            const auto found = pictureModels.find(id);
            full = found == pictureModels.end() ? string{} : found->second;
        }
        const string brief = QString::fromStdString(full).remove(notes).replace(" ,", ",").simplified().toStdString();
        entries.push_back({id, brief.empty() ? id : id + "  -  " + brief, group, full == brief ? string{} : id + "  -  " + full});
    }
    // the main models first in a group (what a person looks for), the rest in the order the program has them
    static const std::vector<string> first{"GFS", "IFS", "AIFS", "AIGFS", "GEOS-FP", "NAM", "RAP", "NBM", "RRFS-NA", "ICON-EU", "HRRR", "RRFS", "NAM-HIRES", "HRW-FV3", "HRW-ARW", "HRW-ARW2", "HRDPS",
                                           "ICON-D2", "ENS-ALL", "GEFS", "IFS-ENS", "GEPS", "NAEFS", "HREF", "REFS", "CFS30", "CFS", "EC46", "SEAS5", "GFS-WAVE", "WW3", "GEFS-WAVE"};
    const auto rank = [] (const string& id) { return static_cast<int>(std::find(first.begin(), first.end(), id) - first.begin()); };
    std::stable_sort(entries.begin(), entries.end(), [&rank] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) { return rank(a.id) < rank(b.id); });
    static const std::vector<string> order{"Deterministic: Global", "Deterministic: Regional", "Deterministic: Convection allowing", "Ensembles", "Probabilistic precipitation", "Seasonal", "Waves and ocean", "Air quality", "Tropical cyclones", "NWS analysis", "Archive",
                                           "SPC guidance and mesoanalysis", "Other"};
    std::stable_sort(entries.begin(), entries.end(), [] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
    return entries;
}

std::vector<string> ModelBar::changeLabels(bool shortWords) {
    if (shortWords) {
        return {"Plain chart", "Change since run 6 h earlier", "Change since run 12 h earlier", "Change since run 24 h earlier"};
    }
    return {"Plain chart", "Change since the run 6 h earlier", "Change since the run 12 h earlier", "Change since the run 24 h earlier", "Maximum of the 24 hours ending at this hour"};
}

int ModelBar::changeIndex(int changeBack, bool maxDay) {
    return maxDay ? 4 : changeBack == 0 ? 0 : changeBack == 6 ? 1 : changeBack == 12 ? 2 : changeBack == 24 ? 3 : -1;
}

void ModelBar::changeFromIndex(int index, int& changeBack, bool& maxDay) {
    maxDay = index == 4;
    changeBack = index == 1 ? 6 : index == 2 ? 12 : index == 3 ? 24 : 0;
}

void ModelBar::editColors(QWidget * parent, const string& model, const string& param, int changeBack, const std::function<void(const QString&)>& say, const std::function<void()>& changed) {
    const auto * product = GfsRender::drawsModel(model) ? GfsChart::product(param, model) : nullptr;
    if (product == nullptr) {
        say("The color editor is for the charts drawn from GRIB (not the pictures fetched from other sites).");
        return;
    }
    if (changeBack > 0) {
        say("A change chart has its own blue / red scale; show the plain chart to edit its colors.");
        return;
    }
    const bool magColors = Utility::readPref("MAG_COLORS", "true").compare(0, 1, "t") == 0;
    const auto ramp = GfsChart::defaultRamp(*product, magColors);
    if (ramp.stops.empty()) {
        say("This chart is lines only, it has no color scale.");
        return;
    }
    new PaletteEditor{parent, product->label, ramp, product->quantity, UIPreferences::unitsF, changed};
}
