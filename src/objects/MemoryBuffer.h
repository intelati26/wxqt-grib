// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef MEMORYBUFFER_H
#define MEMORYBUFFER_H

#include <cstdint>
#include <QByteArray>

class MemoryBuffer {
public:
    MemoryBuffer();
    explicit MemoryBuffer(int);
    explicit MemoryBuffer(const QByteArray &);
    MemoryBuffer(char *, int);
    void mark(int);
    void reset();
    int64_t getCapacity() const;
    int getPosition() const;
    void setPosition(int);
    float getFloat();
    float getFloatByIndex(int) const;
    void putFloat(double);
    void put(unsigned char);
    void putByIndex(int, unsigned char);
    int16_t getShort();
    uint16_t getUnsignedShort();
    int getInt();
    unsigned char get();
    unsigned char getByIndex(int) const;
    void skipBytes(int);
    char * getBackingPointer();

private:
    QByteArray qbyteArray;
    int position{};
    int markPosition{};
    int64_t capacity{};
};

#endif  // MEMORYBUFFER_H
