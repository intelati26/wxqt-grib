// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TIMELINE_H
#define TIMELINE_H

#include <functional>
#include <QObject>
#include <QTimeLine>
#include "ui/Window.h"

using std::function;

class TimeLine {
public:
    TimeLine(Window *, int, size_t, const function<void(int)>&);
    void stop();
    void setCount(int);
    void setSpeed(int);
    int getCount() const;
    void start();
    bool isRunning() const;

private:
    QTimeLine::State state();
    size_t count;
    QTimeLine * timeLine;
    bool running{false};
};

#endif  // TIMELINE_H
