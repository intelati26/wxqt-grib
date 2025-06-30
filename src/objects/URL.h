// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef URL_H
#define URL_H

#include <string>
#include <QByteArray>
#include <QString>

using std::string;

class URL {
public:
    static string getText(const string&);
    static string getTextXmlAcceptHeader(const string&);
    static QByteArray getBytes(const string&);
};

#endif  // URL_H
