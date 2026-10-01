// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef FLOWBOX_H
#define FLOWBOX_H

#include <QLayout>
#include <QList>
#include <QRect>
#include <QWidget>
#include "ui/Box.h"
#include "ui/Widget2.h"

// Lays its children out left to right and starts a new row when the next one would not fit in the width it was
// given, so a row is never longer than the window. Children keep their own (fixed) size. Based on Qt's
// flow layout example.
class FlowLayout : public QLayout {
public:
    explicit FlowLayout(int spacing);
    ~FlowLayout() override;
    void addItem(QLayoutItem *) override;
    int count() const override;
    QLayoutItem * itemAt(int) const override;
    QLayoutItem * takeAt(int) override;
    Qt::Orientations expandingDirections() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;
    void setGeometry(const QRect&) override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;

private:
    int doLayout(const QRect&, bool onlyMeasure) const;
    QList<QLayoutItem *> items;
    int gap;
};

// the Box wrapper, so a flow can stand wherever a VBox / HBox does
class FlowBox : public Box {
public:
    FlowBox();
    void addWidget(Widget2&);
    void addWidgetReal(QWidget *);
    void addStretch() {}   // a flow has no free space to absorb
    void removeChildren();
    void detachAll();   // takes every item out without deleting the widgets, so they can be re-added in another order
    QLayout * getView() override;

private:
    FlowLayout * flow;
};

#endif  // FLOWBOX_H
