// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/NumberPicker.h"
#include <QtGlobal>
#include <QObject>
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

NumberPicker::NumberPicker(Window * parent, const string& label, const string& pref, int defaultValue, int low, int up, int step)
    : pref{pref}
    , defaultValue{defaultValue}
    , text{parent, label}
    , qSpinBox{new QSpinBox{parent}}
{
    text.setWordWrap(false);
    box.addWidgetReal(qSpinBox);
    box.addWidget(text);

    qSpinBox->setRange(low, up);
    qSpinBox->setSingleStep(step);
    qSpinBox->setValue(getCurrentValue());

    // qSpinBox.textChanged.connect(this, "updateNp(String)");
    // connect(get(), &QSpinBox::textChanged, this, [this] (QString s) { updateNp(s); });

// #if QT_VERSION >= 0x051400 //0x060000
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
    QObject::connect(qSpinBox, static_cast<void (QSpinBox::*)(const QString &)>(&QSpinBox::textChanged), parent, [this] (const QString& val) { updateNp(val); });
#else
    QObject::connect(qSpinBox, static_cast<void (QSpinBox::*)(const QString &)>(&QSpinBox::valueChanged), parent, [this] (QString val) { updateNp(val); });
#endif
}

void NumberPicker::updateNp(const QString& newValue) {
    Utility::writePref(pref, newValue.toStdString());
    RadarPreferences::initialize();
    UIPreferences::initialize();
}

int NumberPicker::getCurrentValue() {
    return Utility::readPrefInt(pref, defaultValue);
}

QBoxLayout * NumberPicker::getView() {
    return box.getView();
}
