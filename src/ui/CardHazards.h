// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef CARDHAZARDS_H
#define CARDHAZARDS_H

#include <vector>
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "util/Hazards.h"

using std::vector;

class CardHazards : public HBox {
public:
    CardHazards(Window *, const Hazards&);
    void removeLabels();

private:
    VBox box;
    vector<Button> labels;
};

#endif  // CARDHAZARDS_H
