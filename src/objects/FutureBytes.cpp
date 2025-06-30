// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/FutureBytes.h"
#include <QtConcurrent/QtConcurrent>
#include <QObject>
#include "util/UtilityIO.h"

FutureBytes::FutureBytes(Window * parent, const string& url, const function<void(const QByteArray&)>& updateFunc)
    : updateFunc{updateFunc}
    , watcher{new QFutureWatcher<void>}
    , future{QtConcurrent::run([this, url] { this->ba = UtilityIO::downloadAsByteArray(url); })}
{
    watcher->setFuture(future);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, parent, [&] {
        this->updateFunc(ba);
        delete watcher;
        delete this;
    });
}
