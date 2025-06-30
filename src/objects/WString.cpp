// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "WString.h"
#include <algorithm>
#include <regex>

// WString::WString() = default;
//
// WString WString::toUpper() {
//     return WString::to_upper(*this);
// }
//
// WString WString::toLower() {
//     return WString::to_lower(*this);
// }
//
// WString WString::replace(const string& a, const string& b) {
//    return WString::replace(*this, a, b);
// }

string WString::fixedLengthString(const string& s, size_t width) {
    if (s.size() > width) {
        return s;
    }
    return s + string(width - s.size(), ' ');
}

string WString::fixedLengthStringPad0(const string& s, size_t width) {
    if (s.size() > width) {
        return s;
    }
    return string(width - s.size(), '0') + s;
}

// https://stackoverflow.com/questions/3418231/replace-part-of-a-string-with-another-string
string WString::replace(const string& s, const string& from, const string& to) {
    auto str = s;
    if (from.empty()) {
        return str;
    }
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length();  // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
    return str;
}

string WString::toLower(const string& tmp) {
    auto s = tmp;
    transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

string WString::toUpper(const string& tmp) {
    auto s = tmp;
    transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

vector<string> WString::split(const string &inputS, const string &delim) {
    auto s = inputS;
    vector<string> items;
    size_t pos;
    string token;
    while ((pos = s.find(delim)) != string::npos) {
        token = s.substr(0, pos);
        items.push_back(token);
        s.erase(0, pos + delim.length());
    }
    items.push_back(s);
    return items;
}

bool WString::startsWith(const string &s, const string &prefix) {
    if (s.rfind(prefix, 0) == 0) {
        return true;
    } else {
        return false;
    }
}

// thanks https://stackoverflow.com/questions/874134/find-out-if-string-ends-with-another-string-in-c
bool WString::endsWith(const string &s, const string &suffix) {
    if (s.length() >= suffix.length()) {
        return (0 == s.compare (s.length() - suffix.length(), suffix.length(), suffix));
    } else {
        return false;
    }
}

string WString::strip(const string &s) {
    return std::regex_replace(s, std::regex("^ +| +$|( ) +"), "$1");
}

// string WString::lstrip(const string &s) {
//    return std::regex_replace(s, std::regex("^ +"), "");
// }

// string WString::rstrip(const string &s) {
//    return std::regex_replace(s, std::regex(" +$"), "");
// }

string WString::join(const vector<string>& items, const string& s) {
    string returnString;
    for (const auto& item : items) {
        returnString += item + s;
    }
    return strip(returnString);
}

//
// does string s2 exist in string s1
//
bool WString::contains(const string& s1, const string& s2) {
    if (s1.find(s2) != string::npos) {
        return true;
    } else {
        return false;
    }
}
