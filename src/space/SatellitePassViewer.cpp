// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "space/SatellitePassViewer.h"
#include <algorithm>
#include <memory>
#include <QDateTime>
#include <QTimeZone>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include "mapkit/MasterMapViewer.h"
#include "util/Utility.h"
#include <map>
#include "objects/FutureVoid.h"

namespace {
    QString inText(long long seconds) {
        const bool past = seconds < 0;
        const long long minutes = std::llabs(seconds) / 60;
        const QString text = minutes < 60 ? QString{"%1 min"}.arg(minutes) : minutes < 48 * 60 ? QString{"%1 h %2 min"}.arg(minutes / 60).arg(minutes % 60, 2, 10, QChar{'0'}) : QString{"%1 days %2 h"}.arg(minutes / 1440).arg(minutes % 1440 / 60);
        return past ? text + " ago" : text;
    }

    // "Good", "Fair: pixels about 2.1 times wider", "Poor: ...", "Possible: where it could look ..."
    QString qualityText(const SatellitePasses::Quality& q) {
        using Grade = SatellitePasses::Quality::Grade;
        const QString name = q.grade == Grade::Good ? "Good" : q.grade == Grade::Fair ? "Fair" : q.grade == Grade::Poor ? "Poor" : "Possible";
        return q.summary == "Good" ? name : name + ": " + QString::fromStdString(q.summary);
    }

    QString sunText(double elevation) {
        return elevation > 0.0 ? QString{"day (sun %1°)"}.arg(std::lround(elevation)) : elevation > -12.0 ? QString{"twilight"} : QString{"night"};
    }

    QColor kindColor(SatellitePasses::Kind kind) {
        return kind == SatellitePasses::Kind::Imagery ? QColor{225, 238, 252} : kind == SatellitePasses::Kind::Microwave ? QColor{235, 225, 250} : kind == SatellitePasses::Kind::Wind ? QColor{222, 245, 228} : QColor{253, 236, 214};
    }
}

SatellitePassViewer::SatellitePassViewer(Window * parent, const std::string& place)
    : Window{parent}
    , wanted{place}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Satellite passes - storms and places of interest");
    auto * top = new QWidget{this};
    auto * layout = new QHBoxLayout{top};
    layout->setContentsMargins(0, 0, 0, 0);
    placeBox = new QComboBox{top};
    placeBox->setMinimumWidth(240);
    hoursBox = new QComboBox{top};
    for (const int h : {-168, -72, -24, -12}) {   // a look back: when each last passed (the element sets are today's, run back: the quality column says how far to trust the older ones)
        hoursBox->addItem(h <= -72 ? "Last " + QString::number(-h / 24) + " days" : "Last " + QString::number(-h) + " hours", h);
    }
    for (const int h : {12, 24, 48, 72}) {
        hoursBox->addItem(QString::number(h) + " hours ahead", h);
    }
    hoursBox->setCurrentIndex(qEnvironmentVariableIsSet("WXQT_SATSPAN") ? qEnvironmentVariableIntValue("WXQT_SATSPAN") : 5);   // (the variable is for testing)
    kindBox = new QComboBox{top};
    kindBox->addItem("Every instrument", -1);
    for (const auto kind : {SatellitePasses::Kind::Imagery, SatellitePasses::Kind::Microwave, SatellitePasses::Kind::Wind, SatellitePasses::Kind::Radar}) {
        kindBox->addItem(QString::fromStdString(SatellitePasses::kindName(kind)), static_cast<int>(kind));
    }
    satelliteBox = new QComboBox{top};
    satelliteBox->addItem("Every satellite");
    satelliteBox->addItem("EUMETSAT (Metop, Metop-SG, Sentinel-3)");
    for (const auto& s : SatellitePasses::satellites()) {
        satelliteBox->addItem(QString::fromStdString(s.name));
    }
    auto * add = new QPushButton{"Add a place...", top};
    auto * remove = new QPushButton{"Remove", top};
    auto * map = new QPushButton{"On the map", top};
    auto * open = new QPushButton{"Open the pass on the map", top};
    open->setToolTip("The master map at the time of the selected pass, on its place, with the layers that show what the instrument saw (double-click a row does the same)");
    auto * refresh = new QPushButton{"Refresh", top};
    for (auto * w : std::vector<QWidget *>{placeBox, hoursBox, kindBox, satelliteBox, add, remove, open, map, refresh}) {
        layout->addWidget(w);
    }
    layout->addStretch();
    box.addWidgetReal(top, 0, Qt::Alignment{});
    note = new QLabel{"Reading the active storms and the element sets of the satellites...", this};
    note->setWordWrap(true);
    box.addWidgetReal(note, 0, Qt::Alignment{});
    table = new QTableWidget{this};
    table->setColumnCount(10);
    table->setHorizontalHeaderLabels({"Time (UTC)", "When", "Place", "Satellite", "Instrument", "Track", "From the middle", "Quality", "Sun there", "Good for"});
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    box.addWidgetReal(table, 1, Qt::Alignment{});
    auto * credit = new QLabel{"Passes worked out here (SGP4) from the public element sets of CelesTrak, taken twice a day: the time is when the swath is nearest the place, good to a minute or so for the next day or two. "
                               "A radar (SAR) pass is where the radar could look; which ones are scheduled is on NOAA STAR's SAR winds page. Quality is a guide from the geometry and the age of the element set (hover a row for the numbers): "
                               "far from the middle of a scanning swath the pixels stretch, and an old element set puts the satellite further from where this says.", this};
    credit->setWordWrap(true);
    credit->setStyleSheet("color: gray;");
    box.addWidgetReal(credit, 0, Qt::Alignment{});
    box.getAndShow(this);
    resize(1150, 700);
    for (auto * c : {placeBox, hoursBox, kindBox, satelliteBox}) {
        QObject::connect(c, &QComboBox::currentIndexChanged, this, [this] (int) { recompute(); });
    }
    QObject::connect(add, &QPushButton::clicked, this, [this] { addPlace(); });
    QObject::connect(remove, &QPushButton::clicked, this, [this] { removePlace(); });
    QObject::connect(map, &QPushButton::clicked, this, [this] { openMap(); });
    QObject::connect(open, &QPushButton::clicked, this, [this] { openPassOnMap(table->currentRow()); });
    QObject::connect(table, &QTableWidget::cellDoubleClicked, this, [this] (int row, int) { openPassOnMap(row); });
    QObject::connect(refresh, &QPushButton::clicked, this, [this] { loadPlaces(); });
    loadPlaces();
}

void SatellitePassViewer::loadPlaces() {
    auto found = std::make_shared<std::vector<SatellitePasses::Place>>();
    note->setText("Reading the active storms...");
    new FutureVoid{this,
        [found] { *found = SatellitePasses::stormPlaces(); },
        [this, found] {
            if (closed) {
                return;
            }
            storms = *found;
            mine = SatellitePasses::places();
            fillPlaceBox();
        }};
}

void SatellitePassViewer::fillPlaceBox() {
    placeBox->blockSignals(true);
    const auto before = placeBox->currentIndex() > 0 ? placeBox->currentText() : QString::fromStdString(wanted);
    placeBox->clear();
    placeBox->addItem("Every storm and place", "all");
    placeBox->addItem("Every storm", "storms");
    placeBox->addItem("Every place of mine", "places");
    for (const auto& s : storms) {
        placeBox->addItem(QString::fromStdString("Storm: " + s.name), QString::fromStdString("s:" + s.name));
    }
    for (const auto& p : mine) {
        placeBox->addItem(QString::fromStdString("Place: " + p.name), QString::fromStdString("p:" + p.name));
    }
    int select = 0;
    for (int i = 0; i < placeBox->count(); i++) {
        if (!before.isEmpty() && (placeBox->itemText(i) == before || placeBox->itemText(i).contains(before, Qt::CaseInsensitive)) && i > 2) {
            select = i;
            break;
        }
    }
    if (select == 0 && wanted.empty() && !storms.empty()) {
        select = 1;   // the storms, to begin with
    }
    placeBox->setCurrentIndex(select);
    placeBox->blockSignals(false);
    wanted.clear();
    recompute();
}

void SatellitePassViewer::recompute() {
    const int mineGeneration = ++generation;
    const auto choice = placeBox->currentData().toString().toStdString();
    std::vector<SatellitePasses::Place> at;
    if (choice == "all" || choice == "storms") {
        at.insert(at.end(), storms.begin(), storms.end());
    }
    if (choice == "all" || choice == "places") {
        at.insert(at.end(), mine.begin(), mine.end());
    }
    if (choice.compare(0, 2, "s:") == 0) {
        for (const auto& s : storms) {
            if (s.name == choice.substr(2)) at.push_back(s);
        }
    } else if (choice.compare(0, 2, "p:") == 0) {
        for (const auto& p : mine) {
            if (p.name == choice.substr(2)) at.push_back(p);
        }
    }
    std::vector<std::string> sats;
    if (satelliteBox->currentIndex() == 1) {
        sats = {"Metop-B", "Metop-C", "Metop-SG A1", "Sentinel-3A", "Sentinel-3B", "Sentinel-3C"};
    } else if (satelliteBox->currentIndex() > 1) {
        sats.push_back(satelliteBox->currentText().toStdString());
    }
    std::vector<SatellitePasses::Kind> kinds;
    if (kindBox->currentData().toInt() >= 0) {
        kinds.push_back(static_cast<SatellitePasses::Kind>(kindBox->currentData().toInt()));
    }
    const double span = hoursBox->currentData().toDouble();   // negative: back from now
    const double hours = std::fabs(span);
    if (at.empty()) {
        table->setRowCount(0);
        note->setText("No place to make the passes for: there is no active storm, or no saved location. Add a place.");
        return;
    }
    note->setText("Working out the passes...");
    auto rows = std::make_shared<std::vector<SatellitePasses::Row>>();
    auto problem = std::make_shared<std::string>();
    const long long now = QDateTime::currentSecsSinceEpoch();
    const long long from = span < 0 ? now - static_cast<long long>(hours * 3600.0) : now;
    new FutureVoid{this,
        [=] { *rows = SatellitePasses::passList(at, sats, kinds, hours, from, *problem); if (span < 0) { std::reverse(rows->begin(), rows->end()); } },
        [this, rows, problem, mineGeneration, now] {
            if (closed || mineGeneration != generation) {
                return;
            }
            shown = *rows;
            table->setRowCount(static_cast<int>(rows->size()));
            for (int r = 0; r < static_cast<int>(rows->size()); r++) {
                const auto& row = (*rows)[static_cast<size_t>(r)];
                const auto when = QDateTime::fromSecsSinceEpoch(row.pass.time, QTimeZone::utc());
                QString instrumentNote;
                for (const auto& s : SatellitePasses::satellites()) {
                    if (s.name == row.satellite) {
                        for (const auto& i : s.instruments) {
                            if (i.label == row.instrument) instrumentNote = QString::fromStdString(i.note);
                        }
                    }
                }
                const double off = row.pass.distanceKm;
                const QStringList cells{when.toString("ddd MMM d  HH:mm") + "Z", inText(row.pass.time - now), QString::fromStdString(row.place), QString::fromStdString(row.satellite), QString::fromStdString(row.instrument),
                                        row.pass.ascending ? "north-bound" : "south-bound", QString{"%1 km %2"}.arg(std::lround(std::fabs(off))).arg(off < 0 ? "left" : "right"), qualityText(row.quality), sunText(row.pass.sunElevation), instrumentNote};
                for (int c = 0; c < cells.size(); c++) {
                    auto * item = new QTableWidgetItem{cells[c]};
                    item->setBackground(kindColor(row.kind));
                    item->setToolTip(QString::fromStdString(row.quality.detail).trimmed());
                    if (c == 7) {   // the quality, in its own color
                        using Grade = SatellitePasses::Quality::Grade;
                        item->setForeground(row.quality.grade == Grade::Good ? QColor{20, 120, 40} : row.quality.grade == Grade::Fair ? QColor{170, 100, 0} : row.quality.grade == Grade::Poor ? QColor{180, 30, 30} : QColor{70, 70, 150});
                    }
                    if (row.storm) {
                        auto font = item->font();
                        font.setBold(c == 2);
                        item->setFont(font);
                    }
                    table->setItem(r, c, item);
                }
            }
            table->resizeColumnsToContents();
            if (qEnvironmentVariableIsSet("WXQT_SATOPEN")) {   // (for testing: open that row on the map once)
                const int row = qEnvironmentVariableIntValue("WXQT_SATOPEN");
                qunsetenv("WXQT_SATOPEN");
                openPassOnMap(row);
            }
            note->setText(QString{"%1 passes."}.arg(rows->size()) + (problem->empty() ? QString{} : "  Not read: " + QString::fromStdString(*problem)));
        }};
}

void SatellitePassViewer::addPlace() {
    QDialog dialog{this};
    dialog.setWindowTitle("Add a place");
    auto * form = new QFormLayout{&dialog};
    auto * name = new QLineEdit{&dialog};
    auto * lat = new QDoubleSpinBox{&dialog};
    lat->setRange(-90.0, 90.0);
    lat->setDecimals(3);
    auto * lon = new QDoubleSpinBox{&dialog};
    lon->setRange(-180.0, 180.0);
    lon->setDecimals(3);
    form->addRow("Name", name);
    form->addRow("Latitude (north +)", lat);
    form->addRow("Longitude (east +)", lon);
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    form->addRow(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) {
        return;
    }
    auto own = SatellitePasses::own();
    own.push_back({name->text().trimmed().toStdString(), lat->value(), lon->value(), false, {}});
    SatellitePasses::savePlaces(own);
    wanted = name->text().trimmed().toStdString();
    mine = SatellitePasses::places();
    fillPlaceBox();
}

void SatellitePassViewer::removePlace() {
    const auto choice = placeBox->currentData().toString().toStdString();
    if (choice.compare(0, 2, "p:") != 0) {
        QMessageBox::information(this, "Remove", "Choose one of your own places in the list first (the saved locations of the program are changed in its settings).");
        return;
    }
    auto own = SatellitePasses::own();
    const auto before = own.size();
    own.erase(std::remove_if(own.begin(), own.end(), [&choice] (const SatellitePasses::Place& p) { return p.name == choice.substr(2); }), own.end());
    if (own.size() == before) {
        QMessageBox::information(this, "Remove", "That place is a saved location of the program: it is changed in the program's settings.");
        return;
    }
    SatellitePasses::savePlaces(own);
    mine = SatellitePasses::places();
    fillPlaceBox();
}

void SatellitePassViewer::openMap() {
    const auto choice = placeBox->currentData().toString().toStdString();
    double lat = 20.0, lon = 140.0;
    if (choice.compare(0, 2, "s:") == 0) {
        for (const auto& s : storms) if (s.name == choice.substr(2)) { lat = s.lat; lon = s.lon; }
    } else if (choice.compare(0, 2, "p:") == 0) {
        for (const auto& p : mine) if (p.name == choice.substr(2)) { lat = p.lat; lon = p.lon; }
    } else if (!storms.empty()) {
        lat = storms.front().lat;
        lon = storms.front().lon;
    }
    auto * viewer = new MasterMapViewer{this};
    viewer->openAt(QDateTime::currentDateTimeUtc(), lat, lon, 1500.0, {"satellite/passes", "tropical/world-storms"});
}

// The map at the pass: the daily picture of the satellite itself where NASA has one (the day of the pass, and the time of day the pass was is in the stamp on the map), the radar winds for a SAR; the rest (the
// scatterometer, the microwave imagers, AVHRR) have no picture of their own here, so Terra's true color of the day, and the swaths of the satellites with the pass layer for one still to come
void SatellitePassViewer::openPassOnMap(int index) {
    if (index < 0 || index >= static_cast<int>(shown.size())) {
        return;
    }
    const auto& row = shown[static_cast<size_t>(index)];
    const auto when = QDateTime::fromSecsSinceEpoch(row.pass.time, QTimeZone::utc());
    const bool future = row.pass.time > QDateTime::currentSecsSinceEpoch();
    vector<string> layers;
    if (row.kind == SatellitePasses::Kind::Radar) {
        layers.push_back("satellite/sar-winds");
    } else {
        const std::map<string, int> picture{{"NOAA-20", 0}, {"NOAA-21", 1}, {"Suomi NPP", 2}, {"Terra", 3}, {"Aqua", 4}};
        const auto found = picture.find(row.satellite);
        Utility::writePrefInt("MM_GIBS_PRODUCT", found != picture.end() ? found->second : 3);
        layers.push_back("satellite/polar");
    }
    if (future) {
        layers.push_back("satellite/passes");
    }
    auto * viewer = new MasterMapViewer{this};
    viewer->openAt(when, row.lat, row.lon, row.kind == SatellitePasses::Kind::Radar ? 600.0 : 900.0, layers);
}
