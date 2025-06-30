// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "UtilityList.h"
#include "objects/WString.h"

vector<size_t> range(size_t count) {
    vector<size_t> list;
    for (size_t index = 0; index < count; index++) {
        list.push_back(index);
    }
    return list;
}

vector<size_t> range2(size_t from, size_t to) {
    vector<size_t> list;
    for (size_t i = from; i < to; i += 1) {
        list.push_back(i);
    }
    return list;
}

vector<size_t> range3(int from, int to, int by) {
    vector<size_t> list;
    for (int i = from; i < to; i += by) {
        list.push_back(i);
    }
    return list;
}

vector<size_t> range3(int from, size_t to, int by) {
    vector<size_t> list;
    for (size_t i = from; i < to; i += by) {
        list.push_back(i);
    }
    return list;
}

int findex(const string& value, const vector<string>& items) {
    for (auto index : range(items.size())) {
        if (WString::startsWith(items[index], value)) {
            return index;
        }
    }
    return 0;
}

int indexOf(const vector<string>& items, const string& value) {
    auto index = std::find(items.begin(), items.end(), value);
    if (index != items.end()) {
        return index - items.begin();
    }
    return 0;
}

int UtilityList::count(const vector<string>& items, const string& s) {
    return std::count(std::begin(items), std::end(items), s);
}

vector<string> UtilityList::reversed(const vector<string>& items) {
    vector<string> returnList = items;
    std::reverse(returnList.begin(), returnList.end());
    return returnList;
}
