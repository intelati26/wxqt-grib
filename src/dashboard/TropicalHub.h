// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef TROPICALHUB_H
#define TROPICALHUB_H

#include <memory>
#include <string>
#include <vector>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;
using std::vector;

// The Tropical Hub: one screen for what is happening in the NHC basins (Atlantic, East and Central Pacific) - a card for each active storm
// (and each invest) with its numbers and buttons for its track map / recon, its advisory products and its discussion; the Tropical Weather
// Outlook's areas of possible development; the recon Plan of the Day; the season so far against the 1991-2020 average - and buttons for the
// detailed screens (season charts, recon plan, vortex messages, the older Tropical and Climate screens).
class TropicalHub : public Window {
public:
    explicit TropicalHub(Window * parent);

private:
    void load();
    void fillStorms();
    void fillOutlook();
    void fillPod();
    void fillSeasons();
    void fillLocations();
    QWidget * card(const HurricaneData::StormEntry& entry, const string& basin);
    void closeEventCustom() override { closed = true; }

    VBox box;
    HBox rowTop;
    Button buttonRefresh;
    Button buttonTracks;
    Button buttonSeason;
    Button buttonAce;
    Button buttonPod;
    Button buttonTropical;
    Button buttonClimate;
    Text textStatus;
    QWidget * content{};
    QVBoxLayout * stormsLayout{};
    QVBoxLayout * outlookLayout{};
    QVBoxLayout * podLayout{};
    QVBoxLayout * seasonLayout{};
    QVBoxLayout * locationsLayout{};
    struct BasinStorms {
        string basin;                                       // "al" "ep" "cp"
        vector<HurricaneData::StormEntry> entries;
        string error;
        bool loaded{false};
    };
    vector<BasinStorms> storms;
    std::shared_ptr<HurricaneData::OutlookData> outlook;
    std::shared_ptr<HurricaneData::PodData> pod;
    std::shared_ptr<HurricaneData::WspData> wsp;
    std::shared_ptr<HurricaneData::SeasonData> seasonAtlantic;
    std::shared_ptr<HurricaneData::SeasonData> seasonPacific;
    int generation{0};
    bool closed{false};
};

#endif  // TROPICALHUB_H
