// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTANIMATENEXRAD_H
#define OBJECTANIMATENEXRAD_H

#include <vector>
#include "objects/ObjectAnimateParent.h"
#include "objects/TimeLine.h"
#include "radar/NexradWidget.h"
#include "ui/ComboBox.h"
#include "ui/Window.h"

using std::vector;

class ObjectAnimateNexrad : public ObjectAnimateParent {

public:
    ObjectAnimateNexrad(Window *, vector<NexradWidget *> *, ComboBox *, ComboBox *);
    void animateClicked() override;
    void stopAnimate() override;
    void stopAnimateNoDownload() override;
    void setAnimationSpeed();
    void setAnimationCount();

private:
    void loadAnimationFrame(int) override;
    void downloadFrames() override;
    int animationSpeed{2500};
    vector<NexradWidget *> * nexradList;
    ComboBox * comboboxAnimCount;
    ComboBox * comboboxAnimSpeed;
    TimeLine timeLine;
};

#endif  // OBJECTANIMATENEXRAD_H
