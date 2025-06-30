// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PIX_H
#define PIX_H

#include <string>
#include <vector>
#include <QByteArray>
#include <QImage>
#include <QPixmap>

using std::string;
using std::vector;

class Pix {
public:
    explicit Pix(const string&);
    static Pix fromBytes(const QByteArray&);
    static Pix fromImage(const QImage&);
    static Pix blank(int);
    void scale(int, int);
    void scaleToHeight(int);
    void scaleToWidth(int);
    QPixmap get();
    static vector<int> getScreenBounds();
    static int getImageWidth(int);
    QPixmap pix;
};

#endif  // PIX_H
