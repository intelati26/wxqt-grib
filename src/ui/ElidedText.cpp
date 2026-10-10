// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ElidedText.h"
#include <QAbstractButton>
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
            for (auto * button : panel->findChildren<QAbstractButton *>()) {
                if (button->text().isEmpty() && button->property("fullText").toString().isEmpty()) {
                    continue;
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
