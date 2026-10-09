// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "objects/FutureText.h"
#include <QtConcurrent/QtConcurrent>
#include "util/AppState.h"
#include <QObject>
#include "objects/NetManager.h"
#include "util/NetPriority.h"
#include "util/DownloadText.h"

FutureText::FutureText(Window * parent, const string& url, const function<void(string)>& updateFunc)
    : updateFunc{updateFunc}
    , watcher{new QFutureWatcher<void>}
    , future{(NetManager::trackOwner(parent), QtConcurrent::run([this, url, parent] {
          if (!AppState::quitting) {
              const NetPriority::Carry owned{0, parent};
              html = DownloadText::byProduct(url);
          }
      }))}
{
    watcher->setFuture(future);
    QObject::connect(watcher, &QFutureWatcher<void>::finished, parent, [&] {
        this->updateFunc(html);
    });
}
