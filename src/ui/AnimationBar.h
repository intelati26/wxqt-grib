// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ANIMATIONBAR_H
#define ANIMATIONBAR_H

#include <functional>
#include <string>
#include <vector>
#include <QByteArray>
#include <QSlider>
#include "objects/TimeLine.h"
#include "ui/Button.h"
#include "ui/ButtonToggle.h"
#include "ui/ComboBox.h"
#include "ui/HBox.h"
#include "ui/Shortcut.h"
#include "ui/Text.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::vector;

// Shared play / scrub / export-range / save bar for any screen that shows a
// chronological sequence of still images (RTMA hourly analyses, GRIB forecast
// hours, ...). Owns the UI and the playback timer; the owning screen supplies
// frame bytes on request and reacts to callbacks to update its own display.
//
// Frames the bar knows about ("loaded frames") are always a subset of
// `setAvailableLabels()`'s list, addressed two ways: a "local" index into the
// currently loaded set (0..frameCount()-1) and a "global" index into the full
// label list. `setFrames()` supplies the mapping between the two.
class AnimationBar {
public:
    AnimationBar(
        Window * parent,
        // Play or Save was pressed (or the export range changed while wanting a
        // save) and the loaded frames don't span [rangeStart, rangeEnd]. Fetch/
        // render whatever is needed and call setFrames() when done - synchronously
        // or later; on failure call cancelPending() instead.
        const function<void(int rangeStart, int rangeEnd)>& onRangeRequested,
        // a loaded frame became the one to show (playback tick, or a scrub that
        // landed on an already-loaded label). Use frameAt(localIndex).
        const function<void(int localFrameIndex)>& onFrameShown,
        // the slider was dragged to a label with no loaded frame; do a normal
        // single-image fetch for it (does not touch the loaded frame set).
        const function<void(int globalLabelIndex)>& onScrub,
        // Save was pressed. Never triggers a fetch: build an APNG from
        // loadedFrames() when frameCount() >= 2 (i.e. Play has already rendered
        // the current export range), otherwise save the screen's own single
        // current image. This is what makes the From/To range picker useful -
        // narrow it, press Play to render just that slice, then Save exports it.
        const function<void()>& onSave
    );

    void addTo(VBox&);

    // full chronological label list (e.g. run times or forecast hours, oldest
    // first); clears any loaded frames and resets the export range to all of it
    void setAvailableLabels(const vector<string>&);

    // fulfils the most recent onRangeRequested: frames[i] is the image for
    // availableLabels[globalLabelIndices[i]], both ascending
    void setFrames(const vector<QByteArray>& frames, const vector<int>& globalLabelIndices);
    void clearFrames();
    void cancelPending();
    void stopIfAnimating();

    // owner calls this after satisfying an onScrub() request, to keep the
    // slider in sync with the label it just displayed on its own
    void setSliderPosition(int globalLabelIndex);

    bool isAnimating() const;
    int frameCount() const;
    QByteArray frameAt(int localIndex) const;
    const vector<QByteArray>& loadedFrames() const;
    int rangeStartIndex() const;
    int rangeEndIndex() const;
    int findLocalIndex(int globalLabelIndex) const;   // -1 if not loaded
    int globalIndexAt(int localIndex) const;          // -1 if out of range

private:
    void toggleAnimation();
    void startPlayback();
    void loadAnimationFrame(int);
    void sliderMoved(int);
    void rangeChanged();
    bool coversRange(int start, int end) const;

    function<void(int, int)> onRangeRequested;
    function<void(int)> onFrameShown;
    function<void(int)> onScrub;
    function<void()> onSave;

    HBox rowMain;
    HBox rowRange;
    ButtonToggle buttonPlay;
    QSlider * sliderPosition;
    Button saveButton;
    Text labelFrom;
    ComboBox comboFrom;
    Text labelTo;
    ComboBox comboTo;
    Shortcut animateShortcut;
    Shortcut saveShortcut;
    TimeLine timeLine;

    vector<string> availableLabels;
    vector<QByteArray> frames;
    vector<int> frameLabelIndices;   // parallel to `frames`
    bool animating{false};
    bool pendingPlay{false};
};

#endif  // ANIMATIONBAR_H
