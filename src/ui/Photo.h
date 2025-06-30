// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef PHOTO_H
#define PHOTO_H

#include <functional>
#include <string>
#include <QByteArray>
#include <QLabel>
#include "ui/ClickableLabel.h"
#include "ui/PhotoSizeEnum.h"
#include "ui/Pix.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::function;
using std::string;

class Photo : public Widget2 {
public:
    explicit Photo(Window *);
    Photo(Window *, const QPixmap &);
    Photo(Window *, PhotoSizeEnum);
    Photo(Window *, PhotoSizeEnum, std::function<int()>);
    void setPix(const Pix&);
    void setFullScreenWithHeight();
    void resize(float);
    void setNoScale();
    void setToWidth(int);
    void setToHeight(float);
    void adjustSize();
    void setBytes(const QByteArray&);
    void setNwsIcon(const string&);
    void connect(const std::function<void()>&);
    QLabel * getView();
    QByteArray bytes;

private:
    ClickableLabel * image{};
    PhotoSizeEnum size{};
    int width{};
    int height{};
    std::function<int()> getPhotoHeight;
};

#endif  // PHOTO_H
