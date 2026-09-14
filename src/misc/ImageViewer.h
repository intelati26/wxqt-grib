// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef IMAGEVIEWER_H
#define IMAGEVIEWER_H

#include <string>
#include <QByteArray>
#include "ui/Button.h"
#include "ui/HBox.h"
#include "ui/Shortcut.h"
#include "ui/VBox.h"
#include "ui/Window.h"
#include "ui/ZoomImage.h"

using std::string;

class ImageViewer : public Window {
public:
    ImageViewer(Window *, const QByteArray&, string = "");
    ImageViewer(Window *, const string&, string = "");

private:
    void resizeEventCustom() override;
    void setBytes(const QByteArray&);
    void save();
    string suggestedFileName(const QByteArray& savedBytes) const;
    VBox box;
    HBox boxTop;
    ZoomImage image;
    Button saveButton;
    Shortcut saveShortcut;
    QByteArray imageBytes;
    string titleText;
    string sourceUrl;
};

#endif  // IMAGEVIEWER_H
