// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/PopoverMenu.h"
#include <QMenu>

PopoverMenu::PopoverMenu(Window * parent, const string& buttonLabel, const vector<string>& buttonList, const function<void(string)>& fnAction)
    : button{new QPushButton{QString::fromStdString(buttonLabel), parent}}
    , menu{new QMenu{parent}}
{
    QObject::connect(menu, &QMenu::triggered, parent, [fnAction] (QAction * a) { fnAction(a->text().toStdString()); });
    for (const auto& item : buttonList) {
        QAction * action = menu->addAction(QString::fromStdString(item));
        action->setIconVisibleInMenu(false);
    }
    button->setMenu(menu);
}

QPushButton * PopoverMenu::getView() {
    return button;
}
