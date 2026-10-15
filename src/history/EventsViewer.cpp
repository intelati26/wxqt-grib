// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/EventsViewer.h"
#include <algorithm>
#include <QDesktopServices>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>
#include "history/PointPicker.h"
#include "mapkit/MasterMapViewer.h"
#include "spc/SpcStormReports.h"
#include "util/Utility.h"

namespace {
    QString momentsText(const HistoryEvent& e) {
        QStringList lines;
        for (const auto& m : e.moments) {
            lines << m.time.toUTC().toString("yyyy-MM-dd HH:mm") + " " + QString::fromStdString(m.label);
        }
        return lines.join("\n");
    }

    std::vector<HistoryEvent::Moment> parseMoments(const QString& text) {
        std::vector<HistoryEvent::Moment> out;
        for (const auto& line : text.split('\n', Qt::SkipEmptyParts)) {
            const QString trimmed = line.trimmed();
            const auto when = QDateTime::fromString(trimmed.left(16), "yyyy-MM-dd HH:mm");
            if (when.isValid()) {
                QDateTime utc{when.date(), when.time(), QTimeZone::utc()};
                out.push_back({utc, trimmed.mid(16).trimmed().toStdString()});
            }
        }
        return out;
    }
}

EventsViewer::EventsViewer(Window * parent) : Window{parent} {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("History: events");
    events = HistoryEvents::load();

    list = new QListWidget{this};
    list->setMinimumWidth(240);
    auto * newButton = new QPushButton{"New event", this};
    auto * left = new QVBoxLayout;
    left->addWidget(new QLabel{"Events", this});
    left->addWidget(list, 1);
    left->addWidget(newButton);

    name = new QLineEdit{this};
    name->setPlaceholderText("(left empty: the date and position)");
    start = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addDays(-1), this};
    end = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addDays(-1), this};
    for (auto * edit : {start, end}) {
        edit->setDisplayFormat("yyyy-MM-dd HH:mm");
        edit->setTimeZone(QTimeZone::utc());
        edit->setCalendarPopup(true);
        edit->setToolTip("UTC");
    }
    lat = new QDoubleSpinBox{this};
    lat->setRange(-90.0, 90.0);
    lat->setDecimals(4);
    lon = new QDoubleSpinBox{this};
    lon->setRange(-180.0, 180.0);
    lon->setDecimals(4);
    radius = new QDoubleSpinBox{this};
    radius->setRange(5.0, 2500.0);
    radius->setValue(100.0);
    radius->setSuffix(" km");
    auto * pick = new QPushButton{"Pick on the map...", this};
    note = new QPlainTextEdit{this};
    note->setMaximumHeight(70);
    moments = new QPlainTextEdit{this};
    moments->setPlaceholderText("one per line:  2026-01-14 18:00  the heaviest snow");
    moments->setMaximumHeight(80);
    auto * form = new QFormLayout;
    form->addRow("Name", name);
    form->addRow("From (UTC)", start);
    form->addRow("To (UTC)", end);
    auto * point = new QHBoxLayout;
    point->addWidget(lat);
    point->addWidget(lon);
    point->addWidget(pick);
    form->addRow("Latitude / longitude", point);
    form->addRow("Radius", radius);
    form->addRow("Note", note);
    form->addRow("Moments", moments);
    auto * buttons = new QHBoxLayout;
    auto * save = new QPushButton{"Save", this};
    auto * remove = new QPushButton{"Delete", this};
    buttons->addWidget(save);
    buttons->addWidget(remove);
    buttons->addStretch();

    table = new QTableWidget{0, 4, this};
    table->setHorizontalHeaderLabels({"Data", "Open from", "For this event", ""});
    table->horizontalHeader()->setStretchLastSection(false);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    table->setColumnWidth(1, 120);
    table->setColumnWidth(2, 170);
    table->setColumnWidth(3, 70);
    table->setWordWrap(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);

    auto * right = new QVBoxLayout;
    right->addLayout(form);
    right->addLayout(buttons);
    right->addWidget(new QLabel{"What there is for this time and place", this});
    right->addWidget(table, 1);
    auto * all = new QHBoxLayout;
    all->addLayout(left);
    all->addLayout(right, 1);
    auto * central = new QWidget{this};
    central->setLayout(all);
    setCentralWidget(central);
    setSize(1000, 720);

    QObject::connect(newButton, &QPushButton::clicked, [this] { newEvent(); });
    QObject::connect(save, &QPushButton::clicked, [this] { saveEvent(); });
    QObject::connect(remove, &QPushButton::clicked, [this] { deleteEvent(); });
    QObject::connect(pick, &QPushButton::clicked, [this] { pickPoint(); });
    QObject::connect(list, &QListWidget::currentRowChanged, [this] (int row) { if (row >= 0) { showEvent(static_cast<size_t>(row)); } });
    const auto refresh = [this] {
        HistoryEvent e;
        readForm(e);
        fillAvailability(e);
    };
    QObject::connect(start, &QDateTimeEdit::dateTimeChanged, refresh);
    QObject::connect(lat, &QDoubleSpinBox::valueChanged, refresh);
    QObject::connect(lon, &QDoubleSpinBox::valueChanged, refresh);
    fillList();
    if (events.empty()) {
        newEvent();
    }
}

void EventsViewer::fillList(const std::string& select) {
    list->blockSignals(true);
    list->clear();
    std::sort(events.begin(), events.end(), [] (const HistoryEvent& a, const HistoryEvent& b) { return a.start > b.start; });
    int row = events.empty() ? -1 : 0;
    for (size_t i = 0; i < events.size(); i++) {
        const auto& e = events[i];
        list->addItem(QString::fromStdString(e.shownName()) + "\n" + e.start.toUTC().toString("yyyy-MM-dd") + (e.end.date() != e.start.date() ? " to " + e.end.toUTC().toString("yyyy-MM-dd") : QString{}));
        if (e.id == select) {
            row = static_cast<int>(i);
        }
    }
    list->blockSignals(false);
    if (row >= 0) {
        list->setCurrentRow(row);
        showEvent(static_cast<size_t>(row));
    }
}

void EventsViewer::showEvent(size_t index) {
    if (index >= events.size()) {
        return;
    }
    current = static_cast<int>(index);
    const auto& e = events[index];
    name->setText(QString::fromStdString(e.name));
    start->setDateTime(e.start.toUTC());
    end->setDateTime(e.end.toUTC());
    lat->setValue(e.lat);
    lon->setValue(e.lon);
    radius->setValue(e.radiusKm);
    note->setPlainText(QString::fromStdString(e.note));
    moments->setPlainText(momentsText(e));
    fillAvailability(e);
}

void EventsViewer::readForm(HistoryEvent& e) const {
    if (current >= 0 && current < static_cast<int>(events.size())) {
        e = events[static_cast<size_t>(current)];
    }
    e.name = name->text().trimmed().toStdString();
    e.start = start->dateTime().toUTC();
    e.end = end->dateTime().toUTC();
    if (e.end < e.start) {
        e.end = e.start;
    }
    e.lat = lat->value();
    e.lon = lon->value();
    e.radiusKm = radius->value();
    e.note = note->toPlainText().toStdString();
    e.moments = parseMoments(moments->toPlainText());
}

void EventsViewer::newEvent() {
    current = -1;
    name->clear();
    const auto day = QDateTime::currentDateTimeUtc().addDays(-1);
    start->setDateTime(day);
    end->setDateTime(day);
    note->clear();
    moments->clear();
    // the default point: the current saved location, when one is known
    lat->setValue(Utility::readPrefInt("HISTORY_LAT_X1000", 37240) / 1000.0);
    lon->setValue(Utility::readPrefInt("HISTORY_LON_X1000", -93390) / 1000.0);
    list->blockSignals(true);
    list->clearSelection();
    list->setCurrentRow(-1);
    list->blockSignals(false);
    HistoryEvent e;
    readForm(e);
    fillAvailability(e);
}

void EventsViewer::saveEvent() {
    HistoryEvent e;
    readForm(e);
    if (current < 0 || current >= static_cast<int>(events.size())) {
        e.id = QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz").toStdString();
        e.source = "user";
        events.push_back(e);
    } else {
        events[static_cast<size_t>(current)] = e;
    }
    Utility::writePrefInt("HISTORY_LAT_X1000", static_cast<int>(std::lround(e.lat * 1000.0)));
    Utility::writePrefInt("HISTORY_LON_X1000", static_cast<int>(std::lround(e.lon * 1000.0)));
    HistoryEvents::save(events);
    fillList(e.id);
}

void EventsViewer::deleteEvent() {
    if (current < 0 || current >= static_cast<int>(events.size())) {
        return;
    }
    events.erase(events.begin() + current);
    current = -1;
    HistoryEvents::save(events);
    fillList();
    if (events.empty()) {
        newEvent();
    }
}

void EventsViewer::pickPoint() {
    new PointPicker{this, lat->value(), lon->value(), true, [this] (double pickedLat, double pickedLon) {
        lat->setValue(pickedLat);
        lon->setValue(pickedLon);
    }};
}

// what each kind of data has for the event's time: from when it is open upstream, and whether the event is inside that; with the screen that opens it, where there is one
void EventsViewer::fillAvailability(const HistoryEvent& e) {
    struct Row {
        QString data;
        QDate from;          // invalid: "now only" or unknown
        QString fromText;
        std::function<void()> open;
        QString planned;     // non-empty: no screen opens it at a time yet
    };
    const QDate day = e.start.toUTC().date();
    const auto openStormReports = [this, day] {
        new SpcStormReports{this, day.toString("yyMMdd").toStdString()};
    };
    const auto openSatellite = [this, day] {
        const int back = static_cast<int>(day.daysTo(QDateTime::currentDateTimeUtc().date()));
        Utility::writePrefInt("MM_GIBS_DAYS", std::clamp(back, 0, 4000));
        Utility::writePrefInt("MM_GIBS_PRODUCT", 3);   // Terra MODIS true color: the longest record
        new MasterMapViewer{this, "Polar orbiters"};
    };
    const auto openUrl = [] (const QString& url) { return [url] { QDesktopServices::openUrl(QUrl{url}); }; };
    const QString ymd = day.toString("yyyyMMdd");
    const std::vector<Row> rows{
        {"SPC storm reports (map and list for the day)", QDate{2004, 1, 1}, "about 2004", openStormReports, {}},
        {"SPC convective outlooks (archive page, 16:30Z day 1)", QDate{2010, 1, 1}, "2010 checked", openUrl("https://www.spc.noaa.gov/products/outlook/archive/" + day.toString("yyyy") + "/day1otlk_" + ymd + "_1630.html"), {}},
        {"Satellite: daily true color (Terra MODIS from 2000, NOAA-20 from 2018)", QDate{2000, 2, 24}, "2000-02-24", openSatellite, {}},
        {"Radar: MRMS composite (AWS)", QDate{2020, 10, 14}, "2020-10-14", nullptr, "its screen cannot open at a time yet"},
        {"Model runs: HRRR (AWS)", QDate{2014, 7, 30}, "2014-07-30", nullptr, "its screen cannot open at a time yet"},
        {"Model runs: GFS (AWS)", QDate{2021, 1, 1}, "2021", nullptr, "its screen cannot open at a time yet"},
        {"Observed soundings (SPC)", QDate{1998, 1, 1}, "recent archive; check", nullptr, "its screen cannot open at a time yet"},
        {"Surface obs and warnings (Iowa Mesonet)", QDate{1990, 1, 1}, "varies", openUrl("https://mesonet.agron.iastate.edu/"), {}},
        {"Daily climate: snow, rain, temperature (ACIS / xmACIS)", QDate{1940, 1, 1}, "station dependent (Springfield 1940)", openUrl("https://xmacis.rcc-acis.org/"), {}},
    };
    table->setRowCount(static_cast<int>(rows.size()));
    for (size_t i = 0; i < rows.size(); i++) {
        const auto& r = rows[i];
        table->setItem(static_cast<int>(i), 0, new QTableWidgetItem{r.data});
        table->setItem(static_cast<int>(i), 1, new QTableWidgetItem{r.fromText});
        QString status;
        if (!e.start.isValid()) {
            status = "";
        } else if (r.from.isValid() && day < r.from) {
            status = "before this archive starts";
        } else if (!r.planned.isEmpty()) {
            status = "in the open archive";
            table->item(static_cast<int>(i), 0)->setToolTip(r.planned);
        } else {
            status = "in the open archive";
        }
        table->setItem(static_cast<int>(i), 2, new QTableWidgetItem{status});
        if (r.open) {
            auto * button = new QPushButton{"Open", table};
            const auto open = r.open;
            QObject::connect(button, &QPushButton::clicked, [open] { open(); });
            table->setCellWidget(static_cast<int>(i), 3, button);
        } else {
            table->setCellWidget(static_cast<int>(i), 3, nullptr);
        }
    }
}
