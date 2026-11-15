// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

// The color scales of the charts (Ramp), one for each kind of field: the stops are in the units the chart's fill is in.
#include "gfs/GfsChartInternal.h"

namespace GfsChart {
namespace detail {

Ramp temperature() {
    return {{{-45, QColor{"#4b2d7f"}}, {-30, QColor{"#5b55a8"}}, {-20, QColor{"#3f74b8"}}, {-10, QColor{"#4aa0d4"}}, {0, QColor{"#9ad6e6"}}, {5, QColor{"#bfe4cf"}},
             {10, QColor{"#dcefb4"}}, {15, QColor{"#f3f0a4"}}, {20, QColor{"#f8d97e"}}, {25, QColor{"#f5ac5c"}}, {30, QColor{"#e57741"}}, {35, QColor{"#c5432e"}}, {40, QColor{"#8e1f2c"}}, {47, QColor{"#5c1024"}}}};
}
// flat bands of 4 degrees F (the model guidance site's RTMA and URMA charts are drawn so), in the colors `base` has at the middle of each: lowF to highF, and the first and last bands go on past them
Ramp bands4F(const Ramp& base, double lowF, double highF) {
    const auto celsius = [] (double f) { return (f - 32.0) / 1.8; };
    Ramp r;
    r.banded = true;
    r.stops.push_back({-100.0, QColor::fromRgba(base.at(celsius(lowF)))});   // below the first band: the same color (its number is not on the legend)
    for (double f = lowF; f < highF - 1e-9; f += 4.0) {
        r.stops.push_back({celsius(f), QColor::fromRgba(base.at(celsius(f + 2.0)))});
    }
    return r;
}
Ramp windSpeed() {
    return {{{0, QColor{"#f7fbff"}}, {15, QColor{"#e0ecf6"}}, {30, QColor{"#c4dcee"}}, {45, QColor{"#98c5e0"}}, {60, QColor{"#6aa9d2"}}, {80, QColor{"#4687bf"}},
             {100, QColor{"#7a72b8"}}, {120, QColor{"#b36aa6"}}, {140, QColor{"#d6618a"}}, {170, QColor{"#e8665d"}}, {200, QColor{"#f29a4d"}}}};
}
Ramp vorticityRamp() {   // units of 1e-5 per second; the cyclonic side only
    return {{{0, QColor{255, 255, 255, 0}}, {6, QColor{"#fff7d1"}}, {10, QColor{"#fde8a0"}}, {16, QColor{"#fcc967"}}, {24, QColor{"#f59d47"}}, {34, QColor{"#df6a34"}}, {48, QColor{"#a8402a"}}, {70, QColor{"#6b1f2a"}}}};
}

Ramp precipitation() {   // millimeters
    return {{{0.0, QColor{217, 239, 208, 0}}, {0.25, QColor{"#d9efd0"}}, {1.0, QColor{"#b6e3b0"}}, {2.5, QColor{"#86d08f"}}, {6.0, QColor{"#4fb98a"}}, {12.0, QColor{"#2fa3a6"}},
             {25.0, QColor{"#2b7fc0"}}, {38.0, QColor{"#3a5bb8"}}, {50.0, QColor{"#5b43a8"}}, {75.0, QColor{"#8a3aa6"}}, {100.0, QColor{"#b5368f"}}, {150.0, QColor{"#d9435f"}},
             {200.0, QColor{"#ee7a47"}}, {300.0, QColor{"#f6b24e"}}}};
}
Ramp reflectivity() {   // dBZ
    return {{{0, QColor{207, 232, 243, 0}}, {5, QColor{"#cfe8f3"}}, {15, QColor{"#8ccbe0"}}, {20, QColor{"#5bb0c8"}}, {25, QColor{"#55a868"}}, {30, QColor{"#86c04a"}},
             {35, QColor{"#e3d44a"}}, {40, QColor{"#f0a63a"}}, {45, QColor{"#e0702e"}}, {50, QColor{"#cc3d2c"}}, {55, QColor{"#a8206b"}}, {60, QColor{"#7a2a9a"}}, {70, QColor{"#f2e6f7"}}}};
}
Ramp humidity() {   // percent
    return {{{0, QColor{"#7a4a24"}}, {20, QColor{"#b58a52"}}, {40, QColor{"#e3cfa0"}}, {55, QColor{"#f4f0df"}}, {70, QColor{"#c3e3b4"}}, {85, QColor{"#6cc496"}}, {95, QColor{"#2d9aa6"}}, {100, QColor{"#1e5fa8"}}}};
}
Ramp precipitableWater() {   // millimeters
    return {{{0, QColor{"#f7f4e8"}}, {13, QColor{"#e3ecc0"}}, {25, QColor{"#b5dd9a"}}, {38, QColor{"#7cc9a0"}}, {50, QColor{"#46aeb8"}}, {63, QColor{"#3a86c4"}}, {75, QColor{"#5a5fb8"}}, {90, QColor{"#8b4aa6"}}}};
}
Ramp snowChange() {   // centimeters; transparent where nothing changes
    return {{{-30, QColor{"#8c5a2b"}}, {-10, QColor{"#c99a62"}}, {-1, QColor{"#ecd9b8"}}, {-0.3, QColor{236, 217, 184, 0}}, {0.3, QColor{200, 225, 245, 0}}, {1, QColor{"#c8e1f5"}},
             {10, QColor{"#6aa9d2"}}, {30, QColor{"#5a5fb8"}}, {60, QColor{"#8b4aa6"}}}};
}

Ramp shearRamp() {   // knots: calm to violent, the low shear tropical cyclones like in the pale greens
    return {{{0, QColor{"#f4f9f1"}}, {10, QColor{"#cfe6c8"}}, {20, QColor{"#f0e8a0"}}, {30, QColor{"#f3b86a"}}, {40, QColor{"#e0703e"}}, {60, QColor{"#a22b3a"}}, {80, QColor{"#5c1024"}}}};
}
Ramp divergenceRamp() {   // 1e-5 per second: convergence in blues, divergence in warm colors, little of either left clear
    return {{{-30, QColor{"#2b4a9a"}}, {-15, QColor{"#4f86c6"}}, {-6, QColor{"#a9cde8"}}, {-2, QColor{169, 205, 232, 0}}, {2, QColor{246, 216, 154, 0}}, {6, QColor{"#f6d89a"}}, {15, QColor{"#ee9a4a"}}, {30, QColor{"#c0392b"}}}};
}
Ramp thetaERamp() {   // kelvin
    return {{{260, QColor{"#4b2d7f"}}, {280, QColor{"#3f74b8"}}, {295, QColor{"#9ad6e6"}}, {305, QColor{"#dcefb4"}}, {315, QColor{"#f3f0a4"}}, {325, QColor{"#f8d97e"}}, {335, QColor{"#f5ac5c"}}, {345, QColor{"#e57741"}}, {355, QColor{"#c5432e"}}, {370, QColor{"#5c1024"}}}};
}

Ramp heightAnomaly() {   // decameters: below normal in blues, above in warm colors, within 1.5 clear
    return {{{-30, QColor{"#2b4a9a"}}, {-20, QColor{"#3f74b8"}}, {-10, QColor{"#86b4dc"}}, {-4, QColor{"#d3e5f2"}}, {-1.5, QColor{211, 229, 242, 0}}, {1.5, QColor{246, 224, 184, 0}},
             {4, QColor{"#f6e0b8"}}, {10, QColor{"#f0b070"}}, {20, QColor{"#d9633a"}}, {30, QColor{"#a02a2a"}}}};
}
Ramp standardAnomaly() {   // standard deviations: within a quarter of one clear
    return {{{-3, QColor{"#2b4a9a"}}, {-2, QColor{"#3f74b8"}}, {-1, QColor{"#86b4dc"}}, {-0.5, QColor{"#d3e5f2"}}, {-0.25, QColor{211, 229, 242, 0}}, {0.25, QColor{246, 224, 184, 0}},
             {0.5, QColor{"#f6e0b8"}}, {1, QColor{"#f0b070"}}, {2, QColor{"#d9633a"}}, {3, QColor{"#a02a2a"}}}};
}
Ramp temperatureDeparture() {   // degrees C: below the climate in blues, above in warm colors, within half a degree clear
    return {{{-10, QColor{"#2b4a9a"}}, {-6, QColor{"#3f74b8"}}, {-3, QColor{"#86b4dc"}}, {-1, QColor{"#d3e5f2"}}, {-0.5, QColor{211, 229, 242, 0}}, {0.5, QColor{246, 224, 184, 0}},
             {1, QColor{"#f6e0b8"}}, {3, QColor{"#f0b070"}}, {6, QColor{"#d9633a"}}, {10, QColor{"#a02a2a"}}}};
}
Ramp seaLevelPressure() {   // millibars: lows in purples and blues, highs in yellows to reds
    return {{{980, QColor{"#6e3aa0"}}, {995, QColor{"#3a5ac8"}}, {1005, QColor{"#78bee6"}}, {1013, QColor{"#ebebe1"}}, {1020, QColor{"#fad25a"}}, {1030, QColor{"#f08232"}}, {1040, QColor{"#c82828"}}}};
}
Ramp precipitationDeparture() {   // millimeters in the period: drier than the climate in browns, wetter in greens, within a few clear
    return {{{-60, QColor{"#7a4a1c"}}, {-30, QColor{"#b07a3a"}}, {-12, QColor{"#dcc08a"}}, {-4, QColor{"#efe3c4"}}, {-2, QColor{239, 227, 196, 0}}, {2, QColor{200, 230, 200, 0}},
             {4, QColor{"#c8e6c8"}}, {12, QColor{"#8cc99a"}}, {30, QColor{"#3fa06a"}}, {60, QColor{"#1b6e48"}}}};
}
Ramp pressureAnomaly() {   // millibars
    return {{{-30, QColor{"#2b4a9a"}}, {-16, QColor{"#3f74b8"}}, {-8, QColor{"#86b4dc"}}, {-3, QColor{"#d3e5f2"}}, {-1, QColor{211, 229, 242, 0}}, {1, QColor{246, 224, 184, 0}},
             {3, QColor{"#f6e0b8"}}, {8, QColor{"#f0b070"}}, {16, QColor{"#d9633a"}}, {30, QColor{"#a02a2a"}}}};
}

Ramp dewpoint() {   // degrees C: dry in the browns, humid in greens and blues
    return {{{-30, QColor{"#8c5a2b"}}, {-15, QColor{"#c99a62"}}, {-5, QColor{"#e8d9b0"}}, {0, QColor{"#f1ecd2"}}, {8, QColor{"#d4ead0"}}, {14, QColor{"#a6dbb0"}}, {18, QColor{"#6cc496"}},
             {22, QColor{"#2fa3a6"}}, {25, QColor{"#2b7fc0"}}, {28, QColor{"#5b43a8"}}, {31, QColor{"#8a3aa6"}}}};
}
Ramp lowWind() {   // knots, for the near-surface and 850 mb winds
    return {{{0, QColor{"#f7fbff"}}, {10, QColor{"#e0ecf6"}}, {20, QColor{"#c4dcee"}}, {30, QColor{"#98c5e0"}}, {40, QColor{"#6aa9d2"}}, {50, QColor{"#4687bf"}}, {65, QColor{"#7a72b8"}}, {80, QColor{"#d6618a"}}}};
}
Ramp capeRamp() {   // J/kg
    return {{{0, QColor{227, 240, 198, 0}}, {100, QColor{"#e3f0c6"}}, {500, QColor{"#b8dc8a"}}, {1000, QColor{"#f1e27a"}}, {1500, QColor{"#f4bf5a"}}, {2000, QColor{"#ee9144"}}, {3000, QColor{"#dc5a3c"}},
             {4000, QColor{"#b8303c"}}, {5000, QColor{"#7a2a8a"}}}};
}
Ramp snowPrecipitation() {   // millimeters of water
    return {{{0.0, QColor{214, 230, 247, 0}}, {0.25, QColor{"#d6e6f7"}}, {1.0, QColor{"#a9c9ec"}}, {2.5, QColor{"#7ba7e0"}}, {6.0, QColor{"#4f7fd0"}}, {12.0, QColor{"#3a55b8"}}, {25.0, QColor{"#3a2f9a"}}, {50.0, QColor{"#5b2a8a"}}}};
}
Ramp mixedPrecipitation() {   // freezing rain and ice pellets
    return {{{0.0, QColor{243, 208, 230, 0}}, {0.25, QColor{"#f3d0e6"}}, {1.0, QColor{"#ea9fcb"}}, {2.5, QColor{"#d96aa8"}}, {6.0, QColor{"#c03f88"}}, {12.0, QColor{"#8f2a73"}}, {25.0, QColor{"#5c1a5a"}}}};
}

Ramp cloudCover() {   // percent: clear sky pale, overcast a dark grey-blue
    return {{{0, QColor{"#eef6fb"}}, {20, QColor{"#cfe3f1"}}, {40, QColor{"#a9c3d8"}}, {60, QColor{"#8da1b4"}}, {80, QColor{"#6e7d8c"}}, {100, QColor{"#4c5560"}}}};
}
Ramp spreadRamp(double top) {   // the spread of an ensemble, from none (white) up through yellow and orange to a deep purple at `top`
    return {{{0.0, {255, 255, 255}}, {top * 0.1, {255, 247, 188}}, {top * 0.3, {254, 196, 79}}, {top * 0.5, {244, 109, 67}}, {top * 0.75, {200, 30, 80}}, {top, {90, 20, 120}}}};
}
Ramp tropicalWind() {   // knots: the Saffir-Simpson steps, tropical depression and storm in the cool colors, hurricanes warm to purple
    return {{{0, QColor{235, 245, 255}}, {20, QColor{150, 200, 240}}, {34, QColor{0, 210, 160}}, {50, QColor{255, 230, 0}}, {64, QColor{255, 170, 0}}, {83, QColor{255, 100, 0}}, {96, QColor{230, 30, 30}},
             {113, QColor{180, 0, 90}}, {137, QColor{130, 0, 170}}, {160, QColor{255, 255, 255}}}};
}
Ramp satelliteIr() {   // brightness temperature in kelvin: warm surfaces dark, cool cloud light grey, then the cold tops in color to the coldest in white
    return {{{183, QColor{255, 255, 255}}, {193, QColor{60, 0, 90}}, {203, QColor{200, 0, 200}}, {213, QColor{255, 0, 0}}, {223, QColor{255, 150, 0}}, {233, QColor{255, 255, 0}}, {243, QColor{0, 230, 230}},
             {253, QColor{175, 175, 175}}, {273, QColor{95, 95, 95}}, {303, QColor{0, 0, 0}}}};
}
Ramp waterVapor() {   // brightness temperature in kelvin: the moist, cold air in white through blue, dry air warm in orange to black
    return {{{185, QColor{255, 255, 255}}, {200, QColor{120, 255, 200}}, {210, QColor{0, 200, 255}}, {220, QColor{30, 100, 255}}, {230, QColor{30, 30, 120}}, {240, QColor{110, 110, 110}},
             {250, QColor{190, 170, 120}}, {260, QColor{255, 200, 60}}, {270, QColor{255, 120, 0}}, {285, QColor{0, 0, 0}}}};
}
Ramp seaSurfaceTemperature() {   // degrees C: fine steps through the range that matters to a storm (26 C is where the warm water starts)
    return {{{10, QColor{40, 60, 160}}, {20, QColor{30, 150, 220}}, {24, QColor{0, 200, 200}}, {26, QColor{0, 200, 120}}, {28, QColor{250, 230, 0}}, {29.5, QColor{255, 140, 0}}, {31, QColor{220, 30, 30}},
             {33, QColor{130, 0, 60}}}};
}
Ramp airQuality(const std::vector<double>& steps) {   // the six steps of the US air quality index (good, moderate, unhealthy for sensitive groups, unhealthy, very unhealthy, hazardous): the first step's value is where "good" ends
    static const char * const colors[] = {"#00e400", "#ffff00", "#ff7e00", "#ff0000", "#8f3f97", "#7e0023"};   // 6 colors, 5 steps between them
    Ramp r;
    r.banded = true;
    r.stops.push_back({0, QColor{colors[0]}});
    for (size_t i = 0; i < steps.size() && i < 5; i++) {
        r.stops.push_back({steps[i], QColor{colors[i + 1]}});
    }
    r.stops.push_back({steps.back() * 2, QColor{colors[5]}});
    return r;
}
Ramp uvIndex() {   // the WHO steps: low (green), moderate (yellow), high (orange), very high (red), extreme (violet)
    return {{{0, QColor{"#299501"}}, {2, QColor{"#299501"}}, {3, QColor{"#f7e400"}}, {5, QColor{"#f7e400"}}, {6, QColor{"#f85900"}}, {7, QColor{"#f85900"}}, {8, QColor{"#d8001d"}}, {10, QColor{"#d8001d"}},
             {11, QColor{"#6b49c8"}}, {13, QColor{"#6b49c8"}}}};
}
Ramp currentSpeed() {   // meters per second: slack water pale, the Gulf Stream and the Agulhas (1.5 and more) in strong colors
    return {{{0, QColor{235, 245, 255}}, {0.1, QColor{190, 225, 245}}, {0.25, QColor{120, 190, 235}}, {0.5, QColor{0, 170, 220}}, {0.75, QColor{0, 200, 120}}, {1.0, QColor{250, 230, 0}}, {1.5, QColor{255, 140, 0}},
             {2.0, QColor{220, 30, 30}}, {3.0, QColor{130, 0, 60}}}};
}
Ramp seaLevelHeight() {   // meters above the mean: below in blues, above in warm colors
    return {{{-1.0, QColor{30, 60, 160}}, {-0.5, QColor{70, 140, 220}}, {-0.2, QColor{170, 210, 240}}, {-0.05, QColor{245, 245, 245}}, {0.05, QColor{245, 245, 245}}, {0.2, QColor{250, 215, 160}},
             {0.5, QColor{240, 140, 60}}, {1.0, QColor{190, 40, 30}}}};
}
Ramp waveHeight() {   // meters
    return {{{0, QColor{235, 245, 255}}, {1, QColor{150, 200, 240}}, {2, QColor{0, 170, 220}}, {3, QColor{0, 200, 120}}, {4, QColor{250, 230, 0}}, {6, QColor{255, 140, 0}}, {8, QColor{220, 30, 30}},
             {10, QColor{180, 0, 90}}, {12, QColor{130, 0, 170}}, {15, QColor{255, 255, 255}}}};
}
Ramp helicityRamp() {   // m2/s2: storm-relative helicity, the 150 and 300 steps are where rotation starts to matter
    return {{{0, QColor{240, 244, 250, 0}}, {50, QColor{"#d6e6f2"}}, {100, QColor{"#9fd0e0"}}, {150, QColor{"#7bc47f"}}, {250, QColor{"#f3e04a"}}, {300, QColor{"#f0a63a"}}, {400, QColor{"#e0602e"}},
             {500, QColor{"#c4262c"}}, {700, QColor{"#a8206b"}}, {1000, QColor{"#6a2a9a"}}}};
}
Ramp updraftHelicity() {   // m2/s2: rotating updrafts, from 25 (weak) to 200 and more (a strong supercell)
    return {{{0, QColor{240, 244, 250, 0}}, {10, QColor{240, 244, 250, 0}}, {25, QColor{"#c4e3a4"}}, {50, QColor{"#7bc47f"}}, {75, QColor{"#f3e04a"}}, {100, QColor{"#f0a63a"}}, {150, QColor{"#e0602e"}},
             {200, QColor{"#c4262c"}}, {300, QColor{"#a8206b"}}, {500, QColor{"#6a2a9a"}}}};
}
Ramp lowLevelVorticity() {   // 1e-3 per second: the strongest vertical vorticity of the last hour in the lowest 1-2 km; the weak values clear, a mesocyclone from about 10, a tornadic circulation from 20
    return {{{0, QColor{240, 244, 250, 0}}, {3, QColor{240, 244, 250, 0}}, {5, QColor{"#c4e3a4"}}, {8, QColor{"#7bc47f"}}, {10, QColor{"#f3e04a"}}, {15, QColor{"#f0a63a"}}, {20, QColor{"#e0702e"}},
             {30, QColor{"#c4262c"}}, {40, QColor{"#a8206b"}}, {60, QColor{"#6a2a9a"}}}};
}
Ramp echoTopRamp() {   // thousands of feet
    return {{{0, QColor{240, 244, 250, 0}}, {5, QColor{240, 244, 250, 0}}, {10, QColor{"#cfe8f3"}}, {20, QColor{"#8ccbe0"}}, {30, QColor{"#55a868"}}, {35, QColor{"#e3d44a"}}, {40, QColor{"#f0a63a"}},
             {45, QColor{"#e0702e"}}, {50, QColor{"#cc3d2c"}}, {55, QColor{"#a8206b"}}, {60, QColor{"#7a2a9a"}}}};
}
Ramp ceilingRamp() {   // feet: the flight category steps (500 LIFR, 1000 IFR, 3000 MVFR) in the strong colors
    return {{{0, QColor{"#b5368f"}}, {500, QColor{"#d9435f"}}, {1000, QColor{"#ee7a47"}}, {2000, QColor{"#f6b24e"}}, {3000, QColor{"#e3d44a"}}, {5000, QColor{"#a9d98a"}}, {10000, QColor{"#cfe8f3"}}, {20000, QColor{"#f4f8fb"}}}};
}
Ramp visibilityRamp() {   // statute miles: 1 and 3 are the IFR and MVFR steps
    return {{{0, QColor{"#b5368f"}}, {0.5, QColor{"#d9435f"}}, {1, QColor{"#ee7a47"}}, {3, QColor{"#f6b24e"}}, {5, QColor{"#e3d44a"}}, {7, QColor{"#a9d98a"}}, {10, QColor{"#f4f8fb"}}}};
}
Ramp lightningRamp() {   // flashes: any lightning is yellow, a lot is purple
    return {{{0, QColor{240, 244, 250, 0}}, {0.1, QColor{255, 240, 120, 150}}, {1, QColor{"#ffd34a"}}, {3, QColor{"#f09a2e"}}, {6, QColor{"#e0502e"}}, {12, QColor{"#b02060"}}, {25, QColor{"#6a2a9a"}}}};
}
Ramp wavePeriod() {   // seconds: short choppy seas pale, long swell in deep colors
    return {{{0, QColor{240, 244, 250}}, {4, QColor{"#cfe8f3"}}, {6, QColor{"#8ccbe0"}}, {8, QColor{"#55b6a8"}}, {10, QColor{"#7bc47f"}}, {12, QColor{"#e3d44a"}}, {14, QColor{"#f0a63a"}}, {16, QColor{"#e0602e"}},
             {18, QColor{"#c4262c"}}, {22, QColor{"#6a2a9a"}}}};
}
Ramp probability() {   // percent: nothing under 5, then pale green to deep magenta
    return {{{0, QColor{229, 242, 217, 0}}, {5, QColor{229, 242, 217, 0}}, {10, QColor{"#e5f2d9"}}, {20, QColor{"#c4e3a4"}}, {30, QColor{"#93d17f"}}, {40, QColor{"#5cbf8a"}}, {50, QColor{"#31a8a8"}},
             {60, QColor{"#2f86c4"}}, {70, QColor{"#3a5fb8"}}, {80, QColor{"#5b43a8"}}, {90, QColor{"#8a3aa6"}}, {100, QColor{"#b5368f"}}}};
}
Ramp snowfall() {   // centimeters of new snow, the steps at whole and simple inches (0.1, 1, 2, 4, 8, 12, 20, 30)
    return {{{0.0, QColor{214, 230, 247, 0}}, {0.254, QColor{"#d6e6f7"}}, {2.54, QColor{"#a9c9ec"}}, {5.08, QColor{"#7ba7e0"}}, {10.16, QColor{"#4f7fd0"}}, {20.32, QColor{"#3a55b8"}}, {30.48, QColor{"#3a2f9a"}},
             {50.8, QColor{"#5b2a8a"}}, {76.2, QColor{"#8b4aa6"}}}};
}

Ramp stratosphereTemperature() {   // degrees C at 100 to 10 mb: the polar winter's cold (-80 and below) in purples and blues, the warm side of a sudden warming in oranges
    return {{{-100, QColor{"#4b2d7f"}}, {-90, QColor{"#5b55a8"}}, {-80, QColor{"#3f74b8"}}, {-70, QColor{"#4aa0d4"}}, {-60, QColor{"#9ad6e6"}}, {-50, QColor{"#dcefb4"}},
             {-40, QColor{"#f8d97e"}}, {-30, QColor{"#e57741"}}, {-20, QColor{"#c54331"}}}};
}

Ramp upperStratosphereTemperature() {   // degrees C at 5 to 1 mb: the same colors over a warmer span (the stratopause is near 0 C in the winter polar night's rim), -80 to +10
    return {{{-80, QColor{"#4b2d7f"}}, {-70, QColor{"#5b55a8"}}, {-60, QColor{"#3f74b8"}}, {-50, QColor{"#4aa0d4"}}, {-40, QColor{"#9ad6e6"}}, {-30, QColor{"#dcefb4"}},
             {-20, QColor{"#f8d97e"}}, {-10, QColor{"#e57741"}}, {0, QColor{"#c54331"}}, {10, QColor{"#8e1f2c"}}}};
}

Ramp potentialVorticityRamp(double top) {   // PVU, 0 to `top`: white through lilac and purple to deep red, the vortex's edge and core (the sign of the southern hemisphere is turned over, so one scale does for both)
    const std::pair<double, const char *> stops[] = {{0.0, "#f4f1fa"}, {0.08, "#d9d2ee"}, {0.18, "#a79bd6"}, {0.32, "#6f6cc0"}, {0.46, "#4a97c8"}, {0.6, "#6cc3a6"}, {0.74, "#e7de7a"}, {0.87, "#ee9a48"}, {1.0, "#b8252b"}};
    Ramp r;
    for (const auto& [fraction, color] : stops) {
        r.stops.push_back({fraction * top, QColor{color}});
    }
    return r;
}

}  // namespace detail
}  // namespace GfsChart
