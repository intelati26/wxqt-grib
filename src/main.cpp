// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <QApplication>
#include <QMainWindow>
#include <exception>
#include <QMetaObject>
#include <QDebug>
#include <QPixmap>
#include <QTabWidget>
#include <QThreadPool>
#include <QTimer>
#include "common/GlobalVariables.h"
#include "hurricane/ReconViewer.h"
#include "spc/SpcSwoStateGraphics.h"
#include "ui/MainWindow.h"
#include "util/AppState.h"
#include "util/ContactPrompt.h"
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
        QObject::connect(&a, &QCoreApplication::aboutToQuit, &a, [] {
            AppState::quitting = true;   // queued background jobs skip their work
            // let the running ones finish now, while the application object still exists: a download that is still going when
            // it is destroyed corrupts memory
            QThreadPool::globalInstance()->waitForDone();
            CrashLog::write("---- wxqt closing normally ----");
        });
        // development aids (run with QT_QPA_PLATFORM=offscreen): WXQT_OPEN=<toolbar entry id, e.g. ntor.png> opens that
        // tool; WXQT_GRAB=<file.png>[,<milliseconds>] saves a picture of the newest tool window (else the main
        // window) and quits, so a screen can be checked without a display; WXQT_SIZE=<w>x<h> sizes the tool window first; WXQT_TAB=<n> picks a tab
        QWidget * opened = nullptr;
        if (const auto route = qEnvironmentVariable("WXQT_OPEN"); !route.isEmpty()) {
            const auto before = QApplication::topLevelWidgets();
            if (route.startsWith("recon:")) {   // WXQT_OPEN=recon:<NHC id>:<name>, e.g. recon:al092026:Isaias: the one-flight recon page
                const auto parts = route.split(':');
                new ReconViewer{&w, parts.value(1).toStdString(), parts.value(2).toStdString()};
            } else if (route.startsWith("swostate:")) {   // WXQT_OPEN=swostate:<day>: the SPC convective outlook's state graphics for that day
                new SpcSwoStateGraphics{&w, route.section(':', 1, 1).toInt()};
            } else {
                w.openRoute(route.toStdString());
            }
            for (auto * widget : QApplication::topLevelWidgets()) {
                if (widget != &w && !before.contains(widget) && qobject_cast<QMainWindow *>(widget) != nullptr) {   // not a combo box's hidden popup
                    opened = widget;
                }
            }
        }
        if (const auto size = qEnvironmentVariable("WXQT_SIZE").split('x'); opened != nullptr && size.size() == 2) {
            opened->resize(size[0].toInt(), size[1].toInt());   // WXQT_SIZE=1400x1000: the tool window's size for the picture
        }
        if (const auto size = qEnvironmentVariable("WXQT_SIZE").split('x'); opened == nullptr && size.size() == 2) {
            w.resize(size[0].toInt(), size[1].toInt());   // no tool opened: the home screen itself
        }
        if (const auto tab = qEnvironmentVariable("WXQT_TAB"); opened != nullptr && !tab.isEmpty()) {
            for (auto * tabs : opened->findChildren<QTabWidget *>()) {
                tabs->setCurrentIndex(tab.toInt());   // WXQT_TAB=<n>: show that tab of a tabbed tool (Settings)
            }
        }
        const auto grab = qEnvironmentVariable("WXQT_GRAB").split(',');
        if (!grab[0].isEmpty()) {
            const auto delay = grab.size() > 1 ? grab[1].toInt() : 8000;
            QTimer::singleShot(delay, &a, [&w, &a, opened, file = grab[0]] {
                (opened != nullptr ? opened->grab() : w.grab()).save(file);
                a.quit();
            });
        }
        // the contact-email question, once the window is up (not in the development screenshot runs, which have nobody to answer)
        if (grab[0].isEmpty() && qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen") {
            QTimer::singleShot(0, &w, [&w] { ContactPrompt::askIfMissing(&w); });
        }
        return a.exec();
    }
}
