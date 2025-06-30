// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef NEXRADSTATEANIMATION_H
#define NEXRADSTATEANIMATION_H

#include <vector>
#include "objects/FileStorage.h"
#include "radar/NexradLevelData.h"
#include "radar/NexradState.h"

using std::vector;

class NexradStateAnimation {
public:
    void processAnimationFiles(int, FileStorage *, NexradState *);
    vector<NexradLevelData> levelDataList;

private:
    void process(int);
};

#endif  // NEXRADSTATEANIMATION_H
