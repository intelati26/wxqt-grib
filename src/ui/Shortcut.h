// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef SHORTCUT_H
#define SHORTCUT_H

#include <functional>
#include <QShortcut>
#include <QWidget>

using std::function;

class Shortcut {
public:
    Shortcut(const QKeySequence&, QWidget *);
    QShortcut * get();
    void connect(const function<void()>&);

private:
    QWidget * parent;
    QShortcut * qshortcut;
};

#endif  // SHORTCUT_H
