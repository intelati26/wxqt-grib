// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDSEVENDAY_H
#define CARDSEVENDAY_H

#include <string>
#include "ui/HBox.h"
#include "ui/Text.h"
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class CardSevenDay : public HBox {
public:
    CardSevenDay(Window *, const string&, const string&, const string&);
    void update(const string&, const string&, const string&);

private:
    static string tempAndWind(const string&);
    VBox boxImage;
    VBox boxText;
    Text text1;
    Text text2;
    Photo photo;
};

#endif  // CARDSEVENDAY_H
