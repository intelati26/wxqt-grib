// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef REFSPANEL_H
#define REFSPANEL_H

#include <cmath>
#include <functional>
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

// One cell of RefsViewer's 2x2 grid: a field-selection combo (Stage 0 - the
// full Mean/Spread/PMM/Probability/Paintball/"Member N" product combo lands
// in later stages, see docs/refs-viewer-plan.md) over a ZoomImage. Not a
// `Window` itself - built against its owning RefsViewer's Window* like any
// other widget in this app, and added into that window's own layout via
// addTo(). No forecast-hour control of its own - driven by RefsViewer's
// single master AnimationBar.
class RefsPanel {
public:
    explicit RefsPanel(Window * owner);

    void addTo(HBox& row);
    void setImage(const QByteArray& bytes);
    void setImageKeepView(const QByteArray& bytes);
    int fieldIndex() const;
    void setFieldIndex(int);

    // Threshold box, shown only for threshold-driven fields (Paintball /
    // Member Probability / REFS Probability). Blank or unparsable -> NaN,
    // which UtilityRefs::render() treats as the field's default.
    double threshold() const;
    // Call after the field combo changes: shows/hides the box and resets it
    // to the new field's default threshold (silently - no reload fired).
    void onFieldChanged();
    void connectThreshold(const std::function<void()>&);

    ComboBox& fieldCombo();
    ZoomImage& imageView();

private:
    VBox box;
    HBox rowHeader;
    ComboBox comboField;
    Entry entryThreshold;
    ZoomImage image;
};

#endif  // REFSPANEL_H
