// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/AnimationBar.h"
#include <algorithm>
#include <QObject>

namespace {
    constexpr int frameDelayMs = 400;   // per-frame dwell for playback and any exported APNG
}

AnimationBar::AnimationBar(
    Window * parent,
    const function<void(int, int)>& onRangeRequested,
    const function<void(int)>& onFrameShown,
    const function<void(int)>& onScrub,
    const function<void()>& onSave
)
    : onRangeRequested{onRangeRequested}
    , onFrameShown{onFrameShown}
    , onScrub{onScrub}
    , onSave{onSave}
    , buttonPlay{parent, Play, "Animate (A)"}
    , sliderPosition{new QSlider{Qt::Horizontal, parent}}
    , saveButton{parent, None, "Save Image (S)"}
    , labelFrom{parent, "Range:"}
    , comboFrom{parent}
    , labelTo{parent, "to"}
    , comboTo{parent}
    , animateShortcut{QKeySequence{"A"}, parent}
    , saveShortcut{QKeySequence{"S"}, parent}
    , timeLine{parent, 1000, 1, [this] (int i) { loadAnimationFrame(i); }}
{
    buttonPlay.connect([this] { toggleAnimation(); });
    animateShortcut.connect([this] { toggleAnimation(); });
    saveButton.connect([this] { this->onSave(); });
    saveShortcut.connect([this] { this->onSave(); });

    QObject::connect(sliderPosition, &QSlider::valueChanged, parent, [this] (int v) { sliderMoved(v); });
    comboFrom.connect([this] { rangeChanged(); });
    comboTo.connect([this] { rangeChanged(); });

    rowMain.addWidget(buttonPlay);
    rowMain.addWidgetReal(sliderPosition, 1, Qt::Alignment{});
    rowMain.addWidget(saveButton);
    rowRange.addWidget(labelFrom);
    rowRange.addWidget(comboFrom);
    rowRange.addWidget(labelTo);
    rowRange.addWidget(comboTo);
    rowRange.addStretch();
}

void AnimationBar::addTo(VBox& box) {
    box.addLayout(rowMain);
    box.addLayout(rowRange);
}

void AnimationBar::setAvailableLabels(const vector<string>& labels) {
    availableLabels = labels;
    clearFrames();

    comboFrom.block();
    comboTo.block();
    comboFrom.setList(labels);
    comboTo.setList(labels);
    if (!labels.empty()) {
        comboFrom.setIndex(0);
        comboTo.setIndex(labels.size() - 1);
    }
    comboFrom.unblock();
    comboTo.unblock();

    sliderPosition->blockSignals(true);
    sliderPosition->setRange(0, std::max(0, static_cast<int>(labels.size()) - 1));
    sliderPosition->setValue(0);
    sliderPosition->blockSignals(false);
}

void AnimationBar::setFrames(const vector<QByteArray>& newFrames, const vector<int>& globalLabelIndices) {
    frames = newFrames;
    frameLabelIndices = globalLabelIndices;
    if (pendingPlay) {
        pendingPlay = false;
        startPlayback();
    }
}

void AnimationBar::clearFrames() {
    frames.clear();
    frameLabelIndices.clear();
}

void AnimationBar::cancelPending() {
    pendingPlay = false;
    stopIfAnimating();
}

void AnimationBar::stopIfAnimating() {
    if (timeLine.isRunning()) {
        timeLine.stop();
    }
    animating = false;
    buttonPlay.setActive(false);
    buttonPlay.setText("");
}

void AnimationBar::setSliderPosition(int globalLabelIndex) {
    sliderPosition->blockSignals(true);
    sliderPosition->setValue(globalLabelIndex);
    sliderPosition->blockSignals(false);
}

bool AnimationBar::isAnimating() const {
    return animating;
}

int AnimationBar::frameCount() const {
    return static_cast<int>(frames.size());
}

QByteArray AnimationBar::frameAt(int localIndex) const {
    if (localIndex < 0 || localIndex >= static_cast<int>(frames.size())) {
        return {};
    }
    return frames[localIndex];
}

const vector<QByteArray>& AnimationBar::loadedFrames() const {
    return frames;
}

int AnimationBar::rangeStartIndex() const {
    return std::max(0, comboFrom.getIndex());
}

int AnimationBar::rangeEndIndex() const {
    return std::max(0, comboTo.getIndex());
}

int AnimationBar::findLocalIndex(int globalLabelIndex) const {
    for (size_t i = 0; i < frameLabelIndices.size(); i += 1) {
        if (frameLabelIndices[i] == globalLabelIndex) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int AnimationBar::globalIndexAt(int localIndex) const {
    if (localIndex < 0 || localIndex >= static_cast<int>(frameLabelIndices.size())) {
        return -1;
    }
    return frameLabelIndices[localIndex];
}

bool AnimationBar::coversRange(int start, int end) const {
    return !frameLabelIndices.empty() && frameLabelIndices.front() <= start && frameLabelIndices.back() >= end;
}

void AnimationBar::toggleAnimation() {
    if (animating) {
        stopIfAnimating();
        return;
    }
    if (coversRange(rangeStartIndex(), rangeEndIndex())) {
        startPlayback();
    } else {
        pendingPlay = true;
        buttonPlay.setActive(true);
        buttonPlay.setText("Loading...");
        onRangeRequested(rangeStartIndex(), rangeEndIndex());
    }
}

void AnimationBar::startPlayback() {
    if (frames.size() < 2) {
        buttonPlay.setActive(false);
        buttonPlay.setText("");
        return;
    }
    animating = true;
    buttonPlay.setActive(true);
    buttonPlay.setText("Stop (A)");
    timeLine.setCount(static_cast<int>(frames.size()));
    timeLine.setSpeed(static_cast<int>(frames.size()) * frameDelayMs);
    timeLine.start();
}

void AnimationBar::loadAnimationFrame(int i) {
    if (frames.empty()) {
        return;
    }
    const auto count = static_cast<int>(frames.size());
    const auto idx = ((i % count) + count) % count;
    if (idx < static_cast<int>(frameLabelIndices.size())) {
        setSliderPosition(frameLabelIndices[idx]);
    }
    onFrameShown(idx);
}

void AnimationBar::sliderMoved(int globalIndex) {
    if (animating) {
        stopIfAnimating();
    }
    const auto local = findLocalIndex(globalIndex);
    if (local >= 0) {
        onFrameShown(local);
    } else {
        onScrub(globalIndex);
    }
}

void AnimationBar::rangeChanged() {
    if (comboTo.getIndex() < comboFrom.getIndex()) {
        comboTo.block();
        comboTo.setIndex(comboFrom.getIndex());
        comboTo.unblock();
    }
    stopIfAnimating();
    clearFrames();
}
