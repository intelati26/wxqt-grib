// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Window.h"
#include <algorithm>
#include <typeinfo>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>
#include <QWindow>
#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#endif
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

Window::Window(QWidget * parent)
    : QMainWindow{parent}
    , centralWidget{new QWidget{this}}
    , shortcutClose1{QKeySequence{"W"}, this} // {"Ctrl+W"}
    , shortcutClose2{QKeySequence{Qt::Key_Escape}, this}
{
    setCentralWidget(centralWidget);
    // background follows the application palette / theme (see UtilityTheme)
    shortcutClose1.connect([this] { close(); });
    shortcutClose2.connect([this] { close(); });
    if (embedNext) {   // a page of another screen
        embedNext = false;
        embedded = true;
        setWindowFlags(Qt::Widget);
        shortcutClose1.get()->setEnabled(false);
        shortcutClose2.get()->setEnabled(false);
        return;
    }
    if (!UIPreferences::tiledWindows) {
        maximize();
    } else {
        const auto dims = UtilityUI::getScreenBounds();
        setSize(static_cast<int>(dims[0] / 2), static_cast<int>(dims[1] / 2));
    }
}

bool Window::embedNext = false;

void Window::embedNextWindow() {
    embedNext = true;
}

void Window::setTitle(const string& s) {
    setWindowTitle(QString::fromStdString(s));
}

void Window::setSize(int x, int y) {
    resize(x, y);
    move(0, 0);
}

void Window::setSize2(int x, int y) {
    setFixedWidth(x);
    setFixedHeight(y);
}

int Window::getPhotoHeight() {
    QSize size1 = size();
    return size1.height();
}

int Window::getWindowWidth() {
    return width();
}

int Window::getWindowHeight() {
    return height();
}

void Window::maximize() {
    const auto dimensions = UtilityUI::getScreenBounds();
    resize(dimensions[0], dimensions[1]);
    move(0, 0);  // wxpy diff
    // showMaximized();
}

void Window::resizeEvent([[maybe_unused]] QResizeEvent * event) {
    resizeEventCustom();
}

void Window::resizeEventCustom() {  // wxpy diff
}

string Window::geometryKey() const {
    return string{"WINDOW_GEOMETRY_"} + typeid(*this).name();
}

// A window with a parent is, to the system, owned by it and stays above it: the tool windows would always cover the main window. Without that bond a click on the main window brings it over them,
// as any window comes forward (the parent stays the parent for the program: closing it closes them). The Settings switch MAIN_WINDOW_RAISES (on) turns it off.
void Window::releaseFromOwner() {
    if (embedded || parentWidget() == nullptr || !isWindow() || Utility::readPref("MAIN_WINDOW_RAISES", "true").compare(0, 1, "t") != 0) {
        return;
    }
    if (auto * handle = windowHandle()) {
        handle->setTransientParent(nullptr);
#ifdef Q_OS_WIN
        SetWindowLongPtr(reinterpret_cast<HWND>(handle->winId()), GWLP_HWNDPARENT, 0);   // (the owner of a window on Windows: Qt sets it once, from the parent)
#endif
    }
}

// the first time a screen is shown, put it where and as big as it was when last closed (if that is still on a
// connected screen)
void Window::showEvent(QShowEvent * event) {
    QMainWindow::showEvent(event);
    releaseFromOwner();
    if (geometryRestored || embedded) {
        return;
    }
    geometryRestored = true;
    const auto parts = QString::fromStdString(Utility::readPref(geometryKey(), "")).split(',');
    if (parts.size() != 4) {
        return;
    }
    const QRect saved{parts[0].toInt(), parts[1].toInt(), parts[2].toInt(), parts[3].toInt()};
    const auto * screen = QGuiApplication::screenAt(saved.center());
    if (screen == nullptr || saved.width() < 200 || saved.height() < 150) {
        return;
    }
    const auto available = screen->availableGeometry();
    QRect fitted{saved.topLeft(), QSize{std::min(saved.width(), available.width()), std::min(saved.height(), available.height())}};
    fitted.moveLeft(std::clamp(fitted.left(), available.left(), available.right() - fitted.width() + 1));
    fitted.moveTop(std::clamp(fitted.top(), available.top(), available.bottom() - fitted.height() + 1));
    setGeometry(fitted);
}

void Window::closeEvent(QCloseEvent * event) {
    const auto g = geometry();
    if (!embedded) Utility::writePref(geometryKey(), std::to_string(g.x()) + "," + std::to_string(g.y()) + "," + std::to_string(g.width()) + "," + std::to_string(g.height()));
    closeEventCustom();
    event->accept();
}

void Window::closeEventCustom() {  // wxpy diff
}
