// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/SoundingPick.h"
#include "models/SoundingViewer.h"
#include "util/To.h"

SoundingPick::SoundingPick(Window * owner, std::function<std::pair<std::string, std::string>()> runAndHour, std::string titleBase)
    : owner{owner}
    , runAndHour{std::move(runAndHour)}
    , titleBase{std::move(titleBase)}
    , buttonSounding{owner, Icon::None, "Sounding"}
{
    buttonSounding.getView()->setToolTip("Click the map to pick a point, then open the RRFS model sounding (Skew-T, hodograph, "
                                         "parameters) there for the run and forecast hour selected now. A run/hour not "
                                         "fetched before downloads about 265 MB; further points at the same run and hour are instant.");
    buttonSounding.connect([this] { open(); });
}

void SoundingPick::pick(double pickedLon, double pickedLat) {
    havePoint = true;
    lon = pickedLon;
    lat = pickedLat;
    owner->setTitle(titleBase + " - sounding point " + To::string(lat) + " N, " + To::string(lon) + " E (press Sounding)");
}

void SoundingPick::open() {
    if (!havePoint) {
        owner->setTitle(titleBase + " - click the map to pick a sounding point first");
        return;
    }
    const auto [runId, forecastHour] = runAndHour();
    new SoundingViewer{owner, lon, lat, runId, forecastHour};
}
