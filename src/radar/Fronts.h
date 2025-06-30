// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef FRONTS_H
#define FRONTS_H

#include <vector>
#include <QColor>
#include <QLineF>
#include <QVector>
#include "objects/LatLon.h"
#include "radar/FrontTypeEnum.h"
#include "radar/ProjectionNumbers.h"

using std::vector;

class Fronts {
public:
    explicit Fronts(FrontTypeEnum);
    void translate(int, const ProjectionNumbers&);
    QColor penColor;
    QVector<QLineF> coordinatesModified[4];
    FrontTypeEnum type;
    vector<LatLon> coordinates;
};

#endif  // FRONTS_H
