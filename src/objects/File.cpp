// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/File.h"
#include <fstream>
#include <QFile>
#include <QTextStream>

string File::getText(const string& fileName) {
    QFile file;
    file.setFileName(QString::fromStdString(fileName));
    file.open(QIODevice::ReadOnly);
    QTextStream textStream{&file};
    auto stringValue = textStream.readAll();
    if (file.isOpen()) {
        file.close();
    }
    return stringValue.toStdString();
}

QByteArray File::getBinaryDataFromResource(const string& fileName) {
    QFile file{QString::fromStdString(fileName)};
    file.open(QIODevice::ReadOnly);
    auto data = file.readAll();
    if (file.isOpen()) {
        file.close();
    }
    return data;
}

void File::setText(const string& fileName, const string& data) {
    std::ofstream fileHandle{fileName};
    if (fileHandle.is_open()) {
        fileHandle << data;
        fileHandle.close();
    }
}
