// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "util/UtilityIO.h"
#include <thread>
#include <QFile>
#include <QNetworkAccessManager>
#include <QTextStream>
#include "common/GlobalVariables.h"
#include "external/bzlib.h"
#include "objects/File.h"
#include "objects/URL.h"

MemoryBuffer UtilityIO::uncompress(char * compressedData, unsigned int compressedFileSize) {
    unsigned int retSize = 2000000;
    QByteArray outputBuffer{retSize, '0'};
    BZ2_bzBuffToBuffDecompress(outputBuffer.data(), &retSize, compressedData, compressedFileSize, 1, 0);
    outputBuffer.resize(retSize);
    return MemoryBuffer{outputBuffer};
}

string UtilityIO::getHtml(const string& url) {
    return URL::getText(url);
}

string UtilityIO::getHtmlWithRetry(const string& url, size_t expectedMinSize) {
    auto html = getHtml(url);
    // std::cout << "RETRY: " << url << " " << html.size() << std::endl;
    if (html.size() < expectedMinSize) {
        std::this_thread::sleep_for(std::chrono::seconds{1});
        html = UtilityIO::getHtml(url);
    }
    return html;
}

QByteArray UtilityIO::downloadAsByteArray(const string& url) {
    return URL::getBytes(url);
}

string UtilityIO::readTextFile(const string& filePath) {
    return File::getText(filePath);
}

vector<string> UtilityIO::rawFileToStringArray(const string& filePath) {
    // return File{filePath}.getText().split(QString{GlobalVariables::newline});
    // above line seems to cause crash when starting radar - stack size issue?
    vector<string> stringList;
    QFile file;
    file.setFileName(QString::fromStdString(filePath));
    file.open(QIODevice::ReadOnly);
    QTextStream textStream{&file};
    while (true) {
        auto line = textStream.readLine();
        if (line.isNull()) {
            break;
        } else {
            stringList.push_back(line.toStdString());
        }
    }
    if (file.isOpen()) {
        file.close();
    }
    return stringList;
}

QByteArray UtilityIO::readBinaryFileFromResource(const string& srcFile) {
    return File::getBinaryDataFromResource(srcFile);
}
