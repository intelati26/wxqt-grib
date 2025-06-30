// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef FILE_H
#define FILE_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QString>

using std::string;
using std::vector;

class File {
public:
    static string getText(const string&);
    static QByteArray getBinaryDataFromResource(const string&);
    static void setText(const string&, const string&);
};

#endif  // FILE_H
