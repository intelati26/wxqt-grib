// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef URLANIMATION_H
#define URLANIMATION_H

#include <functional>
#include <string>
#include <vector>
#include <QByteArray>
#include "ui/AnimationBar.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::function;
using std::string;
using std::vector;

// Drop-in successor to ObjectAnimate for screens whose animation is "a list of
// image URLs, oldest first" (GOES, SPC mesoanalysis): the same play / scrub /
// From-To range / save bar (AnimationBar) and zoom/pan image (ZoomImage) the
// RTMA, GRIB and SPC Post viewers use, instead of the old single toggle button.
//
// Usage: keep `product` / `sector` current, call refresh() whenever they (or
// the frame count) change so the bar's timeline matches the new loop, and
// show the normal single "latest" image yourself. Frames are only downloaded
// when Play/Save is pressed or the slider is dragged to one.
class UrlAnimation {
public:
    // getFunction(product, sector, count) returns the frame URLs, oldest first
    UrlAnimation(Window * parent, ZoomImage * image,
                 const function<vector<string>(string, string, int)>& getFunction);

    void addTo(VBox&);
    // re-fetch the frame list for the current product / sector
    void refresh();
    void stopAnimateNoDownload();
    void setFrameCount(int);
    // no animation available for the current product: empty the bar
    void clear();
    // the still image the owner last put on screen - what Save exports when no
    // animation has been rendered
    void setCurrentBytes(const QByteArray&);

    string product;
    string sector;
    function<vector<string>(string, string, int)> getFunction;

private:
    void onRangeRequested(int start, int end);
    void onScrub(int globalIndex);
    void onSave();

    Window * parent;
    ZoomImage * image;
    AnimationBar animBar;
    int frameCount{12};
    int generation{0};
    vector<string> urls;
    vector<string> pendingUrls;
    vector<QByteArray> pendingFrames;
    vector<int> pendingIndices;
    QByteArray pendingSingle;
    QByteArray currentBytes;
};

#endif  // URLANIMATION_H
