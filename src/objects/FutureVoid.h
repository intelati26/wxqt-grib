// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef FUTUREVOID_H
#define FUTUREVOID_H

#include <functional>
#include <QFutureWatcher>
#include "ui/Window.h"

using std::function;

class FutureVoid {
public:
    FutureVoid(Window *, const function<void()>&, const function<void()>&);
    bool isFinished() const;

private:
    void update();
    function<void()> updateFunc;
    QFutureWatcher<void> * watcher;
    QFuture<void> future;
    bool finished;
};

#endif  // FUTUREVOID_H
