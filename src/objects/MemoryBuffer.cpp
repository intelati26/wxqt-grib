// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MemoryBuffer.h"

MemoryBuffer::MemoryBuffer()
    : qbyteArray{0, '0'}
{}

MemoryBuffer::MemoryBuffer(int capacity)
    : qbyteArray{capacity, '0'}
    , capacity{capacity}
{}

MemoryBuffer::MemoryBuffer(const QByteArray& qarray)
    : qbyteArray{qarray}
    , capacity{qbyteArray.size()}
{}

MemoryBuffer::MemoryBuffer(char * d, int capacity)
    : qbyteArray{d, capacity}
    , capacity{qbyteArray.size()}
{}

void MemoryBuffer::mark(int markAt) {
    markPosition = position;
    position = markAt;
}

// KEEP
// void MemoryBuffer::mark() {
//    markPosition = position;
// }

void MemoryBuffer::reset() {
    position = markPosition;
}

int64_t MemoryBuffer::getCapacity() const {
    return capacity;
}

int MemoryBuffer::getPosition() const {
    return position;
}

void MemoryBuffer::setPosition(int posn) {
    position = posn;
}

// KEEP
// bool MemoryBuffer::eof() const {
//    if (position < (getCapacity() - 1))
//        return false;
//    else
//        return true;
// }

float MemoryBuffer::getFloat() {
//    float f;
//    uchar b[] = {b3, b2, b1, b0};
//    memcpy(&f, &b, sizeof(f));
//    return f;
    union {
        float f;
        uchar b[4];
    } u;
    u.b[3] = qbyteArray.at(position);
    u.b[2] = qbyteArray.at(position + 1);
    u.b[1] = qbyteArray.at(position + 2);
    u.b[0] = qbyteArray.at(position + 3);
    position += 4;
    return u.f;
}

// KEEP
// float MemoryBuffer::getFloat(int index) const {
//    union {
//        float f;
//        uchar b[4];
//    } u;
//    u.b[3] = qbyteArray.at(index);
//    u.b[2] = qbyteArray.at(index + 1);
//    u.b[1] = qbyteArray.at(index + 2);
//    u.b[0] = qbyteArray.at(index + 3);
//    return u.f;
// }

// KEEP
// float MemoryBuffer::getFloatNative() {
// //    float f;
// //    uchar b[] = {b3, b2, b1, b0};
// //    memcpy(&f, &b, sizeof(f));
// //    return f;
//    union {
//        float f;
//        uchar b[4];
//    } u;
//    u.b[0] = qbyteArray.at(position);
//    u.b[1] = qbyteArray.at(position + 1);
//    u.b[2] = qbyteArray.at(position + 2);
//    u.b[3] = qbyteArray.at(position + 3);
//    position += 4;
//    return u.f;
// }

float MemoryBuffer::getFloatByIndex(int index) const {
    union {
        float f;
        uchar b[4];
    } u;
    u.b[0] = qbyteArray.at(index);
    u.b[1] = qbyteArray.at(index + 1);
    u.b[2] = qbyteArray.at(index + 2);
    u.b[3] = qbyteArray.at(index + 3);
    return u.f;
}

void MemoryBuffer::putFloat(double number) {
    auto newValue = static_cast<float>(number);
    memcpy(qbyteArray.data() + position, &newValue, sizeof(newValue));
    position += 4;
}

// KEEP
// void MemoryBuffer::putByte(unsigned char newValue) {
//    memcpy(qbyteArray.data() + position, &newValue, sizeof(newValue));
//    position += 1;
// }

void MemoryBuffer::put(unsigned char newValue) {
    memcpy(qbyteArray.data() + position, &newValue, sizeof(newValue));
    position += 1;
}

void MemoryBuffer::putByIndex(int index, unsigned char newValue) {
    memcpy(qbyteArray.data() + index, &newValue, sizeof(newValue));
}

// KEEP
// void MemoryBuffer::putInt(int newValue) {
//    memcpy(qbyteArray.data() + position, &newValue, sizeof(newValue));
//    position += 4;
// }

// void MemoryBuffer::putSignedShort(int16_t newValue) {
//    memcpy(qbyteArray.data() + position, &newValue, sizeof(newValue));
//    position += 2;
// }

int16_t MemoryBuffer::getShort() {
    union {
        int16_t f;
        uchar b[2];
    } u;
    u.b[1] = qbyteArray.at(position);
    u.b[0] = qbyteArray.at(position + 1);
    position += 2;
    return u.f;
}

// KEEP
// int16_t MemoryBuffer::getShortNative() {
//    union {
//        int16_t f;
//        uchar b[2];
//    } u;
//    u.b[0] = qbyteArray.at(position);
//    u.b[1] = qbyteArray.at(position + 1);
//    position += 2;
//    return u.f;
// }

uint16_t MemoryBuffer::getUnsignedShort() {
    union {
        uint16_t f;
        uchar b[2];
    } u;
    u.b[1] = qbyteArray.at(position);
    u.b[0] = qbyteArray.at(position + 1);
    position += 2;
    return u.f;
}

int MemoryBuffer::getInt() {
    union {
        int f;
        uchar b[4];
    } u;
    u.b[3] = qbyteArray.at(position);
    u.b[2] = qbyteArray.at(position + 1);
    u.b[1] = qbyteArray.at(position + 2);
    u.b[0] = qbyteArray.at(position + 3);
    position += 4;
    return u.f;
}

unsigned char MemoryBuffer::get() {
    position += 1;
    return qbyteArray.at(position - 1);
}

unsigned char MemoryBuffer::getByIndex(int index) const {
    return qbyteArray.at(index);
}

void MemoryBuffer::skipBytes(int count) {
    position += count;
}

char * MemoryBuffer::getBackingPointer() {
    return qbyteArray.data();
}

/*
 *Type	Typical Bit Width	Typical Range
char	1byte	-127 to 127 or 0 to 255
unsigned char	1byte	0 to 255
signed char	1byte	-127 to 127
int	4bytes	-2147483648 to 2147483647
unsigned int	4bytes	0 to 4294967295
signed int	4bytes	-2147483648 to 2147483647
short int	2bytes	-32768 to 32767
unsigned short int	2bytes	0 to 65,535
signed short int	2bytes	-32768 to 32767
long int	8bytes	-2,147,483,648 to 2,147,483,647
signed long int	8bytes	same as long int
unsigned long int	8bytes	0 to 18,446,744,073,709,551,615
long long int	8bytes	-(2^63) to (2^63)-1
unsigned long long int	8bytes	0 to 18,446,744,073,709,551,615
float	4bytes
double	8bytes
long double	12bytes
wchar_t	2 or 4 bytes	1 wide character
 * */
