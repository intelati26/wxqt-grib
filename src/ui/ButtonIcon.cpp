// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/ButtonIcon.h"

ButtonIcon::ButtonIcon(QPushButton * button, const string& imageName, int size) {
    const auto pixmap = QPixmap{QString::fromStdString(imageName)};
    const auto buttonIcon = QIcon{pixmap};
    button->setIcon(buttonIcon);
    if (size != 0) {
        button->setIconSize(QSize{size, size});
    }
}
