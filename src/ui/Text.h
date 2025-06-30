// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#ifndef TEXT_H
#define TEXT_H

#include <string>
#include <QLabel>
#include "ui/Widget2.h"
#include "ui/Window.h"

using std::string;

class Text : public Widget2 {
public:
    Text(Window *, const string& = "");
    void setText(const QString&);
    void setText(const string&);
    void setWordWrap(bool);
    void setFixedWidth();
    void setBlue();
    void setGray();
    void setBold();
    void setBlueOnWhite();
    void setVisible(bool);
    QLabel * getView();

private:
    QLabel * textView;
    QString text;
};

#endif  // TEXT_H
