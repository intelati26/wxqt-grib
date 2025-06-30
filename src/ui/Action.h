// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef ACTION_H
#define ACTION_H

#include <string>
#include <QAction>

using std::string;

class Action {
public:
    Action(const string&, QObject *);
    QAction * get() const;

private:
    QAction * action;
};

#endif  // ACTION_H
