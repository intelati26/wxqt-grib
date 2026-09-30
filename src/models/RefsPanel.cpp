// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/RefsPanel.h"
#include <QLineEdit>
#include <QSignalBlocker>
#include "models/UtilityRefs.h"

RefsPanel::RefsPanel(Window * owner)
    : comboKind{owner, UtilityRefs::kindLabels()}
    , comboVariable{owner}
    , comboMember{owner, {"Member 1", "Member 2", "Member 3", "Member 4", "Member 5"}}
    , entryThreshold{owner}
    , image{owner}
    , hoverLabel{new QLabel{&image}}
{
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet(
        "QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2;"
        " padding: 3px 6px; border-radius: 3px; }");
    hoverLabel->hide();
    entryThreshold.getView()->setMaximumWidth(110);
    entryThreshold.getView()->setToolTip("Exceedance threshold - press Enter to apply");
    fillVariables(0);
    rowHeader.addWidget(comboKind);
    rowHeader.addWidget(comboVariable, 1);
    rowHeader.addWidget(comboMember);
    rowHeader.addWidget(entryThreshold);
    box.addLayout(rowHeader);
    box.addWidgetReal(&image, 1, Qt::Alignment{});   // default alignment would size it to its minimum height
    onFieldChanged();
}

void RefsPanel::addTo(HBox& row) {
    row.addLayout(box, 1);
}

void RefsPanel::setImage(const QByteArray& bytes) {
    image.setBytes(bytes);
}

void RefsPanel::setImageKeepView(const QByteArray& bytes) {
    image.setBytesKeepView(bytes);
}

// (Re)fills the variable picker for a kind and remembers which
// UtilityRefs::fields row each entry stands for.
void RefsPanel::fillVariables(int kind) {
    variableFields = UtilityRefs::kindFieldIndices(kind);
    vector<string> labels;
    for (const auto index : variableFields) {
        labels.push_back(UtilityRefs::variableLabel(index));
    }
    comboVariable.block();
    comboVariable.setList(labels);
    comboVariable.setIndex(0);
    comboVariable.unblock();
    comboMember.setVisible(kind == 1);
}

int RefsPanel::fieldIndex() const {
    const auto variable = comboVariable.getIndex();
    if (variable < 0 || variable >= static_cast<int>(variableFields.size())) {
        return 0;
    }
    const auto base = variableFields[variable];
    return UtilityRefs::isMemberField(base) ? UtilityRefs::memberFieldIndex(base, comboMember.getIndex() + 1) : base;
}

void RefsPanel::setFieldIndex(int index) {
    const auto kind = UtilityRefs::kindOf(index);
    comboKind.block();
    comboKind.setIndex(static_cast<size_t>(kind));
    comboKind.unblock();
    fillVariables(kind);
    // member rows collapse to their member-1 representative in the list
    const auto wanted = UtilityRefs::isMemberField(index) ? UtilityRefs::memberFieldIndex(index, 1) : index;
    for (size_t i = 0; i < variableFields.size(); i += 1) {
        if (variableFields[i] == wanted) {
            comboVariable.block();
            comboVariable.setIndex(i);
            comboVariable.unblock();
        }
    }
    if (UtilityRefs::isMemberField(index)) {
        comboMember.block();
        comboMember.setIndex(static_cast<size_t>(UtilityRefs::memberOf(index) - 1));
        comboMember.unblock();
    }
    onFieldChanged();
}

void RefsPanel::connectField(const std::function<void()>& fn) {
    comboKind.connect([this, fn] {
        fillVariables(comboKind.getIndex());
        onFieldChanged();
        fn();
    });
    comboVariable.connect([this, fn] {
        onFieldChanged();
        fn();
    });
    comboMember.connect(fn);
}

double RefsPanel::threshold() const {
    bool ok = false;
    const auto value = QString::fromStdString(entryThreshold.getText()).trimmed().toDouble(&ok);
    return ok ? value : std::nan("");
}

void RefsPanel::onFieldChanged() {
    const auto index = fieldIndex();
    const auto uses = UtilityRefs::usesThreshold(index);
    auto * view = entryThreshold.getView();
    view->setVisible(uses);
    if (uses) {
        const QSignalBlocker blocker{view};
        view->setText(QString::number(UtilityRefs::defaultThreshold(index), 'g', 6));
        view->setPlaceholderText(QString::fromStdString(UtilityRefs::thresholdUnits(index)));
    }
}

void RefsPanel::setHoverText(const QString& text) {
    if (text.isEmpty()) {
        hoverLabel->hide();
        return;
    }
    hoverLabel->setText(text);
    hoverLabel->adjustSize();
    hoverLabel->move(8, 8);
    hoverLabel->show();
    hoverLabel->raise();
}

void RefsPanel::connectThreshold(const std::function<void()>& fn) {
    QObject::connect(entryThreshold.getView(), &QLineEdit::editingFinished, entryThreshold.getView(), fn);
}

ZoomImage& RefsPanel::imageView() {
    return image;
}
