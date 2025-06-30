// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYIO_H
#define UTILITYIO_H

#include <string>
#include <vector>
#include "objects/MemoryBuffer.h"

using std::string;
using std::vector;

class UtilityIO {
public:
    static MemoryBuffer uncompress(char *, unsigned int);
    static string getHtml(const string&);
    static string getHtmlWithRetry(const string&, size_t);
    static string readTextFile(const string&);
    static vector<string> rawFileToStringArray(const string&);
    static QByteArray downloadAsByteArray(const string&);
    static QByteArray readBinaryFileFromResource(const string&);
};

#endif  // UTILITYIO_H
