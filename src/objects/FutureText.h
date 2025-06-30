// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef FUTURETEXT_H
#define FUTURETEXT_H

#include <functional>
#include <string>
#include <QFutureWatcher>
#include "ui/Window.h"

using std::function;
using std::string;

class FutureText {
public:
    FutureText(Window *, const string&, const function<void(string)>&);

private:
    function<void(string)> updateFunc;
    QFutureWatcher<void> * watcher;
    QFuture<void> future;
    string html;
};

#endif  // FUTURETEXT_H
