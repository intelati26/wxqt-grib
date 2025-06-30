// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "To.h"
#include <iostream>
#include <sstream>

int To::Int(const std::string& s) {
    std::istringstream iss(s);
    int number;
    iss >> number;
    if (iss.fail()) {
        std::cerr << "error parsing: " << s << "\n";
        return 0;
    } else {
        return number;
    }
}

int64_t To::int64(const std::string& s) {
    std::istringstream iss(s);
    int64_t number;
    iss >> number;
    if (iss.fail()) {
        std::cerr << "error parsing: " << s << "\n";
        return 0;
    } else {
        return number;
    }
}

double To::Double(const std::string& s) {
    std::istringstream iss(s);
    double number;
    iss >> number;
    if (iss.fail()) {
        std::cerr << "error parsing: " << s << "\n";
        return 0.0;
    } else {
        return number;
    }
}

std::string To::string(size_t i) {
    return std::to_string(i);
}

std::string To::string(int i) {
    return std::to_string(i);
}

std::string To::string(float f) {
    return std::to_string(f);
}

std::string To::string(double d) {
    return std::to_string(d);
}

std::string To::string(bool b) {
    std::ostringstream ss;
    ss << std::boolalpha << b;
    return ss.str();
}

std::string To::stringPadLeft(const std::string& s, int i) {
    if (static_cast<int>(s.size()) > i) {
        return s;
    }
    return s + std::string(i - s.size(), ' ');
}

std::string To::stringPadLeftZeros(const std::string& s, int padAmount) {
    if (static_cast<int>(s.size()) > padAmount) {
        return s;
    }
    return std::string(padAmount - s.size(), '0') + s;
}

std::string To::stringPadLeftZeros(int s, int padAmount) {
    if (static_cast<int>(To::string(s).size()) > padAmount) {
        return To::string(s);
    }
    return std::string(padAmount - To::string(s).size(), '0') + To::string(s);
}

std::string To::stringPadRight(const std::string& s, int padAmount) {
    if (static_cast<int>(s.size()) > padAmount) {
        return s;
    }
    return s + std::string(padAmount - s.size(), '0');
}
