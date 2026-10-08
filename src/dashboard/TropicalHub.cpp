// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dashboard/TropicalHub.h"
#include "ui/ActivityLabel.h"
#include <algorithm>
#include <cmath>
#include <QDateTime>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QPointer>
#include <QLocale>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include "climate/ClimateViewer.h"
#include "hurricane/AceViewer.h"
#include "hurricane/AdvisoryViewer.h"
#include "hurricane/FloaterViewer.h"
#include "hurricane/ReconViewer.h"
#include "hurricane/HistoryViewer.h"
#include "hurricane/HurricaneViewer.h"
#include "hurricane/PodViewer.h"
#include "hurricane/SeasonViewer.h"
#include "hurricane/StrikeReport.h"
#include "hurricane/UtilityAtcf.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "tropical/TropicalViewer.h"
#include "util/UtilityList.h"

namespace {
    QColor classColor(int wind) {
        static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 214, 208}, QColor{200, 200, 130}, QColor{230, 200, 70},
                                        QColor{235, 160, 40}, QColor{235, 110, 20}, QColor{220, 60, 60}};
        const int index = wind < 34 ? 0 : wind < 64 ? 1 : wind < 83 ? 2 : wind < 96 ? 3 : wind < 113 ? 4 : wind < 137 ? 5 : 6;
        return colors[index];
    }

    QString className(const string& code) {
        static const std::map<string, QString> names = {{"TD", "Tropical depression"}, {"TS", "Tropical storm"}, {"HU", "Hurricane"}, {"STD", "Subtropical depression"},
            {"STS", "Subtropical storm"}, {"PTC", "Potential tropical cyclone"}, {"PC", "Post-tropical cyclone"}, {"EX", "Post-tropical cyclone"}, {"LO", "Low"},
            {"DB", "Disturbance"}, {"WV", "Wave"}, {"MH", "Major hurricane"}, {"TY", "Typhoon"}, {"SD", "Subtropical depression"}, {"SS", "Subtropical storm"}};
        const auto found = names.find(code);
        return found == names.end() ? QString::fromStdString(code) : found->second;
    }

    QString basinName(const string& basin) {
        return basin == "al" ? "Atlantic" : basin == "ep" ? "East Pacific" : "Central Pacific";
    }

    void clear(QLayout * layout) {
        while (auto * item = layout->takeAt(0)) {
            if (auto * widget = item->widget()) {
                widget->deleteLater();
            }
            if (auto * inner = item->layout()) {
                clear(inner);
            }
            delete item;
        }
    }

    QLabel * heading(const QString& text, QWidget * parent) {
        auto * label = new QLabel{"<b style='font-size:15px'>" + text + "</b>", parent};
        label->setContentsMargins(0, 8, 0, 2);
        return label;
    }

    QLabel * note(const QString& text, QWidget * parent) {
        auto * label = new QLabel{text, parent};
        label->setWordWrap(true);
        label->setEnabled(false);
        return label;
    }

    QLabel * body(const QString& text, QWidget * parent) {
        auto * label = new QLabel{text, parent};
        label->setWordWrap(true);
        label->setTextFormat(Qt::RichText);
        return label;
    }

    QString knots(int wind) {
        return QString::fromStdString(UtilityAtcf::windLabel(wind));
    }

    QString position(double lat, double lon) {
        return QString::number(std::fabs(lat), 'f', 1) + (lat < 0 ? "S " : "N ") + QString::number(std::fabs(lon), 'f', 1) + (lon < 0 ? "W" : "E");
    }

    QString updatedText(const string& iso) {   // 2026-10-08T12:00:00.000Z -> Oct 08 12:00Z
        const auto when = QDateTime::fromString(QString::fromStdString(iso), Qt::ISODate);
        return when.isValid() ? QLocale{QLocale::English}.toString(when.toUTC(), "yyyy-MM-dd HH:mm") + "Z" : QString::fromStdString(iso);
    }

    QString compass(int degrees) {
        static const char * names[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
        return names[(static_cast<int>(std::lround(degrees / 22.5)) % 16 + 16) % 16];
    }

    struct Tally {
        int named{0};
        int hurricanes{0};
        int major{0};
        double ace{0.0};
        double tike{0.0};
    };

    Tally tally(const vector<UtilitySeason::Storm>& storms) {
        Tally t;
        for (const auto& s : storms) {
            t.named += s.stormStrength ? 1 : 0;
            t.hurricanes += s.peakWind >= 64 ? 1 : 0;
            t.major += s.peakWind >= 96 ? 1 : 0;
            t.ace += s.ace;
            t.tike += s.tike;
        }
        return t;
    }
}

TropicalHub::TropicalHub(Window * parent)
    : Window{parent}
    , buttonRefresh{this, None, "Refresh"}
    , buttonTracks{this, None, "Tracks, guidance and recon..."}
    , buttonSeason{this, None, "Season charts..."}
    , buttonAce{this, None, "ACE by day..."}
    , buttonHistory{this, None, "Historical tracks..."}
    , buttonPod{this, None, "Recon plan of the day..."}
    , buttonTropical{this, None, "Tropical (CIRA, JTWC, JMA)..."}
    , buttonClimate{this, None, "Climate and ocean..."}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tropical Hub - active storms, outlook, recon and the season");
    for (auto * button : {&buttonRefresh, &buttonTracks, &buttonSeason, &buttonAce, &buttonHistory, &buttonPod, &buttonTropical, &buttonClimate}) {
        rowTop.addWidget(*button);
    }
    rowTop.addStretch();
    buttonRefresh.connect([this] { load(); });
    buttonTracks.connect([this] { new HurricaneViewer{this}; });
    buttonSeason.connect([this] {
        if (seasonAtlantic && seasonAtlantic->error.empty()) {
            new SeasonViewer{this, seasonAtlantic};
        }
    });
    buttonHistory.connect([this] { new HistoryViewer{this}; });
    buttonAce.connect([this] {
        if (seasonAtlantic && seasonPacific) {
            new AceViewer{this, seasonAtlantic, seasonPacific};
        }
    });
    buttonPod.connect([this] {
        if (pod && pod->error.empty()) {
            new PodViewer{this, pod};
        }
    });
    buttonTropical.connect([this] { new TropicalViewer{this}; });
    buttonClimate.connect([this] { new ClimateViewer{this}; });

    content = new QWidget{this};
    auto * column = new QVBoxLayout{content};
    column->setContentsMargins(4, 0, 12, 8);
    column->addWidget(heading("Active storms and invests", content));
    stormsLayout = new QVBoxLayout;
    column->addLayout(stormsLayout);
    column->addWidget(heading("Tropical Weather Outlook - areas of possible development", content));
    outlookLayout = new QVBoxLayout;
    column->addLayout(outlookLayout);
    column->addWidget(heading("Aircraft reconnaissance - Plan of the Day", content));
    podLayout = new QVBoxLayout;
    column->addLayout(podLayout);
    column->addWidget(heading("Your locations - chance of tropical storm and hurricane winds", content));
    locationsLayout = new QVBoxLayout;
    column->addLayout(locationsLayout);
    column->addWidget(heading("Season so far", content));
    seasonLayout = new QVBoxLayout;
    column->addLayout(seasonLayout);
    column->addStretch();
    auto * scroll = new QScrollArea{this};
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidgetReal(new ActivityLabel{this});
    box.addWidgetReal(scroll, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 760);
    load();
}

void TropicalHub::load() {
    const int mine = ++generation;
    textStatus.setText(string{"Loading the storm lists, outlook, recon plan and season tables..."});
    storms.clear();
    for (const auto * basin : {"al", "ep", "cp"}) {
        storms.push_back(BasinStorms{basin, {}, {}, false});
    }
    outlook.reset();
    pod.reset();
    seasonAtlantic.reset();
    seasonPacific.reset();
    wsp.reset();
    for (auto * layout : {stormsLayout, outlookLayout, podLayout, seasonLayout, locationsLayout}) {
        clear(layout);
        layout->addWidget(note("Loading...", content));
    }
    // each source on its own thread, so a slow one does not hold up the rest
    for (size_t i = 0; i < storms.size(); i++) {
        auto list = std::make_shared<vector<HurricaneData::StormEntry>>();
        auto error = std::make_shared<string>();
        const auto basin = storms[i].basin;
        new FutureVoid{this,
            [list, error, basin] { HurricaneData::loadStormList(*list, *error, basin); },
            [this, mine, i, list, error] {
                if (closed || mine != generation) {
                    return;
                }
                storms[i].entries = *list;
                storms[i].error = *error;
                storms[i].loaded = true;
                fillStorms();
            }};
    }
    auto outlookData = std::make_shared<HurricaneData::OutlookData>();
    new FutureVoid{this, [outlookData] { HurricaneData::loadOutlook(*outlookData); }, [this, mine, outlookData] {
        if (!closed && mine == generation) {
            outlook = outlookData;
            fillOutlook();
        }
    }};
    auto podData = std::make_shared<HurricaneData::PodData>();
    new FutureVoid{this, [podData] { HurricaneData::loadPod(*podData); }, [this, mine, podData] {
        if (!closed && mine == generation) {
            pod = podData;
            fillPod();
        }
    }};
    auto wspData = std::make_shared<HurricaneData::WspData>();
    new FutureVoid{this, [wspData] { HurricaneData::loadWindProbabilities(*wspData); }, [this, mine, wspData] {
        if (!closed && mine == generation) {
            wsp = wspData;
            fillLocations();
        }
    }};
    auto atlantic = std::make_shared<HurricaneData::SeasonData>();
    auto pacific = std::make_shared<HurricaneData::SeasonData>();
    new FutureVoid{this, [atlantic, pacific] { HurricaneData::loadSeason(*atlantic, "al"); HurricaneData::loadSeason(*pacific, "ep"); }, [this, mine, atlantic, pacific] {
        if (!closed && mine == generation) {
            seasonAtlantic = atlantic;
            seasonPacific = pacific;
            fillSeasons();
            textStatus.setText(string{"Click a storm's buttons for its track map and recon, its advisory products or its discussion"});
        }
    }};
}

QWidget * TropicalHub::card(const HurricaneData::StormEntry& entry, const string& basin) {
    auto * frame = new QFrame{content};
    frame->setFrameShape(QFrame::StyledPanel);
    const bool invest = entry.id.size() >= 2 && entry.classification != "TD" && entry.classification != "TS" && entry.classification != "HU" &&
        entry.classification != "STD" && entry.classification != "STS" && entry.classification != "PTC" && entry.classification != "PC" && entry.classification != "EX";
    const auto color = classColor(entry.wind);
    frame->setObjectName("stormCard");
    frame->setStyleSheet("QFrame#stormCard { border: 1px solid #999; border-left: 6px solid " + color.name() + "; }");
    auto * outer = new QHBoxLayout{frame};
    outer->setContentsMargins(10, 6, 8, 6);
    auto * layout = new QVBoxLayout;
    layout->setSpacing(2);
    outer->addLayout(layout, 1);
    // the satellite picture of the storm (NOAA / NESDIS STAR's GeoColor floater): a thumbnail that opens the larger picture
    auto * thumbnail = new QPushButton{frame};
    thumbnail->setFlat(true);
    thumbnail->setFixedSize(130, 130);
    thumbnail->setCursor(Qt::PointingHandCursor);
    thumbnail->setStyleSheet("QPushButton { border: none; }");
    thumbnail->setToolTip("GeoColor satellite picture (NOAA / NESDIS STAR): click for the floater with all the GOES fields");
    thumbnail->hide();
    outer->addWidget(thumbnail, 0, Qt::AlignTop);
    {
        string upper = entry.id;
        std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
        const string folder = "https://cdn.star.nesdis.noaa.gov/FLOATER/data/" + upper + "/GEOCOLOR/";
        QPointer<QPushButton> guard{thumbnail};
        const int mine = generation;
        const string floaterTitle = HurricaneData::idLabel(entry.id) + (entry.name.empty() ? "" : " " + entry.name);
        new FutureBytes{this, folder + "250x250.jpg", [this, guard, mine, upper, floaterTitle] (const QByteArray& bytes) {
            QPixmap picture;
            if (closed || mine != generation || guard.isNull() || bytes.size() < 500 || !picture.loadFromData(bytes)) {
                return;
            }
            guard->setIcon(QIcon{picture.scaled(130, 130, Qt::KeepAspectRatio, Qt::SmoothTransformation)});
            guard->setIconSize(QSize{130, 130});
            guard->show();
            QObject::connect(guard.data(), &QPushButton::clicked, [this, upper, floaterTitle] { new FloaterViewer{this, upper, floaterTitle}; });
        }};
    }
    const QString title = QString::fromStdString(HurricaneData::idLabel(entry.id)) + "  " + QString::fromStdString(entry.name) + "  -  " + basinName(basin);
    auto * titleLabel = new QLabel{"<b style='font-size:14px'>" + title.toHtmlEscaped() + "</b>", frame};
    titleLabel->setStyleSheet("border: none;");
    layout->addWidget(titleLabel);
    QString line = (invest ? QString{"Invest"} : className(entry.classification));
    if (entry.wind >= 0) {
        line += ", " + knots(entry.wind);
    }
    if (entry.pressure > 0) {
        line += ", " + QString::number(entry.pressure) + " mb";
    }
    if (entry.lat != 0.0 || entry.lon != 0.0) {
        line += "  -  " + position(entry.lat, entry.lon);
    }
    if (entry.movementDir >= 0 && entry.movementSpeed >= 0) {
        line += ", moving " + compass(entry.movementDir) + " at " + QString::number(entry.movementSpeed) + " kt";
    }
    auto * info = new QLabel{line, frame};
    info->setStyleSheet("border: none;");
    info->setWordWrap(true);
    layout->addWidget(info);
    if (!entry.lastUpdate.empty()) {
        auto * updated = new QLabel{"Updated " + updatedText(entry.lastUpdate) + (entry.advNum.empty() ? QString{} : "  (advisory " + QString::fromStdString(entry.advNum) + ")"), frame};
        updated->setStyleSheet("border: none; color: gray;");
        layout->addWidget(updated);
    }
    auto * row = new QHBoxLayout;
    const auto addButton = [&] (const QString& text, std::function<void()> action) {
        auto * button = new QPushButton{text, frame};
        button->setStyleSheet("");
        QObject::connect(button, &QPushButton::clicked, action);
        row->addWidget(button);
    };
    const auto id = entry.id;
    addButton("Track map and recon", [this, basin, id] { new HurricaneViewer{this, basin, id}; });
    if (!invest) {
        const auto stormName = entry.name;
        addButton("Recon flight", [this, id, stormName] { new ReconViewer{this, id, stormName}; });
    }
    if (!entry.advisoryUrl.empty() || !entry.discussionUrl.empty()) {
        const auto copy = entry;
        addButton("Advisory products", [this, copy] { new AdvisoryViewer{this, copy}; });
    }
    row->addStretch();
    layout->addLayout(row);
    return frame;
}

void TropicalHub::fillStorms() {
    clear(stormsLayout);
    bool any = false;
    bool pending = false;
    for (const auto& basin : storms) {
        pending = pending || !basin.loaded;
        if (basin.loaded && !basin.error.empty() && basin.entries.empty()) {
            stormsLayout->addWidget(note(basinName(basin.basin) + ": " + QString::fromStdString(basin.error), content));
        }
        for (const auto& entry : basin.entries) {
            if (entry.active) {
                stormsLayout->addWidget(card(entry, basin.basin));
                any = true;
            }
        }
    }
    if (!any) {
        stormsLayout->addWidget(note(pending ? "Loading..." : "No active storms or invests in the Atlantic, East Pacific or Central Pacific basins.", content));
    }
}

void TropicalHub::fillOutlook() {
    clear(outlookLayout);
    if (!outlook || !outlook->error.empty()) {
        outlookLayout->addWidget(note(outlook ? QString::fromStdString(outlook->error) : "The outlook is not available.", content));
        return;
    }
    if (outlook->areas.empty()) {
        outlookLayout->addWidget(note("NHC outlines no areas of possible development right now.", content));
        return;
    }
    for (const auto& area : outlook->areas) {
        const auto& risk = area.risk7.empty() ? area.risk2 : area.risk7;
        const QString color = risk == "High" ? "#d33" : risk == "Medium" ? "#e80" : "#cb0";
        outlookLayout->addWidget(body("<span style='color:" + color + "'>&#9632;</span> <b>" + QString::fromStdString(area.basin) + " area " + QString::fromStdString(area.area) +
            "</b> near " + position(area.centerLat, area.centerLon) + ":  " + QString::number(area.prob2) + "% in 2 days (" + QString::fromStdString(area.risk2) + "),  " +
            QString::number(area.prob7) + "% in 7 days (" + QString::fromStdString(area.risk7) + ")", content));
    }
}

void TropicalHub::fillPod() {
    clear(podLayout);
    if (!pod || !pod->error.empty()) {
        podLayout->addWidget(note(pod ? QString::fromStdString(pod->error) : "The Plan of the Day is not available.", content));
        return;
    }
    podLayout->addWidget(body(PodViewer::summary(*pod), content));
}

void TropicalHub::fillLocations() {
    clear(locationsLayout);
    if (!wsp || !wsp->map.ok) {
        locationsLayout->addWidget(note(wsp && !wsp->error.empty() ? QString::fromStdString(wsp->error) : "No wind probabilities right now.", content));
        return;
    }
    const auto table = StrikeReport::nhcTable(wsp->map);
    if (table.isEmpty()) {
        locationsLayout->addWidget(note("Save a location in the Settings to see its chances.", content));
        return;
    }
    locationsLayout->addWidget(body(table, content));
    locationsLayout->addWidget(note("NHC's five-day chance of sustained winds of at least 34, 50 and 64 kt, from the " + QString::fromStdString(wsp->map.cycle.substr(8)) + "Z cycle (every storm "
        "together). 'none' means outside every probability band, under 5 %. Open a storm's track map for the ensemble members' view.", content));
}

void TropicalHub::fillSeasons() {
    clear(seasonLayout);
    const auto one = [&] (const std::shared_ptr<HurricaneData::SeasonData>& data, const QString& name) {
        if (!data || !data->error.empty()) {
            seasonLayout->addWidget(note(name + ": " + (data ? QString::fromStdString(data->error) : QString{"not available"}), content));
            return;
        }
        const auto now = tally(data->current);
        const auto seasons = UtilitySeason::seasons(data->history);
        using S = UtilitySeason::Season;
        QString text = "<b>" + name + " " + QString::number(data->currentYear) + "</b>:  " + QString::number(now.named) + " named storms, " + QString::number(now.hurricanes) +
            " hurricanes, " + QString::number(now.major) + " major, ACE " + QString::number(now.ace, 'f', 1) + (now.tike > 0.0 ? ", TIKE " + QString::number(std::lround(now.tike)) + " TJ" : QString{}) + ".  <span style='color:gray'>1991-2020 whole season: " +
            QString::number(UtilitySeason::mean(seasons, 1991, 2020, &S::named), 'f', 1) + " / " + QString::number(UtilitySeason::mean(seasons, 1991, 2020, &S::hurricanes), 'f', 1) + " / " +
            QString::number(UtilitySeason::mean(seasons, 1991, 2020, &S::major), 'f', 1) + ", ACE " + QString::number(UtilitySeason::mean(seasons, 1991, 2020, &S::ace), 'f', 0) + "</span>";
        seasonLayout->addWidget(body(text, content));
        const auto standing = AceChart::standing(*data);
        if (!standing.isEmpty()) {
            seasonLayout->addWidget(body(standing, content));
        }
        const auto tikeStanding = AceChart::standing(*data, AceChart::Metric::Tike);
        if (!tikeStanding.isEmpty()) {
            seasonLayout->addWidget(body(tikeStanding, content));
        }
        auto * chart = new AceChart{content};
        chart->setMinimumHeight(280);
        chart->setData(data);
        seasonLayout->addWidget(chart);
    };
    one(seasonAtlantic, "Atlantic");
    one(seasonPacific, "East and Central Pacific");
}
