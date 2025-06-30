#ifndef DIVIDERLINE_H
#define DIVIDERLINE_H

#include <QFrame>
#include "ui/Widget2.h"
#include "ui/Window.h"

class DividerLine : public Widget2 {
public:
    explicit DividerLine(Window *);
    QFrame * getView();

private:
    QFrame * line;
};

#endif  // DIVIDERLINE_H
