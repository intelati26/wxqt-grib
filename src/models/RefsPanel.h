// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef REFSPANEL_H
#define REFSPANEL_H

#include <cmath>
#include <functional>
#include <QLabel>
#include "ui/ComboBox.h"
#include "ui/Entry.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

// One cell of RefsViewer's 2x2 grid: three linked pickers over a ZoomImage -
// a kind (REFS mean/spread/PMM, a single RRFS Ensemble member, paintball,
// member probability, REFS probability), a variable within that kind, and -
// for the member kind only - which member. Together they select one
// `UtilityRefs::fields` row (fieldIndex()). Not a `Window` itself - built
// against its owning RefsViewer's Window* like any other widget in this app,
// and added into that window's own layout via addTo(). No forecast-hour
// control of its own - driven by RefsViewer's single master AnimationBar.
class RefsPanel {
public:
    explicit RefsPanel(Window * owner);

    void addTo(HBox& row);
    void setImage(const QByteArray& bytes);
    void setImageKeepView(const QByteArray& bytes);
    int fieldIndex() const;
    void setFieldIndex(int);
    // fires after ANY picker changes (the threshold box is reset first)
    void connectField(const std::function<void()>&);

    // Threshold box, shown only for threshold-driven fields (Paintball /
    // Member Probability / REFS Probability). Blank or unparsable -> NaN,
    // which UtilityRefs::render() treats as the field's default.
    double threshold() const;
    // Shows/hides the threshold box and resets it to the selected field's
    // default threshold (silently - no reload fired). Called automatically
    // on picker changes; also call after setFieldIndex().
    void onFieldChanged();
    void connectThreshold(const std::function<void()>&);
    // hover read-out overlay (top-left of the image); empty text hides it
    void setHoverText(const QString&);

    ZoomImage& imageView();

private:
    void fillVariables(int kind);

    VBox box;
    HBox rowHeader;
    ComboBox comboKind;
    ComboBox comboVariable;
    ComboBox comboMember;
    Entry entryThreshold;
    ZoomImage image;
    QLabel * hoverLabel;
    vector<int> variableFields;   // UtilityRefs::fields index behind each variable entry
};

#endif  // REFSPANEL_H
