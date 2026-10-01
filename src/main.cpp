// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <QApplication>
#include <exception>
#include <QMetaObject>
#include <QDebug>
#include <QPixmap>
#include <QTimer>
#include "common/GlobalVariables.h"
#include "ui/MainWindow.h"
#include "util/CrashLog.h"
#include "util/MyApplication.h"
#include "util/UtilityTheme.h"

namespace {
    // an exception thrown inside an event handler would otherwise unwind through Qt and end the program: log it, keep running
    class WxqtApplication : public QApplication {
    public:
        using QApplication::QApplication;
        bool notify(QObject * receiver, QEvent * event) override {
            try {
                return QApplication::notify(receiver, event);
            } catch (const std::exception& e) {
                CrashLog::write(std::string{"exception in an event handler (ignored): "} + e.what() + " [" +
                                (receiver != nullptr ? receiver->metaObject()->className() : "?") + "]");
            } catch (...) {
                CrashLog::write("unknown exception in an event handler (ignored)");
            }
            return false;
        }
    };
}

int main(int argc, char * argv[]) {
    WxqtApplication a{argc, argv};
    bool debug = qEnvironmentVariable("WXQT_DEBUG") == "1";
    for (const auto& argument : a.arguments()) {
        debug = debug || argument == "--debug";
    }
    CrashLog::install(debug);
    MyApplication::onCreate();
    UtilityTheme::apply();
    a.setWindowIcon(QIcon{QString::fromStdString(GlobalVariables::imageDir) + "wx_launcher.png"});
    if (a.arguments().size() == 3 && a.arguments()[1] == "-r") {
        return a.exec();
    } else {
        MainWindow w;
        w.show();
        QObject::connect(&a, &QCoreApplication::aboutToQuit, &a, [] { CrashLog::write("---- wxqt closing normally ----"); });
        // development aid: WXQT_GRAB=<file.png>[,<milliseconds>] saves a picture of the main window and quits, so a
        // layout can be checked without a display (run with QT_QPA_PLATFORM=offscreen)
        const auto grab = qEnvironmentVariable("WXQT_GRAB").split(',');
        if (!grab[0].isEmpty()) {
            const auto delay = grab.size() > 1 ? grab[1].toInt() : 8000;
            QTimer::singleShot(delay, &a, [&w, &a, file = grab[0]] {
                w.grab().save(file);
                a.quit();
            });
        }
        return a.exec();
    }
}
