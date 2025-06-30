// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADCOLORLEGEND_H
#define NEXRADCOLORLEGEND_H

#include <string>
#include <QFont>
#include <QPaintEvent>
#include <QPainter>

using std::string;

class NexradColorLegend {
public:
    explicit NexradColorLegend(const string&);
    void paintEvent(QPainter &, double, double, double);
    void update(const string&);

private:
    void drawRect(QPainter *, int) const;
    void drawText(QPainter *, const string&, double);
    static int setColor(int, int, int);
    void setColorWithBuffers(int, int);
    string product;
    int color{};
    QFont qfont;
    double width1{};
    double widthStarting{};
    double textFromLegend{};
    double scaledHeight{};
    double startHeight{};
};

#endif  // NEXRADCOLORLEGEND_H
