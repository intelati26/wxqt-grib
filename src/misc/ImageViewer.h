// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <string>
#include "ui/Photo.h"
#include "ui/VBox.h"
#include "ui/Window.h"

using std::string;

class ImageViewer : public Window {
public:
    ImageViewer(Window *, const QByteArray&, string = "");
    ImageViewer(Window *, const string&, string = "");

private:
    void resizeEventCustom() override;
    VBox box;
    Photo photo;
};

#endif  // IMAGEVIEWER_H
