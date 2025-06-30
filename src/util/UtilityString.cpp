// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "util/UtilityString.h"
#include <QtGlobal>
#include <QRegularExpression>
#include <regex>
#include "common/GlobalVariables.h"
#include "objects/WString.h"

// bool UtilityString::match(const QString& data, const QString& regexp) {
//     auto re = QRegularExpression(regexp, QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption);
//     QRegularExpressionMatch match = re.match(data);
//     if (match.hasMatch()) {
//         return true;
//     }
//     return false;
// }


string UtilityString::parse(const string& data, const string& regexp) {
    const QRegularExpression re{QString::fromStdString(regexp), QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};
    const QRegularExpressionMatch match = re.match(QString::fromStdString(data));
    if (match.hasMatch()) {
        const auto matched = match.captured(1);
        return matched.toStdString();
    } else {
        return "";
    }
}

string UtilityString::parseMultiLineLastMatch(const string& data, const string& match) {
    const auto stringList = parseColumn(data, match);
    if (!stringList.empty()) {
        return stringList.back();
    } else {
        return "";
    }
}

vector<string> UtilityString::parseTwo(const string& data, const string& regexp) {
    const QRegularExpression re{QString::fromStdString(regexp), QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};
    const QRegularExpressionMatch match = re.match(QString::fromStdString(data));
    if (match.hasMatch()) {
        const auto matched1 = match.captured(1).toStdString();
        const auto matched2 = match.captured(2).toStdString();
        return {matched1, matched2};
    } else {
        return {"", ""};
    }
}

vector<string> UtilityString::parseColumn(const string& data, const string& regexp) {
    const QRegularExpression re{QString::fromStdString(regexp), QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};
    QRegularExpressionMatchIterator i = re.globalMatch(QString::fromStdString(data));
    vector<string> words;
    while (i.hasNext()) {
        const QRegularExpressionMatch match = i.next();
        auto word = match.captured(1);
        words.push_back(word.toStdString());
    }
    return words;
}

// string UtilityString::extractPreLsr(const string& htmlF) {
//     const QString separator = "ABC123E";
//     auto html = QString::fromStdString(htmlF);
//     const auto htmlOneLine = html.replace(QString::fromStdString(GlobalVariables::newline), separator);
//     auto parsedText = parse(htmlOneLine, QString::fromStdString(GlobalVariables::prePattern));
//     return parsedText.replace(separator, QString::fromStdString(GlobalVariables::newline)).toStdString();
// }

string UtilityString::extractPreLsr(const string& s) {
    const string seperator("ABC123E");
    const auto htmlOneLine = WString::replace(s, GlobalVariables::newline, seperator);
    const auto parsedText = parse(htmlOneLine, "<pre.*?>(.*?)</pre>");
    return WString::replace(parsedText, seperator, GlobalVariables::newline);
}

string UtilityString::getLastXChars(const string& data, int count) {
    return QString::fromStdString(data).right(count).toStdString();
}

string UtilityString::removeHtml(const string& htmlF) {
    const QRegularExpression re{"<.*?>"};
    auto html = QString::fromStdString(htmlF);
    html = html.replace(re, "");
    return html.toStdString();
}

string UtilityString::insert(const string& originalString, size_t index, const string& stringToAdd) {
    auto s = QString::fromStdString(originalString);
    s.insert(index, QString::fromStdString(stringToAdd));
    return s.toStdString();
}

string UtilityString::truncate(const string& originalString, size_t length) {
    auto s = QString::fromStdString(originalString);
    s.truncate(length);
    return s.toStdString();
}

string UtilityString::substring(const string& s, int start, int end) {
    if (end == -1) {
        try {
            return s.substr(start);
        } catch(std::out_of_range& exception) {
            return s;
        }
    } else {
        try {
            return s.substr(start, end - start);
        } catch(std::out_of_range& exception) {
            return s;
        }
    }
}

string UtilityString::addPeriodBeforeLastTwoChars(const string& data) {
    const auto index = static_cast<int>(data.size()) - 2;
    return insert(data, index, ".");
}

string UtilityString::title(const string& s) {
    // #if QT_VERSION >= 0x060000
    #if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
        QStringList parts = QString::fromStdString(s).split(' ', Qt::SkipEmptyParts);
    #else
        QStringList parts = QString::fromStdString(s).split(' ', QString::SkipEmptyParts);
    #endif
    for (auto& part : parts) {
        part.replace(0, 1, part[0].toUpper());
    }
    return parts.join(" ").toStdString();
}

string UtilityString::parseNwsPre(const string& html) {
    const auto lines = WString::split(html, GlobalVariables::newline);
    auto preFound = false;
    auto endPreFound = false;
    vector<string> modifiedLines;
    for (const auto& line : lines) {
        if (WString::contains(line, "<pre>")) {
            preFound = true;
            continue;
        }
        if (WString::contains(line, "</pre>")) {
            endPreFound = true;
        }
        if (preFound && !endPreFound) {
            modifiedLines.push_back(line);
        }
    }
    return WString::join(modifiedLines, GlobalVariables::newline);
}

vector<string> UtilityString::parseXml(const string& payloadF, const string& delim) {
    auto payload = payloadF;
    if (delim == "start-valid-time") {
        payload = replaceRegex(payload, "<end-valid-time>.*?</end-valid-time>", "");
        payload = replaceRegex(payload, "<layout-key>.*?</layout-key>", "");
    }
    payload = replaceRegex(payload, "<name>.*?</name>" , "");
    payload = replaceRegex(payload, "</" + delim + ">" , "");
    return WString::split(payload, "<" + delim + ">");
}

vector<string> UtilityString::parseXmlExt(const vector<string>& regexpList, const string& html) {
    vector<string> items;
    for (const auto& reg : regexpList) {
        items.push_back(parse(html, reg));
    }
    return items;
}

vector<string> UtilityString::parseXmlValue(const string& payloadF) {
    auto payload = payloadF;
    payload = replaceRegex(payload, "<name>.*?</name>" , "");
    payload = replaceRegex(payload, "</value>" , "");
    return WString::split(payload, GlobalVariables::xmlValuePattern);
}

bool UtilityString::match(const string& s, const string& regexp) {
    return regex_match(s, std::regex(regexp));
}

string UtilityString::replaceRegex(const string& s, const string& regexp, const string& newString) {
    return regex_replace(s, std::regex(regexp), newString);
}

string UtilityString::parseBetweenTokens(const string& s, const string& t1, const string& t2) {
    const auto pos1 = s.find(t1);
    const auto pos2 = s.find(t2);
    if (pos1 < pos2 && pos1 < s.size()) {
        return s.substr(pos1 + t1.size(), pos2 - pos1 - t1.size());
    }
    return "";
}

// QString UtilityString::parse(const QString& data, const QString& regexp) {
//     const QRegularExpression re{regexp, QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption};
//     const QRegularExpressionMatch match = re.match(data);
//     if (match.hasMatch()) {
//         return match.captured(1);
//     } else {
//         return "";
//     }
// }
