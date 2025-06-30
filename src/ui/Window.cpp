// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Window.h"
#include "settings/UIPreferences.h"
#include "util/UtilityUI.h"

Window::Window(QWidget * parent)
    : QMainWindow{parent}
    , centralWidget{new QWidget{this}}
    , shortcutClose1{QKeySequence{"W"}, this} // {"Ctrl+W"}
    , shortcutClose2{QKeySequence{Qt::Key_Escape}, this}
{
    setCentralWidget(centralWidget);
    // setStyleSheet("background-color: white;");
    QPalette pal = QPalette();
    pal.setColor(QPalette::Window, Qt::white);
    setAutoFillBackground(true);
    setPalette(pal);
    shortcutClose1.connect([this] { close(); });
    shortcutClose2.connect([this] { close(); });
    if (!UIPreferences::tiledWindows) {
        maximize();
    } else {
        const auto dims = UtilityUI::getScreenBounds();
        setSize(static_cast<int>(dims[0] / 2), static_cast<int>(dims[1] / 2));
    }
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

void Window::closeEvent(QCloseEvent * event) {
    closeEventCustom();
    event->accept();
}

void Window::closeEventCustom() {  // wxpy diff
}
