// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSTRING_H
#define UTILITYSTRING_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilityString {
public:
    static string parse(const string&, const string&);
    static string parseMultiLineLastMatch(const string&, const string&);
    static vector<string> parseTwo(const string&, const string&);
    static vector<string> parseColumn(const string&, const string&);
    static string extractPreLsr(const string&);
    static string getLastXChars(const string&, int);
    static string removeHtml(const string&);
    static string insert(const string&, size_t, const string&);
    static string truncate(const string&, size_t);
    static string substring(const string&, int, int = -1);
    static string addPeriodBeforeLastTwoChars(const string&);
    static string title(const string&);
    static string parseNwsPre(const string&);
    static vector<string> parseXml(const string&, const string&);
    static vector<string> parseXmlExt(const vector<string>&, const string&);
    static vector<string> parseXmlValue(const string&);
    static bool match(const string&, const string&);
    static string replaceRegex(const string&, const string&, const string&);
    static string parseBetweenTokens(const string&, const string&, const string&);
};

#endif  // UTILITYSTRING_H
