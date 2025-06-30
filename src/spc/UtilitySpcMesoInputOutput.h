// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef UTILITYSPCMESOINPUTOUTPUT_H
#define UTILITYSPCMESOINPUTOUTPUT_H

#include <string>
#include <vector>

using std::string;
using std::vector;

class UtilitySpcMesoInputOutput {
public:
    static vector<string> getAnimation(const string&, const string&, size_t);
    static string getImageUrl(const string&, const string&);
};

#endif  // UTILITYSPCMESOINPUTOUTPUT_H
