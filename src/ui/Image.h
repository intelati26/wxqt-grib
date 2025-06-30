// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef IMAGE_H
#define IMAGE_H

#include <functional>
#include <QByteArray>
#include "ui/ClickableLabel.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;

class Image : public Widget2 {
public:
    explicit Image(Window *);
    void setNumberAcross(int, int);
    void setToWidth(const QByteArray&, int);
    void resize(float);
    void resizeToWidth(float);
    void connect(const std::function<void()>&);
    void setBytes(const QByteArray&);
    ClickableLabel * getView();
    int imageSize;
    QByteArray bytes;

private:
    Window * parent;
    ClickableLabel * image;
};

#endif  // IMAGE_H
