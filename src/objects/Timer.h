// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TIMER_H
#define TIMER_H

#include <functional>
#include <QTimer>
#include "ui/Window.h"

using std::function;

class Timer {
public:
    Timer(Window *, const function<void()>&);
    bool isRunning() const;
    void stop();
    void start(int);

private:
    QTimer * timer;
};

#endif  // TIMER_H
