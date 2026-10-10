// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef NUMBERITEM_H
#define NUMBERITEM_H

#include <QTableWidgetItem>

// A table cell that shows some text and sorts by a number (a click on the column heading sorts the table): "1,204" after "98", "-" (no number) after every number.
class NumberItem : public QTableWidgetItem {
public:
    NumberItem(const QString& text, double number, bool hasNumber = true) : QTableWidgetItem{text}, number{number}, hasNumber{hasNumber} {
        setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        setFlags(flags() & ~Qt::ItemIsEditable);
    }
    bool operator<(const QTableWidgetItem& other) const override {
        const auto * o = dynamic_cast<const NumberItem *>(&other);
        if (o == nullptr) {
            return QTableWidgetItem::operator<(other);
        }
        if (hasNumber != o->hasNumber) {
            return !hasNumber;   // a cell with no number sorts as the smallest: last when the order is the biggest first
        }
        return number < o->number;
    }

private:
    double number;
    bool hasNumber;
};

#endif  // NUMBERITEM_H
