// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SEVEREHAZARDMATRIXDATA_H
#define SEVEREHAZARDMATRIXDATA_H

#include <vector>
#include <QColor>
#include <QString>

// What the Day 1-8 hazard matrix shows for the country: the highest level of each product anywhere: the SPC convective outlooks (tornado, wind, hail and any severe: days 1-3 categories and probabilities, days 4-8 the 15% and 30% areas) and the
// WPC's excessive rainfall (days 1-5) and probabilistic snow and ice (days 1-3), read from the National Weather Service's map services.
namespace SevereHazardMatrixData {
    struct Cell {
        int row;                // SevereHazardMatrixWidget::RowIndex
        int day;                // 0 for day 1
        QString label;          // "ENH", "10%", "SLGT"; empty when the place is in no outlook area of that product
        QColor background;
        QColor text;
        QString tooltip;
    };
    // blocking (two dozen requests, made in parallel): off the UI thread. `problem` is set when nothing could be read.
    std::vector<Cell> fetch(QString& problem);
}

#endif  // SEVEREHAZARDMATRIXDATA_H
