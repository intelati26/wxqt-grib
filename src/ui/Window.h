// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef WINDOW_H
#define WINDOW_H

#include <string>
#include <QCloseEvent>
#include <QMainWindow>
#include <QResizeEvent>
#include "ui/Shortcut.h"

using std::string;

class Window : public QMainWindow {
public:
    explicit Window(QWidget *);
    void setTitle(const string&);
    void setSize(int, int);
    void setSize2(int, int);
    int getPhotoHeight();
    int getWindowWidth();
    int getWindowHeight();
    void maximize();
    // The next Window made is a page of another screen (the model screen's sources), not a window of its own: no size or place of its own, no close keys
    // (the screen's are used), shown inside the screen that holds it.
    static void embedNextWindow();
    bool isEmbedded() const { return embedded; }
    QWidget * centralWidget;

protected:
    void resizeEvent(QResizeEvent *) override;
    void closeEvent(QCloseEvent *) override;
    void showEvent(QShowEvent *) override;

private:
    string geometryKey() const;   // one saved size / position per kind of screen
    bool geometryRestored{false};
    bool embedded{false};
    static bool embedNext;
    virtual void resizeEventCustom();
    virtual void closeEventCustom();
    Shortcut shortcutClose1;
    Shortcut shortcutClose2;
};

#endif  // WINDOW_H
