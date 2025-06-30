// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef OBJECTANIMATEPARENT_H
#define OBJECTANIMATEPARENT_H

#include <vector>
#include "ui/ButtonToggle.h"
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::vector;

class ObjectAnimateParent : public Widget2 {

public:
    ObjectAnimateParent(Window *);
    virtual void animateClicked() = 0;
    virtual void stopAnimate() = 0;
    virtual void stopAnimateNoDownload() = 0;
    void setVisible(bool);
    void setFrameCount(int);
    // virtual void setActive();
    QPushButton * getView();

protected:
    Window * parent;
    ButtonToggle button;
    size_t frameCount;

private:
    virtual void loadAnimationFrame(int) = 0;
    virtual void downloadFrames() = 0;
};

#endif  // OBJECTANIMATEPARENT_H
