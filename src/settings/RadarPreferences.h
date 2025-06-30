// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef RADARPREFERENCES_H
#define RADARPREFERENCES_H

#include <string>
#include <QColor>

using std::string;

class RadarPreferences {
public:
    static void initialize();
    static void initializeColors();
    static QColor getInitialPreference(const string&, int);
    static bool locdotFollowsGps;
    static bool dualpaneshareposn;
    static bool obs;
    static bool obsWindbarbs;
    static bool swo;
    static bool fire;
    static bool cities;
    static bool locationDot;
    static bool countyLabels;
    static bool sti;
    static bool hailIndex;
    static bool tvs;
    static bool colorLegend;
    static int textSize;
    static bool wpcFronts;
    static bool showControls;
    static bool rememberLocation;
    static double warnLinesize;
    static double watmcdLinesize;
//    static double gpsCircleLinesize;
    static double stiLinesize;
    static double swoLinesize;
    static double wbLinesize;
    static double lineFactor;
    static double locdotSize;
    static double aviationSize;
    static QColor colorLocdot;
    static QColor colorCity;
    static QColor colorSti;
    static QColor colorHi;
    static QColor colorTvs;
    static QColor colorObs;
    static QColor colorObsWindbarbs;
    static QColor colorCountyLabels;
    static QColor nexradRadarBackgroundColor;
};

#endif  // RADARPREFERENCES_H
