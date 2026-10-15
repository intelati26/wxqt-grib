// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CAMSVIEWER_H
#define CAMSVIEWER_H

#include <map>
#include <mutex>
#include <set>
#include <QPushButton>
#include <QTimer>
#include <string>
#include <utility>
#include <vector>
#include <QLabel>
#include "models/UtilityCams.h"
#include "models/TimeStrip.h"
#include "util/Utility.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/ControlRow.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;
using std::vector;

// NSSL's CAMs imagery (experimental convection-allowing models) in the layout of the SPC HREF viewer: model,
// run, sector, product and forecast time, with back / forward and the shared loop bar. The lists are asked of
// cams.nssl.noaa.gov each time (see UtilityCams), so new models and products appear without a code change.
class CamsViewer : public Window {
public:
    explicit CamsViewer(Window * parent);

private:
    struct Loaded;   // what one background load brings back

    void discoverModels();
    void loadModel(const string& modelId);       // model info, runs, sectors and products for its newest run
    void reloadCatalog();                        // products for the chosen run / sector (they can differ)
    void applyLoaded(const Loaded&, bool newModel);
    void fillCategories();
    void fillProducts();
    void productChanged();                       // times for the product, then the picture and the loop
    void fillTimes(int latestSeconds);
    void showFrame();
    void selectIndex(int index);
    void prefetch(int generation);
    void refreshStrip();
    void startPlaying(bool on);
    void saveFrames();
    void notify(const string& message);          // a visible note above the picture; empty hides it
    UtilityCams::Run currentRun() const;
    const UtilityCams::Product * currentProduct() const;
    string currentSector() const;
    void closeEventCustom() override;

    VBox box;
    ControlRow rowTop;
    QLabel notice;
    ComboBox comboModel;
    ComboBox comboRun;
    ComboBox comboSector;
    ComboBox comboCategory;
    ComboBox comboProduct;
    QLabel labelValid;                             // "F01  Thu 2026-10-15 13Z": the forecast hour and the valid time of the picture
    QPushButton buttonSave;
    ZoomImage image;
    TimeStrip * strip{};                           // the forecast hours as the model screen has them: a timeline, the hours drawn, play and a grid of the hours
    QTimer playTimer;
    std::map<int, QByteArray> frames;              // by forecast seconds: the pictures drawn so far for the product on view
    std::set<int> inFlight;
    int prefetchGeneration{0};
    QByteArray shown;

    vector<std::pair<string, string>> modelList;   // id, display name
    UtilityCams::Model model;
    vector<UtilityCams::Run> runs;
    std::map<string, string> sectorNames;
    UtilityCams::Catalog catalog;
    vector<size_t> productRows;                    // catalog.products index behind each product-combo entry
    vector<string> categoryIds;                    // id behind each category-combo entry ("" = all)
    vector<int> times;                             // forecast seconds behind each time-combo entry

    int generation{0};
    bool closed{false};
    string lastFrameError;
};

#endif  // CAMSVIEWER_H
