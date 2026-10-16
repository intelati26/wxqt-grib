// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "history/EventsViewer.h"
#include <algorithm>
#include <QComboBox>
#include <QPointer>
#include <QDesktopServices>
#include <QDialog>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QFormLayout>
#include <QGridLayout>
#include <QTabWidget>
#include "objects/FutureVoid.h"
#include "util/UtilityIO.h"
#include "models/ModelScreen.h"
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTimeZone>
#include <QUrl>
#include <QVBoxLayout>
#include "history/CatalogDialog.h"
#include "history/CatalogSources.h"
#include "history/HistoryCursor.h"
#include "history/PointPicker.h"
#include "mapkit/MasterMapViewer.h"
#include "spc/SpcStormReports.h"
#include "tornado/TornadoData.h"
#include "tornado/UtilityOutbreaks.h"
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
    auto * catalogButton = new QPushButton{"From the catalog...", this};
    catalogButton->setToolTip("Take an event from a list: tornado outbreaks since 1950 (more kinds later)");
    auto * left = new QVBoxLayout;
    left->addWidget(new QLabel{"Events", this});
    left->addWidget(list, 1);
    left->addWidget(newButton);
    left->addWidget(catalogButton);

    name = new QLineEdit{this};
    name->setPlaceholderText("(left empty: the date and position)");
    start = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addDays(-1), this};
    end = new QDateTimeEdit{QDateTime::currentDateTimeUtc().addDays(-1), this};
    for (auto * edit : {start, end}) {
        edit->setDisplayFormat("yyyy-MM-dd HH:mm");
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        edit->setTimeZone(QTimeZone::utc());
#else
        edit->setTimeSpec(Qt::UTC);   // (the Linux build's Qt 6.4 has no setTimeZone on the date box)
#endif
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

    atBox = new QComboBox{this};
    atBox->setToolTip("The time the screens open at");
    auto * atRow = new QHBoxLayout;
    atRow->addWidget(new QLabel{"Open the screens at", this});
    atRow->addWidget(atBox, 1);
    tabs = new QTabWidget{this};
    auto * dataPage = new QWidget{this};
    auto * dataColumn = new QVBoxLayout{dataPage};
    dataColumn->setContentsMargins(0, 0, 0, 0);
    dataColumn->addLayout(atRow);
    dataColumn->addWidget(table, 1);
    tabs->addTab(dataPage, "What there is for this time and place");
    // the SPC outlook, added to every event on its own: the categorical and the tornado, wind and hail chances of the event's convective day
    auto * outlookPage = new QWidget{this};
    auto * outlookColumn = new QVBoxLayout{outlookPage};
    issuance = new QComboBox{this};
    issuance->addItems({"16:30Z (the afternoon update)", "13:00Z", "06:00Z (the morning)", "20:00Z", "01:00Z (the evening, next day)"});
    outlookNote = new QLabel{this};
    outlookColumn->addWidget(issuance);
    outlookColumn->addWidget(outlookNote);
    auto * grid = new QGridLayout;
    static const char * titles[4] = {"Categorical", "Tornado", "Wind", "Hail"};
    for (int i = 0; i < 4; i++) {
        auto * image = new QLabel{this};
        image->setAlignment(Qt::AlignCenter);
        image->setMinimumSize(300, 220);
        image->setToolTip(titles[i]);
        outlookImages.push_back(image);
        grid->addWidget(new QLabel{QString{"<b>%1</b>"}.arg(titles[i]), this}, (i / 2) * 2, i % 2);
        grid->addWidget(image, (i / 2) * 2 + 1, i % 2);
    }
    outlookColumn->addLayout(grid, 1);
    tabs->addTab(outlookPage, "SPC outlook (added automatically)");
    // the weather map of the day: WPC's surface analysis archive from 2006 (every 3 hours), and where the scanned maps before it are
    auto * surfacePage = new QWidget{this};
    auto * surfaceColumn = new QVBoxLayout{surfacePage};
    auto * surfaceRow = new QHBoxLayout;
    surfaceHour = new QComboBox{this};
    for (int h = 0; h < 24; h += 3) {
        surfaceHour->addItem(QString{"%1Z"}.arg(h, 2, 10, QChar{'0'}), h);
    }
    surfaceKind = new QComboBox{this};
    surfaceKind->addItem("Surface analysis, North America", "namussfc");
    surfaceKind->addItem("Fronts and pressure, United States", "usfntsfc");
    surfaceKind->addItem("With the satellite picture", "ussatsfc");
    surfaceKind->addItem("With the radar", "radsfcus_exp");
    auto * scanned = new QPushButton{"Scanned Daily Weather Map (1871-2002)...", this};
    scanned->setToolTip("NOAA Central Library: the printed Daily Weather Map of the day, by date (daily to April 1968, weekly from May 1968)");
    auto * reanalysis = new QPushButton{"20th Century Reanalysis (1836-2015)...", this};
    reanalysis->setToolTip("NOAA PSL: weather maps for any date back to 1836, made from surface pressure observations");
    surfaceRow->addWidget(new QLabel{"Time", this});
    surfaceRow->addWidget(surfaceHour);
    surfaceRow->addWidget(surfaceKind, 1);
    surfaceRow->addWidget(scanned);
    surfaceRow->addWidget(reanalysis);
    surfaceNote = new QLabel{this};
    surfaceNote->setWordWrap(true);
    surfaceImage = new QLabel{this};
    surfaceImage->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto * surfaceScroll = new QScrollArea{this};
    surfaceScroll->setWidget(surfaceImage);
    surfaceScroll->setWidgetResizable(true);
    surfaceColumn->addLayout(surfaceRow);
    surfaceColumn->addWidget(surfaceNote);
    surfaceColumn->addWidget(surfaceScroll, 1);
    tabs->addTab(surfacePage, "Weather map of the day");
    QObject::connect(scanned, &QPushButton::clicked, [] { QDesktopServices::openUrl(QUrl{"https://library.noaa.gov/weather-climate/daily-weather-maps"}); });
    QObject::connect(reanalysis, &QPushButton::clicked, [] { QDesktopServices::openUrl(QUrl{"https://psl.noaa.gov/data/20thC_Rean/"}); });
    QObject::connect(surfaceHour, &QComboBox::currentIndexChanged, [this] { loadSurface(); });
    QObject::connect(surfaceKind, &QComboBox::currentIndexChanged, [this] { loadSurface(); });
    QObject::connect(issuance, &QComboBox::currentIndexChanged, [this] { loadOutlooks(); });
    QObject::connect(tabs, &QTabWidget::currentChanged, [this] (int) { loadOutlooks(); syncSurface(); loadSurface(); });
    auto * right = new QVBoxLayout;
    right->addLayout(form);
    right->addLayout(buttons);
    right->addWidget(tabs, 1);
    auto * all = new QHBoxLayout;
    all->addLayout(left);
    all->addLayout(right, 1);
    auto * central = new QWidget{this};
    central->setLayout(all);
    setCentralWidget(central);
    setSize(1000, 720);
    show();

    QObject::connect(newButton, &QPushButton::clicked, [this] { newEvent(); });
    QObject::connect(catalogButton, &QPushButton::clicked, [this] { openCatalog(); });
    QObject::connect(save, &QPushButton::clicked, [this] { saveEvent(); });
    QObject::connect(remove, &QPushButton::clicked, [this] { deleteEvent(); });
    QObject::connect(pick, &QPushButton::clicked, [this] { pickPoint(); });
    QObject::connect(list, &QListWidget::currentRowChanged, [this] (int row) { if (row >= 0) { showEvent(static_cast<size_t>(row)); } });
    const auto refresh = [this] {
        HistoryEvent e;
        readForm(e);
        fillAvailability(e);
        outlookTimer.start(700);
    };
    QObject::connect(start, &QDateTimeEdit::dateTimeChanged, refresh);
    QObject::connect(lat, &QDoubleSpinBox::valueChanged, refresh);
    QObject::connect(lon, &QDoubleSpinBox::valueChanged, refresh);
    outlookTimer.setSingleShot(true);
    QObject::connect(&outlookTimer, &QTimer::timeout, [this] { loadOutlooks(); syncSurface(); loadSurface(); });
    QObject::connect(atBox, &QComboBox::currentIndexChanged, [this] (int) {
        HistoryEvent e;
        readForm(e);
        fillAvailability(e);
        HistoryCursor::get().set(atTime(), QString::fromStdString(e.shownName()), this);   // the open screens follow the time the page opens at
    });
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
    outlookTimer.start(200);
    HistoryCursor::get().set(atTime(), QString::fromStdString(e.shownName()), this);   // the screens already open from an event go to this one's time
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

// The catalogs (history/CatalogSources): tornado outbreaks, the biggest days at the page's place, tropical storms. The entry chosen is added as an event (or the event of that catalog id that is
// there is selected).
void EventsViewer::openCatalog() {
    new CatalogDialog{this, CatalogSources::all(), lat->value(), lon->value(), [this] (const HistoryEvent& made) {
        const auto have = std::find_if(events.begin(), events.end(), [&made] (const HistoryEvent& e) { return !made.catalogId.empty() && e.catalogId == made.catalogId; });
        std::string select;
        if (have != events.end()) {
            select = have->id;
        } else {
            HistoryEvent e = made;
            e.id = QDateTime::currentDateTimeUtc().toString("yyyyMMddHHmmsszzz").toStdString();
            events.push_back(e);
            HistoryEvents::save(events);
            select = e.id;
        }
        fillList(select);
    }};
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
QDateTime EventsViewer::atTime() const {
    const int index = atBox->currentIndex();
    HistoryEvent e;
    readForm(e);
    if (index <= 0) {
        return e.start;
    }
    if (index - 1 < static_cast<int>(e.moments.size())) {
        return e.moments[static_cast<size_t>(index - 1)].time;
    }
    return e.end;
}

void EventsViewer::fillAtBox(const HistoryEvent& e) {
    const int keep = atBox->currentIndex();
    atBox->blockSignals(true);
    atBox->clear();
    atBox->addItem("the start  " + e.start.toUTC().toString("yyyy-MM-dd HH:mm") + "Z");
    for (const auto& m : e.moments) {
        atBox->addItem(m.time.toUTC().toString("yyyy-MM-dd HH:mm") + "Z  " + QString::fromStdString(m.label));
    }
    atBox->addItem("the end  " + e.end.toUTC().toString("yyyy-MM-dd HH:mm") + "Z");
    atBox->setCurrentIndex(std::clamp(keep, 0, atBox->count() - 1));
    atBox->blockSignals(false);
}

// the convective day's outlook (12Z to 12Z) of the time the event starts: the categorical map and the three chances, from SPC's archive, shown on their own
// the day and the 3 hour box of the weather map follow the time the screens open at (called when the event or that time changes, not when the hour is chosen by hand)
void EventsViewer::syncSurface() {
    const auto at = atTime().toUTC();
    const int rounded = (at.time().hour() * 60 + at.time().minute() + 90) / 180 * 3;   // the nearest 3 hours
    surfaceDay = rounded >= 24 ? at.date().addDays(1) : at.date();
    surfaceHour->blockSignals(true);
    surfaceHour->setCurrentIndex((rounded % 24) / 3);
    surfaceHour->blockSignals(false);
}

void EventsViewer::loadSurface() {
    if (tabs == nullptr || tabs->currentIndex() != 2) {
        return;   // only when its tab is on show
    }
    HistoryEvent e;
    readForm(e);
    if (!e.start.isValid()) {
        return;
    }
    const QDate day = surfaceDay.isValid() ? surfaceDay : atTime().toUTC().date();
    const QDate firstDay{2006, 3, 29};
    const int hour = surfaceHour->currentData().toInt();
    if (day < firstDay) {
        surfaceImage->clear();
        surfaceNote->setText("WPC's surface analysis archive starts on 2006-03-29. Before that: the scanned Daily Weather Map (NOAA Central Library, 1871 to 2002: search the library page for the date as " +
                             day.toString("yyyyMMdd") + ") or the 20th Century Reanalysis (maps of any date back to 1836, made from surface pressure observations).");
        return;
    }
    const QString kind = surfaceKind->currentData().toString();
    const QString url = QString{"https://www.wpc.ncep.noaa.gov/archives/sfc/%1/%2%3%4.gif"}.arg(day.toString("yyyy"), kind, day.toString("yyyyMMdd"), QString{"%1"}.arg(hour, 2, 10, QChar{'0'}));
    surfaceNote->setText(QString{"WPC surface analysis archive, %1 %2Z (every 3 hours from 2006-03-29 18Z)."}.arg(day.toString("yyyy-MM-dd")).arg(hour, 2, 10, QChar{'0'}));
    const int mine = ++surfaceGeneration;
    auto bytes = std::make_shared<QByteArray>();
    new FutureVoid{this, [bytes, url] { *bytes = UtilityIO::downloadAsByteArray(url.toStdString()); }, [this, bytes, mine] {
        if (mine != surfaceGeneration) {
            return;
        }
        QPixmap pixmap;
        if (!bytes->isEmpty() && pixmap.loadFromData(*bytes)) {
            surfaceImage->setPixmap(pixmap);
        } else {
            surfaceImage->setText("not in the archive for this time and kind of map");
        }
    }};
}

void EventsViewer::loadOutlooks() {
    HistoryEvent e;
    readForm(e);
    if (!e.start.isValid()) {
        return;
    }
    const QDate day = e.start.toUTC().addSecs(-12 * 3600).date();   // the convective day: 12Z to 12Z
    static const char * times[5] = {"1630", "1300", "0600", "2000", "0100"};
    const int pick = std::clamp(issuance->currentIndex(), 0, 4);
    const QDate issued = pick == 4 ? day.addDays(1) : day;
    const QString stem = QString{"https://www.spc.noaa.gov/products/outlook/archive/%1/%2_%3_%4"}.arg(issued.toString("yyyy"), "%1", issued.toString("yyyyMMdd"), times[pick]);
    // the archive's files are *_prt.png for the recent years and *.gif for the older ones: the names are tried in turn
    const QString names[4] = {stem.arg("day1otlk"), stem.arg("day1probotlk") + "_torn", stem.arg("day1probotlk") + "_wind", stem.arg("day1probotlk") + "_hail"};
    outlookNote->setText("Convective day " + day.toString("yyyy-MM-dd") + " (12Z to 12Z), issued " + issued.toString("yyyy-MM-dd") + " " + QString{times[pick]}.insert(2, ':') + "Z. SPC archive.");
    const int mine = ++outlookGeneration;
    auto bytes = std::make_shared<std::vector<QByteArray>>(4);
    new FutureVoid{this,
        [bytes, names] {
            static const char * suffixes[4] = {"_prt.png", ".gif", "_prt.gif", ".png"};
            for (int i = 0; i < 4; i++) {
                for (const auto * suffix : suffixes) {
                    QPixmap probe;
                    const auto data = UtilityIO::downloadAsByteArray((names[i] + suffix).toStdString());
                    if (!data.isEmpty() && probe.loadFromData(data)) {
                        (*bytes)[static_cast<size_t>(i)] = data;
                        break;
                    }
                }
            }
        },
        [this, bytes, mine] {
            if (mine != outlookGeneration) {
                return;
            }
            for (size_t i = 0; i < 4; i++) {
                QPixmap pixmap;
                if (!(*bytes)[i].isEmpty() && pixmap.loadFromData((*bytes)[i])) {
                    outlookImages[i]->setPixmap(pixmap.scaledToWidth(std::max(300, outlookImages[i]->width() - 8), Qt::SmoothTransformation));
                } else {
                    outlookImages[i]->setText("not in the archive for this day / issuance");
                }
            }
        }};
}

void EventsViewer::fillAvailability(const HistoryEvent& e) {
    fillAtBox(e);
    struct Row {
        QString data;
        QDate from;          // invalid: "now only" or unknown
        QString fromText;
        std::function<void()> open;
        QString planned;     // non-empty: no screen opens it at a time yet
    };
    const QDateTime at = atTime();
    const QDate day = at.toUTC().date();
    const double eventLat = e.lat, eventLon = e.lon, eventRadius = e.radiusKm;
    const auto openStormReports = [this, day] {
        new SpcStormReports{this, day.toString("yyMMdd").toStdString()};
    };
    const auto openRadar = [this, at, eventLat, eventLon, eventRadius] {
        auto * map = new MasterMapViewer{this};
        map->openAt(at, eventLat, eventLon, eventRadius, {"radar/mrms"});
    };
    const auto openGoes = [this, at, eventLat, eventLon, eventRadius] {
        auto * map = new MasterMapViewer{this};
        map->openAt(at, eventLat, eventLon, std::max(eventRadius, 400.0), {"satellite/goes-history"});
    };
    const auto openCones = [this, at, eventLat, eventLon, eventRadius] {
        auto * map = new MasterMapViewer{this};
        map->openAt(at, eventLat, eventLon, std::max(eventRadius, 900.0), {"tropical/cone-history", "tropical/outlook-history"});
    };
    const auto openClimate = [this, at, eventLat, eventLon, eventRadius] {
        auto * map = new MasterMapViewer{this};
        map->openAt(at, eventLat, eventLon, std::max(eventRadius, 1500.0), {"climate/acis-grid"});
    };
    const auto openModel = [this, at] {
        auto * screen = new ModelScreen{this, "NCEP"};
        screen->openAt("GFS", at);
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
        {"Satellite: GOES infrared / visible / water vapor, 4 km CONUS, every 15 min (Iowa Mesonet)", QDate{2010, 1, 1}, "about 2010", openGoes, {}},
        {"Tropical: NHC cones advisory by advisory against the track taken, and the development areas of the outlooks (NHC archive)", QDate{2008, 1, 1}, "2008 (cones)", openCones, {}},
        {"Climate: how far from normal the days before were (ACIS stations, temperature or rain; 1991-2020 normals)", QDate{1950, 1, 1}, "about 1950 (stations)", openClimate, {}},
        {"Radar: MRMS composite reflectivity (AWS), map at the event", QDate{2020, 10, 14}, "2020-10-14", openRadar, {}},
        {"Model: GFS run and hour of that time (AWS, from 2021)", QDate{2021, 1, 1}, "2021", openModel, {}},
        {"Model: HRRR, NAM and the others (pictures from NOMADS: the last days only)", QDate{2026, 10, 1}, "recent only", nullptr, "its screen cannot open at a time yet"},
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
