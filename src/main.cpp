// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <QApplication>
#include <QDebug>
#include "common/GlobalVariables.h"
#include "ui/MainWindow.h"
#include "util/MyApplication.h"
#include "util/UtilityTheme.h"

int main(int argc, char * argv[]) {
    QApplication a{argc, argv};
    MyApplication::onCreate();
    UtilityTheme::apply();
    a.setWindowIcon(QIcon{QString::fromStdString(GlobalVariables::imageDir) + "wx_launcher.png"});
    if (a.arguments().size() == 3 && a.arguments()[1] == "-r") {
        return a.exec();
    } else {
        MainWindow w;
        w.show();
        return a.exec();
    }
}
