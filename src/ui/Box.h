// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef BOX_H
#define BOX_H

#include <QLayout>

class Box {
public:
    virtual QLayout * getView() = 0;
    virtual ~Box() {};
};

#endif  // BOX_H
