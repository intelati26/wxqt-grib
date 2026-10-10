// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ComboBox.h"
#include <algorithm>
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityList.h"
#include "util/UtilityUI.h"

ComboBox::ComboBox(Window * parent, const vector<string>& items)
    : comboBox{new QComboBox{parent}}
    , parent{parent}
{
    for (const auto& s : items) {
        this->items.push_back(s);
    }
    comboBox->setMaxVisibleItems(UIPreferences::comboBoxSize);
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    addItemsQt();
}

void ComboBox::setIndex(size_t index) {
    comboBox->setCurrentIndex(index);
}

int ComboBox::getIndex() const {
    return comboBox->currentIndex();
}

void ComboBox::setIndexByPref(const string& s, int i) {
    setIndex(Utility::readPrefInt(s, i));
}

void ComboBox::setIndexByValue(const string& item) {
    const auto index = findex(item, items);
    comboBox->setCurrentIndex(index);
}

void ComboBox::setList(const vector<string>& items) {
    comboBox->clear();
    this->items.clear();
    for (const auto& s : items) {
        this->items.push_back(s);
    }
    addItemsQt();
}

vector<string> ComboBox::getItems() const {
    return items;
}

// GTK compat
void ComboBox::block() {
    blockSignals(true);
}

// GTK compat
void ComboBox::unblock() {
    blockSignals(false);
}

void ComboBox::blockSignals(bool b) {
    comboBox->blockSignals(b);
}

void ComboBox::connect(const function<void()>& fn) {
    QObject::connect(comboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), parent, fn);
}

QComboBox * ComboBox::getView() {
    return comboBox;
}

void ComboBox::setVisible(bool b) {
    comboBox->setVisible(b);
}

void ComboBox::setLabels(const vector<string>& labels) {
    for (size_t i = 0; i < labels.size() && static_cast<int>(i) < comboBox->count(); i++) {
        comboBox->setItemText(static_cast<int>(i), QString::fromStdString(labels[i]));
    }
}

string ComboBox::getValue() const {
    const auto index = comboBox->currentIndex();
    return index >= 0 && index < static_cast<int>(items.size()) ? items[static_cast<size_t>(index)] : comboBox->currentText().toStdString();
}

void ComboBox::addItemsQt() {
    QVector<QString> comboItemsQt;
    std::transform(items.begin(), items.end(), std::back_inserter(comboItemsQt), [](const string& v){ return QString::fromStdString(v); });
    comboBox->addItems(comboItemsQt);
}
