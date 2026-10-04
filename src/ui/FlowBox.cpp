// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/FlowBox.h"
#include <vector>
using std::vector;
#include <algorithm>
#include "settings/UIPreferences.h"
#include "util/UtilityUI.h"

FlowLayout::FlowLayout(int spacing)
    : gap{spacing}
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout() {
    while (QLayoutItem * item = takeAt(0)) {
        delete item;
    }
}

void FlowLayout::addItem(QLayoutItem * item) {
    items.append(item);
}

int FlowLayout::count() const {
    return items.size();
}

QLayoutItem * FlowLayout::itemAt(int index) const {
    return items.value(index);
}

QLayoutItem * FlowLayout::takeAt(int index) {
    return index >= 0 && index < items.size() ? items.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const {
    return {};
}

bool FlowLayout::hasHeightForWidth() const {
    return true;
}

int FlowLayout::heightForWidth(int width) const {
    return doLayout(QRect{0, 0, width, 0}, true);
}

void FlowLayout::setGeometry(const QRect& rect) {
    QLayout::setGeometry(rect);
    doLayout(rect, false);
}

QSize FlowLayout::sizeHint() const {
    return minimumSize();
}

// the widest single child: the window may be narrowed until only one fits per row
QSize FlowLayout::minimumSize() const {
    QSize size;
    for (const auto * item : items) {
        size = size.expandedTo(item->minimumSize());
    }
    const auto margins = contentsMargins();
    return size + QSize{margins.left() + margins.right(), margins.top() + margins.bottom()};
}

// returns the height used; the items of a row are centred on it vertically (pictures of different heights share a middle line, not a top)
int FlowLayout::doLayout(const QRect& rect, bool onlyMeasure) const {
    const auto margins = contentsMargins();
    const auto area = rect.adjusted(margins.left(), margins.top(), -margins.right(), -margins.bottom());
    int x = area.x();
    int y = area.y();
    int rowHeight = 0;
    struct Placed {
        QLayoutItem * item;
        int x;
        QSize size;
    };
    vector<Placed> row;
    const auto flushRow = [&] {
        if (!onlyMeasure) {
            for (const auto& placed : row) {
                if (equalHeights) {
                    placed.item->setGeometry(QRect{QPoint{placed.x, y}, QSize{placed.size.width(), rowHeight}});
                } else {
                    placed.item->setGeometry(QRect{QPoint{placed.x, y + (rowHeight - placed.size.height()) / 2}, placed.size});
                }
            }
        }
        row.clear();
    };
    for (auto * item : items) {
        if (item->isEmpty()) {
            continue;
        }
        const auto size = item->sizeHint();
        if (x > area.x() && x + size.width() > area.right() + 1) {
            flushRow();
            x = area.x();
            y += rowHeight + gap;
            rowHeight = 0;
        }
        row.push_back(Placed{item, x, size});
        x += size.width() + gap;
        rowHeight = std::max(rowHeight, size.height());
    }
    flushRow();
    return y + rowHeight - rect.y() + margins.bottom();
}

FlowBox::FlowBox()
    : flow{new FlowLayout{UIPreferences::boxPadding}}
{}

void FlowBox::addWidget(Widget2& w) {
    flow->addWidget(w.getView());
}

void FlowBox::addWidgetReal(QWidget * w) {
    flow->addWidget(w);
}

void FlowBox::removeChildren() {
    UtilityUI::removeChildren(flow);
}

void FlowBox::detachAll() {
    while (QLayoutItem * item = flow->takeAt(0)) {
        delete item;   // the wrapper only; the widget lives on
    }
}

QLayout * FlowBox::getView() {
    return flow;
}
