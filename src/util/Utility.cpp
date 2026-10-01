// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <mutex>
#include "util/Utility.h"
#include <QString>
#include "MyApplication.h"
#include "util/To.h"

namespace {
    // The one QSettings object is read by download threads (the User-Agent, model code, ...) while the UI thread writes it
    // (radar position, window geometry, ...). QSettings is not safe for that on a single object, so every access holds this.
    std::mutex preferencesMutex;
}

string Utility::readPref(const string& key, const string& value) {
    const std::lock_guard<std::mutex> guard{preferencesMutex};
    return MyApplication::preferences->value(QString::fromStdString(key), QString::fromStdString(value)).toString().toStdString();
}

int Utility::readPrefInt(const string& key, int value) {
    const std::lock_guard<std::mutex> guard{preferencesMutex};
    return To::Int(MyApplication::preferences->value(QString::fromStdString(key), QString::fromStdString(To::string(value))).toString().toStdString());
}

void Utility::writePref(const string& key, const string& value) {
    const std::lock_guard<std::mutex> guard{preferencesMutex};
    const auto name = QString::fromStdString(key);
    const auto text = QString::fromStdString(value);
    if (MyApplication::preferences->contains(name) && MyApplication::preferences->value(name).toString() == text) {
        return;   // unchanged: no disk write
    }
    MyApplication::preferences->setValue(name, text);
    MyApplication::preferences->sync();
}

void Utility::writePrefInt(const string& key, int value) {
    writePref(key, To::string(value));
}

vector<string> Utility::prefGetAllKeys() {
    const std::lock_guard<std::mutex> guard{preferencesMutex};
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
