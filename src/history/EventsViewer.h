// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef EVENTSVIEWER_H
#define EVENTSVIEWER_H

#include <vector>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLabel>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QTimer>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QTableWidget>
#include "history/HistoryEvent.h"
#include "ui/Window.h"

// The history page (first slice, docs/history-archive-plan.md section 7 / 8): the events, an entry for a new one (dates, any point picked on the map, a name that defaults to the
// datetime and position, a note and the moments inside it), and for the event chosen what each kind of data has for that time and place, with a button that opens the screen.
class EventsViewer : public Window {
public:
    explicit EventsViewer(Window * parent);
    void openCatalog();                // tornado outbreaks (and later storms) to take an event from

private:
    void fillList(const std::string& select = {});
    void showEvent(size_t index);
    void readForm(HistoryEvent&) const;
    void fillAvailability(const HistoryEvent&);
    QDateTime atTime() const;          // the moment the screens open at: the start, a moment of the event, or the end
    void fillAtBox(const HistoryEvent&);
    void loadOutlooks();               // the SPC day 1 outlook of the event's convective day, added to the event on its own
    void newEvent();
    void saveEvent();
    void deleteEvent();
    void pickPoint();
    void closeEventCustom() override {}

    std::vector<HistoryEvent> events;
    int current{-1};
    QListWidget * list{};
    QLineEdit * name{};
    QDateTimeEdit * start{};
    QDateTimeEdit * end{};
    QDoubleSpinBox * lat{};
    QDoubleSpinBox * lon{};
    QDoubleSpinBox * radius{};
    QPlainTextEdit * note{};
    QPlainTextEdit * moments{};
    QTableWidget * table{};
    QComboBox * atBox{};
    QComboBox * issuance{};
    std::vector<QLabel *> outlookImages;
    QLabel * outlookNote{};
    int outlookGeneration{0};
    QTimer outlookTimer;
};

#endif  // EVENTSVIEWER_H
