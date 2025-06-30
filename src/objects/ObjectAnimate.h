// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTANIMATE_H
#define OBJECTANIMATE_H

#include <functional>
#include <string>
#include <vector>
#include "objects/ObjectAnimateParent.h"
#include "objects/TimeLine.h"
#include "ui/ButtonToggle.h"
#include "ui/Photo.h"
#include "ui/Window.h"

using std::function;
using std::string;
using std::vector;

class ObjectAnimate : public ObjectAnimateParent {

public:
    ObjectAnimate(
        Window *,
        Photo *,
        const function<vector<string>(string, string, int)>&,
        const function<void()>&
    );
    void stopAnimate() override;
    void stopAnimateNoDownload() override;
    void animateClicked() override;
    string product;
    string sector;
    function<vector<string>(string, string, int)> getFunction;

private:
    void loadAnimationFrame(int) override;
    void downloadFrames() override;
    Photo * photo;
    function<void()> downloadFunction;
    int animationSpeed;
    TimeLine timeLine;
    vector<QByteArray> animationFrames;
    vector<string> urls;
};

#endif  // OBJECTANIMATE_H
