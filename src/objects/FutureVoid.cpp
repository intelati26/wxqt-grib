// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/FutureVoid.h"
#include <QtConcurrent/QtConcurrent>
#include "util/AppState.h"
#include "util/Activity.h"
#include "util/NetPriority.h"
#include "objects/NetManager.h"
#include <QObject>

FutureVoid::FutureVoid(Window * parent, const function<void()>& downloadFunc, const function<void()>& updateFunc)
    : updateFunc{updateFunc}
    , watcher{new QFutureWatcher<void>}
    , future{(NetManager::trackOwner(parent), QtConcurrent::run([downloadFunc, parent] {
          if (!AppState::quitting) {   // queued behind other work when the app starts closing: skip it
              const Activity::Task counted;
              const NetPriority::Carry owned{0, parent};   // its requests are the screen's: dropped if the screen closes first
              downloadFunc();
          }
      }))}
{
    watcher->setFuture(future);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, parent, [this] {
        update();
        delete watcher;
        delete this;
    });
}

void FutureVoid::update() {
    updateFunc();
    finished = true;
}

bool FutureVoid::isFinished() const {
    return finished;
}
