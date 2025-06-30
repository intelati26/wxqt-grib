// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "util/Utility.h"
#include <QString>
#include "MyApplication.h"
#include "util/To.h"

string Utility::readPref(const string& key, const string& value) {
    return MyApplication::preferences->value(QString::fromStdString(key), QString::fromStdString(value)).toString().toStdString();
}

int Utility::readPrefInt(const string& key, int value) {
    return To::Int(MyApplication::preferences->value(QString::fromStdString(key), QString::fromStdString(To::string(value))).toString().toStdString());
}

void Utility::writePref(const string& key, const string& value) {
    MyApplication::preferences->setValue(QString::fromStdString(key), QString::fromStdString(value));
    MyApplication::preferences->sync();
}

void Utility::writePrefInt(const string& key, int value) {
    MyApplication::preferences->setValue(QString::fromStdString(key), QString::fromStdString(To::string(value)));
    MyApplication::preferences->sync();
}

vector<string> Utility::prefGetAllKeys() {
    const auto items = MyApplication::preferences->allKeys();
    vector<string> stringList;
    for (const auto& item : items) {
        stringList.push_back(item.toStdString());
    }
    return stringList;
}

string Utility::safeGet(const vector<string>& items, size_t index) {
    return items.size() <= index ? "" : items[index];
}
