// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "FileStorage.h"
#include "objects/ObjectDateTime.h"

FileStorage::FileStorage()
    : obsDownloadTimer{"OBS_AND_WIND_BARBS" + QString::number(ObjectDateTime::currentTimeMillis()).toStdString()}
{}

void FileStorage::clearBuffers() {
    animationMemoryBuffer.clear();
}

void FileStorage::setMemoryBuffer(const QByteArray& byteArray) {
    memoryBuffer = MemoryBuffer{byteArray};
}

void FileStorage::setMemoryBufferForAnimation(int index, const QByteArray& byteArray) {
    animationMemoryBuffer[index] = MemoryBuffer{byteArray};
}
