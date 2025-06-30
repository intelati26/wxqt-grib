// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/TimeLine.h"

TimeLine::TimeLine(Window * parent, int speed, size_t count, const function<void(int)>& updateFn)
    : count{count}
    , timeLine{new QTimeLine{speed, parent}}
{
    timeLine->setFrameRange(0, count);
    timeLine->setLoopCount(0);
    QObject::connect(timeLine, &QTimeLine::frameChanged, parent, updateFn);
}

void TimeLine::stop() {
    running = false;
    timeLine->stop();
}

void TimeLine::setCount(int count) {
    this->count = count;
    timeLine->setFrameRange(0, count);
}

void TimeLine::setSpeed(int s) {
    timeLine->setDuration(s);
}

int TimeLine::getCount() const {
    return count;
}

void TimeLine::start() {
    running = true;
    timeLine->start();
}

bool TimeLine::isRunning() const {
    return running;
}

QTimeLine::State TimeLine::state() {
    return timeLine->state();
}
