// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/FutureBytes.h"
#include <QtConcurrent/QtConcurrent>
#include "util/AppState.h"
#include "util/Activity.h"
#include "util/NetPriority.h"
#include "objects/NetManager.h"
#include <QObject>
#include "util/UtilityIO.h"

FutureBytes::FutureBytes(Window * parent, const string& url, const function<void(const QByteArray&)>& updateFunc)
    : updateFunc{updateFunc}
    , watcher{new QFutureWatcher<void>}
    , future{(NetManager::trackOwner(parent), QtConcurrent::run([this, url, parent] {
          if (!AppState::quitting) {
              const Activity::Task counted;
              const NetPriority::Carry owned{0, parent};
              this->ba = UtilityIO::downloadAsByteArray(url);
          }
      }))}
{
    watcher->setFuture(future);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, parent, [&] {
        this->updateFunc(ba);
        delete watcher;
        delete this;
    });
}
