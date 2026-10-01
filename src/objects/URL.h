// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef URL_H
#define URL_H

#include <string>
#include <QByteArray>
#include <QDateTime>
#include <QString>

using std::string;

class URL {
public:
    static string getText(const string&);
    static string getTextXmlAcceptHeader(const string&);
    static QByteArray getBytes(const string&);
    // like getBytes(), and also reports the HTTP status (0 = no response). A server may send an error body that
    // is itself a valid image (CAMs: a 404 with an "image not available" PNG), so callers that decode images
    // should require 2xx.
    static QByteArray getBytesWithStatus(const std::string& url, int& status);
    static QByteArray getBytesRange(const string&, long long, long long);

    // Where a downloaded picture came from and when the server says it was
    // produced (the HTTP Last-Modified header - the radar/outlook/satellite
    // servers all send it). Recorded by getBytes() and looked up later from the
    // image bytes themselves, so a screen that only holds the bytes (Photo,
    // Image, the save dialog) can still name the file by its real time without
    // every call site passing metadata around. The table is small and in
    // memory (hash -> url + time); bytes that were not downloaded here, or
    // whose entry has aged out, simply have no metadata.
    struct Meta {
        string url;
        QDateTime lastModified;   // invalid if the server sent none
        QDateTime fetched;        // when we downloaded it
    };
    static bool metaFor(const QByteArray& bytes, Meta& out);
};

#endif  // URL_H
