// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ElidedText.h"
#include <algorithm>
#include <QAbstractButton>
#include <QAbstractItemView>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QLabel>
#include <QObject>
#include <QTimer>

namespace {
    class Filter : public QObject {
    public:
        explicit Filter(QWidget * panel) : QObject{panel}, panel{panel} {}

        void apply() {
            // a drop-down is as wide as its longest item and would push the panel wider than the room it has (everything past the edge is cut off): it takes the room it is given, shows the choice
            // elided and has it whole as its tooltip; the list that opens is as wide as its longest item
            for (auto * combo : panel->findChildren<QComboBox *>()) {
                if (!combo->property("fitted").toBool()) {
                    combo->setProperty("fitted", true);
                    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
                    combo->setMinimumContentsLength(6);
                    combo->setMinimumWidth(0);
                    combo->setSizePolicy(QSizePolicy::Ignored, combo->sizePolicy().verticalPolicy());
                    QObject::connect(combo, &QComboBox::currentIndexChanged, combo, [combo] { combo->setToolTip(combo->currentText()); });
                }
                combo->setToolTip(combo->currentText());
                int widest = 0;
                for (int i = 0; i < combo->count(); i++) {
                    widest = std::max(widest, combo->fontMetrics().horizontalAdvance(combo->itemText(i)));
                }
                if (auto * view = combo->view()) {
                    view->setMinimumWidth(widest + 40);
                }
            }
            for (auto * button : panel->findChildren<QAbstractButton *>()) {
                if (button->text().isEmpty() && button->property("fullText").toString().isEmpty()) {
                    continue;
                }
                if (button->property("fullText").isValid() && button->text() != button->property("shownText").toString() && button->text() != button->property("fullText").toString()) {
                    button->setProperty("fullText", button->text());   // the program has set another text since: that is the whole text now
                    button->setToolTip(button->text());
                }
                if (!button->property("fullText").isValid()) {
                    button->setProperty("fullText", button->text());   // the whole text, kept the first time
                    if (button->toolTip().isEmpty()) {
                        button->setToolTip(button->text());
                    }
                }
                const QString full = button->property("fullText").toString();
                if (full.isEmpty() || button->width() <= 40) {
                    continue;
                }
                // the room beside the indicator (and an icon, when there is one)
                const int room = button->width() - 30 - (button->icon().isNull() ? 0 : button->iconSize().width() + 6);
                const QString shown = button->fontMetrics().elidedText(full, Qt::ElideRight, std::max(40, room));
                if (shown != button->text()) {
                    // a button with a size hint wider than the panel: take its minimum so the panel does not grow
                    button->setText(shown);
                    button->setProperty("shownText", shown);
                    button->setMinimumWidth(0);
                    button->setSizePolicy(QSizePolicy::Ignored, button->sizePolicy().verticalPolicy());
                }
            }
        }

    protected:
        bool eventFilter(QObject * object, QEvent * event) override {
            if (object == panel && (event->type() == QEvent::Resize || event->type() == QEvent::Show || event->type() == QEvent::LayoutRequest)) {
                QTimer::singleShot(0, this, [this] { apply(); });
            }
            return false;
        }

    private:
        QWidget * panel;
    };
}

void ElidedText::install(QWidget * panel) {
    auto * filter = new Filter{panel};
    panel->installEventFilter(filter);
    QTimer::singleShot(0, filter, [filter] { filter->apply(); });
}
