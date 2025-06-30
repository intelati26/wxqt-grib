// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WSTRING_H
#define WSTRING_H

// #include <algorithm>
#include <string>
#include <vector>

using std::string;
using std::vector;

class WString : public string {
public:
//    WString();
//    explicit WString(const char* c) : string(c) {}
//    // don't mark explicit
//    WString(const string& s) : string(s) {}
//    virtual ~WString()= default;;
//    WString toUpper();
//    WString toLower();
//    WString replace(const string&, const string&);
//    WString& operator = (const string& s) { return (WString&)this->assign(s); }
//    WString& operator = (const char* c) { return (WString&)this->assign(c); }
    static string fixedLengthString(const string&, size_t);
    static string fixedLengthStringPad0(const string&, size_t);
    static string replace(const string&, const string&, const string&);
    static vector<string> split(const string&, const string&);
    static bool startsWith(const string&, const string&);
    static bool endsWith(const string&, const string&);
    static string strip(const string&);
//    static string lstrip(const string&);
//    static string rstrip(const string&);
    static string toLower(const string&);
    static string toUpper(const string&);
    static string join(const vector<string>&, const string&);
    static bool contains(const string&, const string&);
};

#endif  // WSTRING_H
