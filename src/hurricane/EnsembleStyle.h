// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef ENSEMBLESTYLE_H
#define ENSEMBLESTYLE_H

#include <string>
#include <QColor>

// How each ensemble is told apart on the map and in the charts: its tree entries, its colors and what is credited. The open-data ECMWF ensembles and the DeepMind Weather Lab ones
// (FNV3 and the experimental WeatherNext 3) have members and a mean of their own; GEFS comes from NHC's guidance and has only its statistics.
namespace EnsembleStyle {
    struct Style {
        const char * label;       // the set's label: "DeepMind FNV3"
        const char * membersId;   // the guidance tree's entry for its members
        const char * meansId;     // and for its mean
        QColor member;            // a member on the map (translucent)
        QColor mean;              // the mean on the map
        QColor chart;             // in the statistics and intensity charts
    };

    inline const Style * of(const std::string& label) {
        static const Style styles[] = {
            {"AIFS ENS", "members/aifs", "means/aifs", QColor{60, 220, 170, 85}, QColor{40, 255, 170}, QColor{0, 150, 100}},
            {"IFS ENS", "members/ifs", "means/ifs", QColor{255, 150, 60, 85}, QColor{255, 140, 30}, QColor{230, 110, 20}},
            {"DeepMind FNV3", "members/fnv3", "means/fnv3", QColor{214, 110, 255, 85}, QColor{228, 130, 255}, QColor{150, 40, 190}},
            {"DeepMind WNV3", "members/wnv3", "means/wnv3", QColor{255, 100, 170, 85}, QColor{255, 130, 190}, QColor{200, 30, 110}},
        };
        for (const auto& style : styles) {
            if (label == style.label) {
                return &style;
            }
        }
        return nullptr;
    }
    inline bool deepMind(const std::string& label) { return label.rfind("DeepMind", 0) == 0; }
    // the sets whose members are an ensemble to take statistics of (GEFS included)
    inline bool probabilistic(const std::string& label) { return of(label) != nullptr || label == "GEFS"; }
    inline QColor chartColor(const std::string& label) {
        const auto * style = of(label);
        return style ? style->chart : QColor{120, 70, 200};
    }
    // what the terms of use ask to be shown with anything made from the data (the text of Google's own notice)
    inline std::string deepMindCredit() {
        return "(c) 2024-6 Google LLC, whose machine learning models were used to create the experimental data made available under the following licence terms "
               "https://storage.googleapis.com/weathernext-public/terms-of-use.pdf. This data is intended for experimental modelling only and is not intended, validated, or approved for real world use.";
    }
}

#endif  // ENSEMBLESTYLE_H
