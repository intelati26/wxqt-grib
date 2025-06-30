// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TO_H
#define TO_H

#include <cstdint>
#include <string>

class To {
public:
    static int Int(const std::string&);
    static int64_t int64(const std::string& s);
    static double Double(const std::string&);
    static std::string string(size_t);
    static std::string string(int);
    static std::string string(float);
    static std::string string(double);
    static std::string string(bool);
    static std::string stringPadLeft(const std::string&, int);
    static std::string stringPadLeftZeros(const std::string&, int);
    static std::string stringPadLeftZeros(int, int);
    static std::string stringPadRight(const std::string&, int);
};

#endif  // TO_H
