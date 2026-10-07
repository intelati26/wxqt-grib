// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HurricaneViewer.h"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>
#include "hurricane/AdvisoryViewer.h"
#include "hurricane/EnsembleStatsViewer.h"
#include "hurricane/IntensityViewer.h"
#include "hurricane/PodViewer.h"
#include "hurricane/SeasonViewer.h"
#include "hurricane/UtilityChanges.h"
#include "hurricane/ShipsViewer.h"
#include "hurricane/VdmViewer.h"
#include "objects/FutureVoid.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
    using Group = UtilityAtcf::Group;

    // distance in km between two points on the earth
    double kilometers(double lat1, double lon1, double lat2, double lon2) {
        const double rad = std::numbers::pi / 180.0;
        const double a = std::pow(std::sin((lat2 - lat1) * rad / 2.0), 2) +
            std::cos(lat1 * rad) * std::cos(lat2 * rad) * std::pow(std::sin((lon2 - lon1) * rad / 2.0), 2);
        return 12742.0 * std::asin(std::min(1.0, std::sqrt(a)));
    }

    QIcon swatch(const QColor& color) {
        QPixmap pixmap{14, 14};
        pixmap.fill(color);
        return QIcon{pixmap};
    }

    const std::vector<Group>& allGroups() {
        static const std::vector<Group> groups{Group::Official, Group::Consensus, Group::Global, Group::Hurricane, Group::Ensemble, Group::Simple, Group::Other};
        return groups;
    }

    // painting order: the broad background first, the official forecast last
    int paintRank(Group group) {
        switch (group) {
            case Group::Ensemble: return 0;
            case Group::Other: return 1;
            case Group::Simple: return 2;
            case Group::Global: return 3;
            case Group::Hurricane: return 4;
            case Group::Consensus: return 5;
            default: return 6;
        }
    }

    QString knots(int wind) {   // "85 kt (Cat 2)"
        return QString::fromStdString(UtilityAtcf::windLabel(wind));
    }

    QString knotsOf(double wind) {
        return UtilityEcmwfTracks::has(wind) ? knots(static_cast<int>(std::lround(wind))) : QString{"-"};
    }
}

QColor HurricaneViewer::groupColor(Group group) {
    switch (group) {
        case Group::Official: return QColor{255, 255, 255};
        case Group::Consensus: return QColor{255, 214, 0};
        case Group::Global: return QColor{0, 200, 255};
        case Group::Hurricane: return QColor{255, 90, 220};
        case Group::Ensemble: return QColor{140, 165, 255, 105};
        case Group::Simple: return QColor{120, 225, 120};
        default: return QColor{190, 190, 190};
    }
}

QColor HurricaneViewer::categoryColor(int category) {
    static const QColor colors[] = {QColor{94, 186, 255}, QColor{0, 250, 244}, QColor{255, 255, 204}, QColor{255, 231, 117},
                                    QColor{255, 193, 64}, QColor{255, 143, 32}, QColor{255, 96, 96}};
    return colors[std::clamp(category, 0, 6)];
}

HurricaneViewer::HurricaneViewer(Window * parent)
    : Window{parent}
    , comboBasin{this, {"Atlantic", "East Pacific", "Central Pacific"}}
    , comboStorm{this, {"Loading the storm list..."}}
    , buttonRefresh{this, None, "Refresh"}
    , buttonZoom{this, None, "Zoom to the storm"}
    , buttonStats{this, None, "Ensemble statistics..."}
    , buttonShips{this, None, "SHIPS and RI..."}
    , buttonPod{this, None, "Recon plan of the day..."}
    , buttonVdm{this, None, "Recon vortex messages..."}
    , buttonIntensity{this, None, "Intensity chart..."}
    , buttonSeason{this, None, "Season table and ACE..."}
    , buttonText{this, None, "NHC advisory text..."}
    , buttonOutlook{this, None, "Tropical weather outlook text..."}
    , textStatus{this, "Loading..."}
    , comboRecon{this, {"Flight-level wind", "SFMR surface wind"}}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tropical cyclones (NHC basins) - track, model guidance and recon");
    textStatus.setWordWrap(false);

    // the coastlines and borders of the basin (resourceCreation/createBasinCoast.py): float pairs, a NaN pair ends a line
    QFile file{":/res/nhc_basins.bin"};
    if (file.open(QIODevice::ReadOnly)) {
        const auto bytes = file.readAll();
        const auto * values = reinterpret_cast<const float *>(bytes.constData());
        std::vector<std::pair<float, float>> line;
        for (qsizetype i = 0; i + 1 < bytes.size() / 4; i += 2) {
            if (std::isnan(values[i])) {
                coast.push_back(std::move(line));
                line.clear();
            } else {
                line.emplace_back(values[i], values[i + 1]);
            }
        }
    }

    const auto dimens = UtilityUI::getScreenBounds();
    const int panelWidth = 330;
    const auto side = std::max(320, std::min(dimens[0] - panelWidth - 40, dimens[1] - 170));
    view = std::make_unique<MapView>(this, side);
    auto * map = view->map();
    map->dataLayer = [] (QPainter& painter) { painter.fillRect(QRectF{-1.0e6, -1.0e6, 2.0e6, 2.0e6}, QColor{16, 26, 42}); };   // map coordinates: a huge rectangle
    map->topLayer = [this] (QPainter& painter) { paintMap(painter); paintOutlook(painter); paintPlannedRecon(painter); paintLegend(painter); };
    view->onPointer = [this] (const QPointF& at) { showHover(at); };
    view->onLeave = [this] { hoverLabel->hide(); if (!hoverTech.empty()) { hoverTech.clear(); view->map()->update(); } };
    view->showRegion(5.0, 50.0, -100.0, -10.0);

    hoverLabel = new QLabel{map};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 220); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();

    // the side panel: the storm's numbers, the guidance families, the recon switch
    panel = new QWidget{this};
    panel->setFixedWidth(panelWidth);
    auto * column = new QVBoxLayout{panel};
    column->setContentsMargins(4, 0, 0, 0);
    infoLabel = new QLabel{panel};
    infoLabel->setWordWrap(true);
    infoLabel->setTextFormat(Qt::RichText);
    infoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    infoLabel->setOpenExternalLinks(true);
    column->addWidget(infoLabel, 1);
    auto * heading = new QLabel{"<b>Track guidance</b> (the newest run of each model)", panel};
    heading->setWordWrap(true);
    column->addWidget(heading);
    for (const auto group : allGroups()) {
        auto * check = new QCheckBox{QString::fromStdString(UtilityAtcf::groupName(group)), panel};
        check->setIcon(swatch(groupColor(group).alpha() == 255 ? groupColor(group) : QColor{140, 165, 255}));
        check->setChecked(group != Group::Other);
        QObject::connect(check, &QCheckBox::toggled, [this] { view->map()->update(); });
        column->addWidget(check);
        groupChecks.emplace_back(group, check);
    }
    wwCheck = new QCheckBox{"Watches and warnings", panel};
    wwCheck->setChecked(true);
    QObject::connect(wwCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    column->addSpacing(4);
    column->addWidget(wwCheck);
    coneCheck = new QCheckBox{"NHC forecast cone", panel};
    coneCheck->setChecked(true);
    radiiCheck = new QCheckBox{"Wind radii now (34 / 50 / 64 kt)", panel};
    radiiCheck->setChecked(true);
    QObject::connect(coneCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    QObject::connect(radiiCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    column->addSpacing(4);
    column->addWidget(coneCheck);
    column->addWidget(radiiCheck);
    podCheck = new QCheckBox{"Planned recon flights (Plan of the Day)", panel};
    podCheck->setChecked(true);
    QObject::connect(podCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    column->addWidget(podCheck);
    outlookCheck = new QCheckBox{"Development areas (Tropical Weather Outlook)", panel};
    outlookCheck->setChecked(true);
    QObject::connect(outlookCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    column->addWidget(outlookCheck);
    autoCheck = new QCheckBox{"Refresh every 10 minutes, alert on a new advisory", panel};
    autoCheck->setChecked(Utility::readPref("HURRICANE_AUTO", "false") == "true");
    QObject::connect(autoCheck, &QCheckBox::toggled, [this] (bool on) {
        Utility::writePref("HURRICANE_AUTO", on ? "true" : "false");
        on ? refreshTimer.start() : refreshTimer.stop();
    });
    column->addWidget(autoCheck);
    fixCheck = new QCheckBox{"Recon centre fixes (vortex messages)", panel};
    fixCheck->setChecked(true);
    QObject::connect(fixCheck, &QCheckBox::toggled, [this] { view->map()->update(); });
    column->addWidget(fixCheck);
    static const char * ensembleNames[3] = {"AIFS ENS members (ECMWF AI)", "IFS ENS members (ECMWF)", "AIFS and IFS unperturbed runs"};
    static const QColor ensembleSwatch[3] = {QColor{60, 220, 170}, QColor{255, 150, 60}, QColor{255, 255, 255}};
    column->addSpacing(4);
    for (int i = 0; i < 3; i++) {
        ensembleChecks[i] = new QCheckBox{ensembleNames[i], panel};
        ensembleChecks[i]->setIcon(swatch(ensembleSwatch[i]));
        ensembleChecks[i]->setChecked(i != 1);
        QObject::connect(ensembleChecks[i], &QCheckBox::toggled, [this] { view->map()->update(); });
        column->addWidget(ensembleChecks[i]);
    }
    column->addWidget(buttonIntensity.getView());
    column->addWidget(buttonSeason.getView());
    column->addWidget(buttonText.getView());
    column->addWidget(buttonOutlook.getView());
    column->addWidget(buttonStats.getView());
    column->addWidget(buttonShips.getView());
    column->addWidget(buttonPod.getView());
    column->addWidget(buttonVdm.getView());
    reconCheck = new QCheckBox{"Recon flights (HDOB, the last 6 hours)", panel};
    reconCheck->setChecked(Utility::readPref("HURRICANE_RECON", "false") == "true");
    column->addSpacing(6);
    column->addWidget(reconCheck);
    column->addWidget(comboRecon.getView());
    comboRecon.connect([this] { view->map()->update(); });
    QObject::connect(reconCheck, &QCheckBox::toggled, [this] (bool on) {
        Utility::writePref("HURRICANE_RECON", on ? "true" : "false");
        if (on && !recon) {
            loadRecon();
        }
        view->map()->update();
        updateInfo();
    });

    comboBasin.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("HURRICANE_BASIN", 0), 0, 2)));
    comboBasin.connect([this] {
        Utility::writePrefInt("HURRICANE_BASIN", comboBasin.getIndex());
        recon.reset();
        storm.reset();
        loadList();
    });
    comboStorm.connect([this] { if (!filling) { loadStorm(); } });
    buttonRefresh.connect([this] { loadList(); });
    buttonZoom.connect([this] { zoomToStorm(); });
    buttonSeason.connect([this] { openSeason(); });
    buttonOutlook.connect([this] {
        // the outlook of the basin on show: Atlantic, Eastern Pacific, Central Pacific
        const auto code = basinCode();
        const string page = code == "al" ? "MIATWOAT" : code == "ep" ? "MIATWOEP" : "HFOTWOCP";
        new AdvisoryViewer{this, "NHC Tropical Weather Outlook", {{"Outlook", "https://www.nhc.noaa.gov/text/" + page + ".shtml"}}};
    });
    buttonText.connect([this] {
        const auto index = comboStorm.getIndex();
        if (index >= 0 && static_cast<size_t>(index) < entries.size() && !entries[static_cast<size_t>(index)].advisoryUrl.empty()) {
            new AdvisoryViewer{this, entries[static_cast<size_t>(index)]};
        } else {
            textStatus.setText(string{"NHC has no advisory text for this storm (advisories are issued for active storms)."});
        }
    });
    refreshTimer.setInterval(10 * 60 * 1000);
    refreshTimer.connect(&refreshTimer, &QTimer::timeout, this, [this] { loadList(); });
    if (autoCheck->isChecked()) {
        refreshTimer.start();
    }
    buttonIntensity.connect([this] {
        if (storm) {
            new IntensityViewer{this, storm, ensembles, ships, vdm, recon};
        }
    });
    buttonVdm.connect([this] {
        if (vdm && storm) {
            new VdmViewer{this, vdm, QString::fromStdString(HurricaneData::idLabel(storm->id))};
        } else {
            textStatus.setText(string{"The vortex messages have not loaded yet."});
        }
    });
    buttonPod.connect([this] {
        if (pod) {
            new PodViewer{this, pod};
        } else {
            textStatus.setText(string{"The Plan of the Day has not loaded yet."});
        }
    });
    buttonShips.connect([this] {
        if (storm && ships) {
            new ShipsViewer{this, ships, storm};
        } else {
            textStatus.setText(string{"No SHIPS forecast loaded for this storm yet."});
        }
    });
    buttonStats.connect([this] {
        if (storm && ensembles && !ensembles->sets.empty()) {
            new EnsembleStatsViewer{this, storm, ensembles};
        } else {
            textStatus.setText(string{"No ensemble members loaded for this storm yet."});
        }
    });
    rowTop.addWidget(comboBasin);
    rowTop.addWidget(comboStorm);
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonZoom);
    rowTop.addStretch();
    rowMain.addWidgetReal(map, 0, Qt::AlignTop | Qt::AlignLeft);
    auto * scroll = new QScrollArea{this};   // the panel is taller than a small screen
    scroll->setWidget(panel);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFixedWidth(panelWidth + 18);
    rowMain.addWidgetReal(scroll, 1, Qt::AlignTop | Qt::AlignLeft);
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addLayout(rowMain);
    box.addStretch();
    box.getAndShow(this);
    loadList();
    loadPod();
    loadOutlook();
}

void HurricaneViewer::resizeEventCustom() {
    if (view == nullptr) {
        return;
    }
    const int above = rowTop.getView()->sizeHint().height() + textStatus.getView()->sizeHint().height();
    view->fit(width() - 330 - 18 - 24, height() - above - 40);
}

bool HurricaneViewer::groupShown(Group group) const {
    for (const auto& [g, check] : groupChecks) {
        if (g == group) {
            return check->isChecked();
        }
    }
    return false;
}

string HurricaneViewer::basinCode() const {
    static const char * codes[] = {"al", "ep", "cp"};
    return codes[std::clamp(comboBasin.getIndex(), 0, 2)];
}

void HurricaneViewer::loadList() {
    const auto gen = ++generation;
    textStatus.setText(string{"Loading the storm list..."});
    auto list = std::make_shared<vector<HurricaneData::StormEntry>>();
    auto error = std::make_shared<string>();
    auto ok = std::make_shared<bool>(false);
    const auto basin = basinCode();
    new FutureVoid{this,
        [list, error, ok, basin] { *ok = HurricaneData::loadStormList(*list, *error, basin); },
        [this, gen, list, error, ok] {
            if (closed || gen != generation) {
                return;
            }
            if (!*ok) {
                textStatus.setText(*error);
                return;
            }
            const auto before = comboStorm.getIndex() >= 0 && static_cast<size_t>(comboStorm.getIndex()) < entries.size()
                ? entries[static_cast<size_t>(comboStorm.getIndex())].id : string{};
            // a new advisory for the storm on screen (the timer reloads the list): tell the user
            string newAdvisory;
            for (const auto& old : entries) {
                for (const auto& fresh : *list) {
                    if (old.id == fresh.id && old.id == before && old.active && !old.advNum.empty() && !fresh.advNum.empty() && old.advNum != fresh.advNum) {
                        newAdvisory = fresh.name + " advisory " + fresh.advNum;
                    }
                }
            }
            entries = *list;
            if (!newAdvisory.empty()) {
                textStatus.setText("New NHC advisory: " + newAdvisory);
                QApplication::alert(this, 0);
            }
            vector<string> labels;
            for (const auto& entry : entries) {
                labels.push_back(entry.label);
            }
            filling = true;
            comboStorm.setList(labels);
            size_t pick = 0;
            // WXQT_STORM=al022026 (a development aid, with WXQT_OPEN) opens on that storm instead of the first
            const auto wanted = qEnvironmentVariableIsSet("WXQT_STORM") ? qEnvironmentVariable("WXQT_STORM").toStdString() : before;
            for (size_t i = 0; i < entries.size(); i++) {
                if (entries[i].id == wanted) {
                    pick = i;
                }
            }
            comboStorm.setIndex(pick);
            filling = false;
            recon.reset();
            loadStorm();
        }};
}

void HurricaneViewer::loadStorm() {
    const auto index = comboStorm.getIndex();
    if (index < 0 || static_cast<size_t>(index) >= entries.size()) {
        return;
    }
    const auto id = entries[static_cast<size_t>(index)].id;
    const auto gen = ++generation;
    textStatus.setText("Loading " + HurricaneData::idLabel(id) + ": best track, forecast and model guidance...");
    auto data = std::make_shared<HurricaneData::StormData>();
    new FutureVoid{this,
        [id, data] { HurricaneData::loadStorm(id, *data); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            storm = data;
            ensembles.reset();
            ships.reset();
            vdm.reset();
            gis.reset();
            showStorm();
            updateChanges();
            loadEnsembles();
            loadShips();
            loadVdm();
            loadGis();
            if (reconCheck->isChecked()) {
                loadRecon();
            }
        }};
}

// "what changed": the advisory on screen against the one before it (kept in the settings per storm), or, the first time, against the best track six hours earlier
void HurricaneViewer::updateChanges() {
    changeLines.clear();
    const auto index = comboStorm.getIndex();
    if (!storm || index < 0 || static_cast<size_t>(index) >= entries.size()) {
        return;
    }
    const auto& entry = entries[static_cast<size_t>(index)];
    if (!entry.active || entry.advNum.empty()) {
        return;
    }
    UtilityChanges::Snapshot now;
    now.advisory = entry.advNum;
    now.classification = entry.classification;
    now.wind = entry.wind;
    now.pressure = entry.pressure;
    now.lat = entry.lat;
    now.lon = entry.lon;
    now.moveDir = entry.movementDir;
    now.moveSpeed = entry.movementSpeed;
    for (const auto& f : storm->official.fixes) {
        if (f.wind > now.forecastPeak) {
            now.forecastPeak = f.wind;
            now.forecastPeakHour = f.tau;
        }
    }
    if (ships && ships->ships.ok) {
        for (const auto& p : ships->ships.riLines) {
            if (p.knots == 30 && p.hours == 24) {
                now.ri30 = p.percent;
            }
        }
    }
    const string lastKey = "HURRICANE_LAST_" + entry.id;
    const string priorKey = "HURRICANE_PRIOR_" + entry.id;
    const auto last = UtilityChanges::parse(Utility::readPref(lastKey, ""));
    if (last.valid() && last.advisory != now.advisory) {
        Utility::writePref(priorKey, UtilityChanges::serialize(last));
    }
    Utility::writePref(lastKey, UtilityChanges::serialize(now));
    auto prior = UtilityChanges::parse(Utility::readPref(priorKey, ""));
    if (prior.valid() && prior.advisory != now.advisory) {
        changeLines = UtilityChanges::describe(prior, now);
        changeTitle = "Since advisory " + prior.advisory + " (now " + now.advisory + ")";
        return;
    }
    // no earlier advisory seen by this program: the best track six hours before the newest fix
    if (storm->best.size() >= 2) {
        const auto& old = storm->best[storm->best.size() - 2];
        UtilityChanges::Snapshot before;
        before.advisory = "6 h earlier";
        before.classification = old.status;
        before.wind = old.wind;
        before.pressure = old.pressure;
        before.lat = old.lat;
        before.lon = old.lon;
        now.classification = storm->best.back().status;
        now.wind = storm->best.back().wind;
        now.pressure = storm->best.back().pressure;
        now.lat = storm->best.back().lat;
        now.lon = storm->best.back().lon;
        now.moveDir = -1;
        now.moveSpeed = -1;
        now.forecastPeak = -1;
        now.ri30 = -1.0;
        changeLines = UtilityChanges::describe(before, now);
        changeTitle = "Since the previous best-track fix (" + UtilityAtcf::formatTime(old.time) + ")";
    }
}

void HurricaneViewer::loadEnsembles() {
    const auto gen = generation;
    const auto id = storm->id;
    auto data = std::make_shared<HurricaneData::EnsembleData>();
    new FutureVoid{this,
        [id, data] { HurricaneData::loadEnsembles(id, *data); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            ensembles = data;
            HurricaneData::EnsembleSet gefs;
            if (storm && HurricaneData::gefsFromGuidance(storm->id, storm->guidance, gefs)) {
                ensembles->sets.push_back(std::move(gefs));   // for the statistics; its lines are drawn by the Ensemble members group
            }
            if (!ensembles->error.empty() && ensembles->sets.empty()) {
                textStatus.setText(ensembles->error);
            }
            updateInfo();
            view->map()->update();
        }};
}

// the season window: the data is loaded on first use (the history file is large the first time, then kept on disk)
void HurricaneViewer::openSeason() {
    textStatus.setText(string{"Loading the hurricane seasons (the first time this downloads NHC's HURDAT2 file, about 7 MB)..."});
    auto data = std::make_shared<HurricaneData::SeasonData>();
    const string basin = basinCode() == "al" ? "al" : "ep";   // the northeast Pacific file holds both the Eastern and the Central Pacific
    new FutureVoid{this,
        [data, basin] { HurricaneData::loadSeason(*data, basin); },
        [this, data] {
            if (closed) {
                return;
            }
            if (!data->error.empty()) {
                textStatus.setText(data->error);
                return;
            }
            textStatus.setText(string{"Seasons loaded"});
            new SeasonViewer{this, data};
        }};
}

void HurricaneViewer::loadVdm() {
    const auto gen = generation;
    const auto id = storm->id;
    auto data = std::make_shared<HurricaneData::VdmData>();
    new FutureVoid{this,
        [id, data] { HurricaneData::loadVdm(id, *data); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            vdm = data;
            updateInfo();
            view->map()->update();
        }};
}

void HurricaneViewer::loadGis() {
    const auto gen = generation;
    const auto index = comboStorm.getIndex();
    if (index < 0 || static_cast<size_t>(index) >= entries.size() || !entries[static_cast<size_t>(index)].active || entries[static_cast<size_t>(index)].coneZip.empty()) {
        return;   // a finished storm or an invest: NHC has no cone file for it
    }
    const auto entry = entries[static_cast<size_t>(index)];
    auto data = std::make_shared<HurricaneData::GisData>();
    new FutureVoid{this,
        [entry, data] { HurricaneData::loadGis(entry, *data); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            gis = data;
            updateInfo();
            view->map()->update();
        }};
}

void HurricaneViewer::loadOutlook() {
    auto data = std::make_shared<HurricaneData::OutlookData>();
    new FutureVoid{this,
        [data] { HurricaneData::loadOutlook(*data); },
        [this, data] {
            if (closed) {
                return;
            }
            outlook = data;
            updateInfo();
            view->map()->update();
        }};
}

void HurricaneViewer::loadPod() {
    auto data = std::make_shared<HurricaneData::PodData>();
    new FutureVoid{this,
        [data] { HurricaneData::loadPod(*data); },
        [this, data] {
            if (closed) {
                return;
            }
            pod = data;
            updateInfo();
            view->map()->update();
        }};
}

void HurricaneViewer::loadShips() {
    const auto gen = generation;
    const auto id = storm->id;
    auto data = std::make_shared<HurricaneData::ShipsData>();
    new FutureVoid{this,
        [id, data] { HurricaneData::loadShips(id, *data); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            ships = data;
            updateChanges();
            updateInfo();
        }};
}

void HurricaneViewer::loadRecon() {
    const auto gen = generation;
    textStatus.setText(string{"Loading the recent reconnaissance bulletins..."});
    auto data = std::make_shared<HurricaneData::ReconData>();
    const auto basin = basinCode();
    new FutureVoid{this,
        [data, basin] { HurricaneData::loadRecon(*data, 36, basin); },
        [this, gen, data] {
            if (closed || gen != generation) {
                return;
            }
            recon = data;
            textStatus.setText(recon->error.empty() ? "Recon bulletins loaded: flights within 600 km of the storm are drawn" : recon->error);
            updateInfo();
            view->map()->update();
        }};
}

void HurricaneViewer::showStorm() {
    if (!storm) {
        return;
    }
    if (!storm->error.empty()) {
        textStatus.setText(storm->error);
    } else {
        const auto& cycle = storm->guidance.empty() ? string{} : storm->guidance.front().cycle;
        textStatus.setText(std::to_string(storm->guidance.size()) + " guidance tracks" + (cycle.empty() ? "" : ", the newest run " + UtilityAtcf::formatTime(cycle)) +
            "   -   hover a line for its model, drag to pan, wheel to zoom");
    }
    updateInfo();
    zoomToStorm();
}

// the box around the best track, the official forecast and the main models (not the whole ensemble, which strays far)
void HurricaneViewer::zoomToStorm() {
    if (!storm) {
        return;
    }
    double minLat = 90.0, maxLat = -90.0, minLon = 180.0, maxLon = -180.0;
    const auto take = [&] (const UtilityAtcf::Fix& f) {
        minLat = std::min(minLat, f.lat);
        maxLat = std::max(maxLat, f.lat);
        minLon = std::min(minLon, f.lon);
        maxLon = std::max(maxLon, f.lon);
    };
    for (const auto& f : storm->best) take(f);
    for (const auto& f : storm->official.fixes) take(f);
    for (const auto& track : storm->guidance) {
        const auto group = UtilityAtcf::groupOf(track.tech);
        if (group == Group::Consensus || group == Group::Hurricane) {
            for (const auto& f : track.fixes) {
                if (f.tau <= 120) take(f);
            }
        }
    }
    if (minLat > maxLat) {
        return;
    }
    const double padLat = std::max(2.0, (maxLat - minLat) * 0.18);
    const double padLon = std::max(2.0, (maxLon - minLon) * 0.18);
    minLat = std::max(-5.0, minLat - padLat);
    maxLat = std::min(70.0, maxLat + padLat);
    minLon -= padLon;
    maxLon += padLon;
    // at least a 16 degree view, so a storm that has just formed is not a dot
    if (maxLon - minLon < 16.0) {
        const double mid = (minLon + maxLon) / 2.0;
        minLon = mid - 8.0;
        maxLon = mid + 8.0;
    }
    view->showRegion(minLat, maxLat, minLon, maxLon);
}

bool HurricaneViewer::reconNear(const UtilityHdob::Ob& ob) const {
    // the bulletins are not sorted by storm: keep what is within 600 km of the selected storm's newest position
    if (!storm || storm->best.empty()) {
        return true;
    }
    const auto& centre = storm->best.back();
    return kilometers(centre.lat, centre.lon, ob.lat, ob.lon) < 600.0;
}

QColor HurricaneViewer::reconColor(const UtilityHdob::Ob& ob) const {
    const double wind = comboRecon.getIndex() == 1 ? ob.sfmrWind : ob.windSpeed;
    if (!UtilityHdob::has(wind)) {
        return QColor{150, 150, 150};
    }
    return categoryColor(UtilityAtcf::categoryOf(static_cast<int>(wind)));
}

void HurricaneViewer::updateInfo() {
    if (!storm) {
        return;
    }
    const auto index = comboStorm.getIndex();
    const HurricaneData::StormEntry * entry = index >= 0 && static_cast<size_t>(index) < entries.size() ? &entries[static_cast<size_t>(index)] : nullptr;
    QString html;
    const auto name = !storm->name.empty() ? storm->name : (entry != nullptr ? entry->name : string{});
    html += "<h3>" + QString::fromStdString(HurricaneData::idLabel(storm->id) + (name.empty() ? "" : " " + name)) + "</h3>";
    if (!storm->best.empty()) {
        const auto& last = storm->best.back();
        const int category = last.wind >= 0 ? UtilityAtcf::categoryOf(last.wind) : 0;
        html += "<b>" + QString::fromStdString(UtilityAtcf::categoryName(category)) + "</b> at " + QString::fromStdString(UtilityAtcf::formatTime(last.time)) + "<br>";
        html += "Wind " + knots(last.wind) + (last.pressure >= 0 ? ", pressure " + QString::number(last.pressure) + " mb" : QString{}) + "<br>";
        html += "Position " + QString::number(std::abs(last.lat), 'f', 1) + (last.lat >= 0 ? "N " : "S ") +
            QString::number(std::abs(last.lon), 'f', 1) + (last.lon >= 0 ? "E" : "W") + "<br>";
        if (entry != nullptr && entry->movementSpeed >= 0) {
            html += "Moving " + QString::number(entry->movementDir) + " deg at " + QString::number(entry->movementSpeed) + " kt<br>";
        }
        int peak = -1;
        string peakTime;
        for (const auto& f : storm->best) {
            if (f.wind > peak) {
                peak = f.wind;
                peakTime = f.time;
            }
        }
        html += "Peak so far " + knots(peak) + " (" + QString::fromStdString(UtilityAtcf::formatTime(peakTime)) + ")<br>";
    }
    if (!storm->official.fixes.empty()) {
        int peak = -1;
        int peakTau = 0;
        for (const auto& f : storm->official.fixes) {
            if (f.wind > peak) {
                peak = f.wind;
                peakTau = f.tau;
            }
        }
        html += "<br><b>NHC forecast</b> (" + QString::fromStdString(UtilityAtcf::formatTime(storm->official.cycle)) + "): peak " + knots(peak) +
            " at " + QString::number(peakTau) + " h, out to " + QString::number(storm->official.fixes.back().tau) + " h<br>";
    }
    if (outlook && !outlook->areas.empty()) {
        QStringList parts;
        for (const auto& area : outlook->areas) {
            if (area.basin == (basinCode() == "al" ? "Atlantic" : "Pacific")) {
                parts << "area " + QString::fromStdString(area.area) + ": " + QString::number(area.prob2) + " % / " + QString::number(area.prob7) + " %";
            }
        }
        if (!parts.isEmpty()) {
            html += "<br><b>Outlook</b> (2 / 7 day formation chance) " + parts.join(", ") + "<br>";
        }
    }
    if (!changeLines.empty()) {
        html += "<br><b>" + QString::fromStdString(changeTitle) + "</b><br>";
        for (const auto& line : changeLines) {
            html += "&bull; " + QString::fromStdString(line).toHtmlEscaped() + "<br>";
        }
    }
    if (gis && !gis->watchWarnings.empty()) {
        std::set<string> kinds;
        for (const auto& w : gis->watchWarnings) {
            kinds.insert(w.kind.empty() ? UtilityNhcGis::nameFor(w.code) : w.kind);
        }
        QStringList names;
        for (const auto& k : kinds) {
            names << QString::fromStdString(k);
        }
        html += "<br><b>In effect</b> " + names.join(", ") + "<br>";
    } else if (gis && gis->error.empty() && gis->cone.ok) {
        html += "<br><b>Watches and warnings</b> none in effect<br>";
    }
    if (vdm && vdm->error.empty()) {
        html += "<br><b>Vortex messages</b> " + VdmViewer::summary(*vdm) + "<br>";
    }
    if (pod && pod->error.empty()) {
        html += "<br><b>Recon plan</b> " + PodViewer::summary(*pod, basinCode() != "al").mid(PodViewer::summary(*pod, basinCode() != "al").indexOf(' ', 12) + 1) + "<br>";
    }
    if (ships && ships->ships.ok) {
        html += "<br><b>SHIPS</b> " + ShipsChart::summary(ships->ships).mid(6) + "<br>";
    }
    if (ensembles && !ensembles->sets.empty()) {
        html += "<br><b>Ensembles</b> (ECMWF: contains ECMWF open data, CC BY 4.0)<br>";
        QStringList singles;
        for (const auto& set : ensembles->sets) {
            int members = 0;
            int alive = 0;
            for (const auto& m : set.storm.members) {
                if (m.type >= 2) {
                    members++;
                    alive += !m.steps.empty() && UtilityEcmwfTracks::has(m.steps.front().lat) ? 1 : 0;
                }
            }
            if (members > 0) {
                html += QString::fromStdString(set.label) + " " + QString::fromStdString(UtilityAtcf::formatTime(set.cycle)) + ": " + QString::number(alive) + " of " +
                    QString::number(members) + " members have a cyclone now<br>";
            } else {
                singles << QString::fromStdString(set.label);
            }
        }
        if (!singles.isEmpty()) {
            html += singles.join(", ") + " (unperturbed runs)<br>";
        }
    }
    if (entry != nullptr && !entry->discussionUrl.empty()) {
        html += "<br><a href=\"" + QString::fromStdString(entry->discussionUrl) + "\">Discussion</a> &nbsp; <a href=\"" + QString::fromStdString(entry->advisoryUrl) +
            "\">Advisory</a> &nbsp; <a href=\"" + QString::fromStdString(entry->graphicsUrl) + "\">Graphics</a><br>";
    }
    if (reconCheck != nullptr && reconCheck->isChecked()) {
        html += "<br><b>Recon</b><br>";
        if (!recon) {
            html += "loading...";
        } else {
            std::set<string> missions;
            double peakFlight = UtilityHdob::missing;
            double peakSfmr = UtilityHdob::missing;
            double lowPressure = UtilityHdob::missing;
            const UtilityHdob::Ob * newest = nullptr;
            int count = 0;
            for (const auto& message : recon->messages) {
                for (const auto& ob : message.obs) {
                    if (!reconNear(ob)) {
                        continue;
                    }
                    count++;
                    missions.insert(message.mission.substr(0, message.mission.find(' ')));
                    if (UtilityHdob::has(ob.peakWind)) peakFlight = std::max(peakFlight, ob.peakWind);
                    if (UtilityHdob::has(ob.sfmrWind)) peakSfmr = std::max(peakSfmr, ob.sfmrWind);
                    if (UtilityHdob::has(ob.surfacePressure) && (!UtilityHdob::has(lowPressure) || ob.surfacePressure < lowPressure)) lowPressure = ob.surfacePressure;
                    if (newest == nullptr || ob.seconds > newest->seconds) newest = &ob;
                }
            }
            if (count == 0) {
                html += "No flights within 600 km of the storm in the last 6 hours.";
            } else {
                QStringList ids;
                for (const auto& m : missions) ids << QString::fromStdString(m);
                html += "Aircraft " + ids.join(", ") + ", " + QString::number(count) + " observations, the latest " + QString::fromStdString(UtilityHdob::timeText(newest->seconds)) + "<br>";
                html += "Peak flight-level wind " + (UtilityHdob::has(peakFlight) ? knotsOf(peakFlight) : QString{"-"}) +
                    ", peak SFMR surface wind " + (UtilityHdob::has(peakSfmr) ? knotsOf(peakSfmr) : QString{"-"}) + "<br>";
                if (UtilityHdob::has(lowPressure)) {
                    html += "Lowest extrapolated surface pressure " + QString::number(lowPressure, 'f', 1) + " mb<br>";
                }
            }
        }
    }
    infoLabel->setText(html);
}

void HurricaneViewer::paintMap(QPainter& painter) {
    const auto t = view->transform();
    const double px = view->unitsPerPixel();
    painter.setRenderHint(QPainter::Antialiasing, true);
    // coastlines and borders
    painter.setPen(QPen{QColor{150, 165, 185}, 1.0 * px});
    painter.setBrush(Qt::NoBrush);
    for (const auto& line : coast) {
        QPainterPath path;
        bool started = false;
        for (const auto& [lon, lat] : line) {
            const auto p = t(lat, lon);
            if (p.x() < -1500.0 || p.x() > 1500.0 || p.y() < -1250.0 || p.y() > 1750.0) {
                started = false;   // far outside: skip it, and do not join across the gap
                continue;
            }
            if (!started) {
                path.moveTo(p);
                started = true;
            } else {
                path.lineTo(p);
            }
        }
        painter.drawPath(path);
    }
    if (!storm) {
        return;
    }
    // the model guidance, the background families first
    auto tracks = storm->guidance;
    std::stable_sort(tracks.begin(), tracks.end(), [] (const auto& a, const auto& b) { return paintRank(UtilityAtcf::groupOf(a.tech)) < paintRank(UtilityAtcf::groupOf(b.tech)); });
    for (const auto& track : tracks) {
        const auto group = UtilityAtcf::groupOf(track.tech);
        if (!groupShown(group)) {
            continue;
        }
        const bool hovered = track.tech == hoverTech;
        auto color = groupColor(group);
        QPainterPath path;
        for (size_t i = 0; i < track.fixes.size(); i++) {
            const auto p = t(track.fixes[i].lat, track.fixes[i].lon);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }
        const double width = hovered ? 3.2 : (group == Group::Ensemble ? 1.0 : group == Group::Consensus ? 2.2 : 1.5);
        painter.setPen(QPen{hovered ? QColor{255, 255, 255} : color, width * px});
        painter.drawPath(path);
        const auto end = t(track.fixes.back().lat, track.fixes.back().lon);
        painter.setBrush(color);
        painter.drawEllipse(end, 1.8 * px * (hovered ? 2.0 : 1.0), 1.8 * px * (hovered ? 2.0 : 1.0));
        painter.setBrush(Qt::NoBrush);
        if (hovered) {
            painter.setPen(QColor{255, 255, 255});
            QFont font{painter.font()};
            font.setPixelSize(static_cast<int>(12 * px));
            painter.setFont(font);
            painter.drawText(end + QPointF{6.0 * px, -4.0 * px}, QString::fromStdString(track.tech));
        }
    }
    // the forecast cone: NHC's own polygon when it has been loaded
    if (coneCheck->isChecked() && gis && gis->cone.ok) {
        QPainterPath cone;
        for (const auto& ring : gis->cone.polygons) {
            QPolygonF polygon;
            for (const auto& [lon, lat] : ring) {
                polygon << t(lat, lon);
            }
            QPainterPath piece;
            piece.addPolygon(polygon);
            piece.closeSubpath();
            cone = cone.united(piece);
        }
        painter.setPen(QPen{QColor{255, 255, 255, 190}, 1.4 * px, Qt::DashLine});
        painter.setBrush(QColor{255, 255, 255, 38});
        painter.drawPath(cone);
    } else if (coneCheck->isChecked() && storm->official.fixes.size() >= 2) {
        // otherwise the area swept by NHC's published error circles along the official forecast (12, 24 ... 120 h)
        const auto circle = [&] (double lat, double lon, double nm) {
            QPolygonF points;
            const double kmPerDegree = 111.2;
            for (int i = 0; i < 48; i++) {
                const double a = i * 2.0 * std::numbers::pi / 48.0;
                const double dLat = nm * 1.852 / kmPerDegree * std::cos(a);
                const double dLon = nm * 1.852 / (kmPerDegree * std::max(0.2, std::cos(lat * std::numbers::pi / 180.0))) * std::sin(a);
                points << t(lat + dLat, lon + dLon);
            }
            return points;
        };
        const auto hull = [] (QPolygonF points) {   // the convex hull (monotone chain): two circles joined into one tapered piece
            std::sort(points.begin(), points.end(), [] (const QPointF& a, const QPointF& b) { return a.x() < b.x() || (a.x() == b.x() && a.y() < b.y()); });
            const auto cross = [] (const QPointF& o, const QPointF& a, const QPointF& b) { return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x()); };
            QPolygonF result;
            for (int pass = 0; pass < 2; pass++) {
                const auto start = result.size();
                for (int i = 0; i < points.size(); i++) {
                    const auto& p = pass == 0 ? points[i] : points[points.size() - 1 - i];
                    while (result.size() >= start + 2 && cross(result[result.size() - 2], result[result.size() - 1], p) <= 0) {
                        result.removeLast();
                    }
                    result << p;
                }
                result.removeLast();
            }
            return result;
        };
        static const int hours[] = {0, 12, 24, 36, 48, 60, 72, 96, 120};
        vector<const UtilityAtcf::Fix *> marks;
        for (const int h : hours) {
            for (const auto& f : storm->official.fixes) {
                if (f.tau == h) {
                    marks.push_back(&f);
                    break;
                }
            }
        }
        QPainterPath cone;
        for (size_t i = 1; i < marks.size(); i++) {
            QPolygonF both = circle(marks[i - 1]->lat, marks[i - 1]->lon, UtilityAtcf::coneRadiusNm(marks[i - 1]->tau, storm->id.rfind("al", 0) != 0));
            both += circle(marks[i]->lat, marks[i]->lon, UtilityAtcf::coneRadiusNm(marks[i]->tau, storm->id.rfind("al", 0) != 0));
            QPainterPath piece;
            piece.addPolygon(hull(both));
            piece.closeSubpath();
            cone = cone.united(piece);
        }
        painter.setPen(QPen{QColor{255, 255, 255, 170}, 1.2 * px, Qt::DashLine});
        painter.setBrush(QColor{255, 255, 255, 38});
        painter.drawPath(cone);
    }
    // ECMWF's ensemble members (thin, one line each), then the unperturbed runs
    if (ensembles) {
        const auto drawTrack = [&] (const UtilityEcmwfTracks::Member& member, const QPen& pen) {
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            QPainterPath path;
            bool started = false;
            for (const auto& s : member.steps) {
                if (!UtilityEcmwfTracks::has(s.lat) || !UtilityEcmwfTracks::has(s.lon)) {
                    started = false;
                    continue;
                }
                const auto p = t(s.lat, s.lon);
                started ? path.lineTo(p) : path.moveTo(p);
                started = true;
            }
            painter.drawPath(path);
        };
        for (const auto& set : ensembles->sets) {
            if (set.label == "GEFS") {
                continue;
            }
            const bool isEnsemble = set.label == "AIFS ENS" || set.label == "IFS ENS";
            const int box = set.label == "AIFS ENS" ? 0 : set.label == "IFS ENS" ? 1 : 2;
            for (const auto& member : set.storm.members) {
                if (member.type >= 2) {
                    if (isEnsemble && ensembleChecks[box]->isChecked()) {
                        const auto color = box == 0 ? QColor{60, 220, 170, 85} : QColor{255, 150, 60, 85};
                        drawTrack(member, QPen{color, 1.1 * px});
                    }
                } else if (ensembleChecks[2]->isChecked()) {
                    // the control and high-resolution runs of the ensembles, and the single AIFS / IFS runs
                    const bool aifs = set.label.rfind("AIFS", 0) == 0;
                    const auto color = aifs ? QColor{40, 255, 170} : QColor{255, 110, 40};
                    const bool single = !isEnsemble;
                    drawTrack(member, QPen{color, (single ? 2.6 : 1.6) * px, single ? Qt::SolidLine : Qt::DashLine});
                }
            }
        }
    }
    // the official forecast: a heavy line, the intensity colour at each point, a label each day
    if (groupShown(Group::Official) && !storm->official.fixes.empty()) {
        QPainterPath path;
        const auto& fixes = storm->official.fixes;
        for (size_t i = 0; i < fixes.size(); i++) {
            const auto p = t(fixes[i].lat, fixes[i].lon);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }
        painter.setPen(QPen{QColor{0, 0, 0, 200}, 5.0 * px});
        painter.drawPath(path);
        painter.setPen(QPen{QColor{255, 255, 255}, 2.6 * px});
        painter.drawPath(path);
        QFont font{painter.font()};
        font.setPixelSize(static_cast<int>(11 * px));
        painter.setFont(font);
        for (const auto& f : fixes) {
            const auto p = t(f.lat, f.lon);
            painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
            painter.setBrush(categoryColor(f.wind >= 0 ? UtilityAtcf::categoryOf(f.wind) : 0));
            painter.drawEllipse(p, 4.2 * px, 4.2 * px);
            if (f.tau > 0 && f.tau % 24 == 0) {
                painter.setPen(QColor{255, 255, 255});
                painter.drawText(p + QPointF{7.0 * px, -5.0 * px}, QString::number(f.tau / 24) + " d");
            }
        }
    }
    // the best track so far
    if (!storm->best.empty()) {
        QPainterPath path;
        for (size_t i = 0; i < storm->best.size(); i++) {
            const auto p = t(storm->best[i].lat, storm->best[i].lon);
            i == 0 ? path.moveTo(p) : path.lineTo(p);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen{QColor{0, 0, 0, 200}, 4.4 * px});
        painter.drawPath(path);
        painter.setPen(QPen{QColor{200, 215, 235}, 2.0 * px});
        painter.drawPath(path);
        for (size_t i = 0; i < storm->best.size(); i++) {
            const auto& f = storm->best[i];
            const auto p = t(f.lat, f.lon);
            const bool last = i + 1 == storm->best.size();
            painter.setPen(QPen{QColor{0, 0, 0, 220}, 1.0 * px});
            painter.setBrush(categoryColor(f.wind >= 0 ? UtilityAtcf::categoryOf(f.wind) : 0));
            const double r = (last ? 7.5 : 3.6) * px;
            painter.drawEllipse(p, r, r);
        }
    }
    // watches and warnings: NHC's lines along the coast, hurricane warning red, hurricane watch pink, tropical storm warning blue, tropical storm watch yellow
    if (wwCheck->isChecked() && gis) {
        for (const auto& w : gis->watchWarnings) {
            QPainterPath path;
            for (const auto& ring : w.lines) {
                bool started = false;
                for (const auto& [lon, lat] : ring) {
                    const auto pt = t(lat, lon);
                    started ? path.lineTo(pt) : path.moveTo(pt);
                    started = true;
                }
            }
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen{QColor{0, 0, 0, 220}, 8.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            painter.drawPath(path);
            painter.setPen(QPen{QColor{QString::fromStdString(UtilityNhcGis::colorFor(w.code))}, 5.0 * px, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
            painter.drawPath(path);
        }
    }
    // the wind radii of the newest best-track fix: 34, 50 and 64 kt, a wedge of each quadrant (NE, SE, SW, NW)
    if (radiiCheck->isChecked() && !storm->best.empty()) {
        const auto& now = storm->best.back();
        static const QColor colors[3] = {QColor{255, 235, 80, 70}, QColor{255, 150, 40, 90}, QColor{255, 70, 70, 110}};
        for (int threshold = 0; threshold < 3; threshold++) {
            for (int quadrant = 0; quadrant < 4; quadrant++) {
                const int nm = now.radii[static_cast<size_t>(threshold)][static_cast<size_t>(quadrant)];
                if (nm <= 0) {
                    continue;
                }
                QPolygonF wedge;
                wedge << t(now.lat, now.lon);
                for (int step = 0; step <= 12; step++) {
                    const double bearing = (quadrant * 90.0 + step * 90.0 / 12.0) * std::numbers::pi / 180.0;   // from north, clockwise
                    const double dLat = nm / 60.0 * std::cos(bearing);
                    const double dLon = nm / (60.0 * std::max(0.2, std::cos(now.lat * std::numbers::pi / 180.0))) * std::sin(bearing);
                    wedge << t(now.lat + dLat, now.lon + dLon);
                }
                painter.setPen(QPen{colors[threshold].darker(150), 0.8 * px});
                painter.setBrush(colors[threshold]);
                painter.drawPolygon(wedge);
            }
        }
    }
    // the recon flight tracks
    if (reconCheck->isChecked() && recon) {
        for (const auto& message : recon->messages) {
            const UtilityHdob::Ob * previous = nullptr;
            for (const auto& ob : message.obs) {
                if (!reconNear(ob)) {
                    previous = nullptr;
                    continue;
                }
                const auto p = t(ob.lat, ob.lon);
                const auto color = reconColor(ob);
                if (previous != nullptr) {
                    painter.setPen(QPen{QColor{color.red(), color.green(), color.blue(), 200}, 2.4 * px});
                    painter.drawLine(t(previous->lat, previous->lon), p);
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(color);
                painter.drawEllipse(p, 2.6 * px, 2.6 * px);
                previous = &ob;
            }
        }
    }
}

// the Tropical Weather Outlook: shaded areas of possible development in NHC's risk colours, labelled with the 2 day and 7 day chances
void HurricaneViewer::paintOutlook(QPainter& painter) {
    if (outlookCheck == nullptr || !outlookCheck->isChecked() || !outlook) {
        return;
    }
    const auto t = view->transform();
    const double px = view->unitsPerPixel();
    const string name = basinCode() == "al" ? "Atlantic" : "Pacific";
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(12 * px));
    font.setBold(true);
    painter.setFont(font);
    for (const auto& area : outlook->areas) {
        if (area.basin != name) {
            continue;
        }
        const auto& risk = area.risk7.empty() ? area.risk2 : area.risk7;
        const QColor color = risk == "High" ? QColor{255, 60, 60} : risk == "Medium" ? QColor{255, 150, 30} : QColor{255, 225, 60};
        QPainterPath path;
        for (const auto& ring : area.rings) {
            QPolygonF polygon;
            for (const auto& [lon, lat] : ring) {
                polygon << t(lat, lon);
            }
            path.addPolygon(polygon);
            path.closeSubpath();
        }
        auto fill = color;
        fill.setAlpha(45);
        painter.setPen(QPen{color, 2.2 * px, Qt::DashLine});
        painter.setBrush(fill);
        painter.drawPath(path);
        const auto at = t(area.centerLat, area.centerLon);
        painter.setPen(color.lighter(130));
        painter.drawText(at + QPointF{-30 * px, 4 * px}, QString::number(area.prob2) + "% / " + QString::number(area.prob7) + "%");
    }
}

void HurricaneViewer::paintPlannedRecon(QPainter& painter) {
    if (fixCheck != nullptr && fixCheck->isChecked() && vdm) {
        const auto t = view->transform();
        const double px = view->unitsPerPixel();
        for (const auto& m : vdm->messages) {
            if (!UtilityVdm::has(m.lat) || !UtilityVdm::has(m.lon)) {
                continue;
            }
            const auto at = t(m.lat, m.lon);
            painter.setPen(QPen{QColor{255, 255, 255}, 1.6 * px});
            painter.setBrush(QColor{220, 40, 40, 200});
            painter.drawEllipse(at, 5.5 * px, 5.5 * px);
            painter.drawLine(at + QPointF{-8 * px, 0}, at + QPointF{8 * px, 0});
            painter.drawLine(at + QPointF{0, -8 * px}, at + QPointF{0, 8 * px});
        }
    }
    if (podCheck == nullptr || !podCheck->isChecked() || !pod || !pod->error.empty()) {
        return;
    }
    const auto t = view->transform();
    const double px = view->unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(static_cast<int>(11 * px));
    painter.setFont(font);
    vector<QPointF> labelled;   // a label only where there is room for it
    for (const auto * list : {basinCode() == "al" ? &pod->pod.atlantic : &pod->pod.pacific}) {   // the flights of the chosen basin
      for (const auto& requirement : *list) {
        for (const auto& f : requirement.flights) {
            if (!f.hasPosition) {
                continue;
            }
            const auto at = t(f.lat, f.lon);
            // a diamond: the planned centre of the mission
            QPolygonF diamond;
            diamond << at + QPointF{0, -7 * px} << at + QPointF{7 * px, 0} << at + QPointF{0, 7 * px} << at + QPointF{-7 * px, 0};
            painter.setPen(QPen{QColor{20, 20, 20}, 1.2 * px});
            painter.setBrush(QColor{255, 90, 255, 220});
            painter.drawPolygon(diamond);
            const bool room = std::none_of(labelled.begin(), labelled.end(), [&] (const QPointF& other) { return std::hypot(other.x() - at.x(), other.y() - at.y()) < 90 * px; });
            if (room) {
                labelled.push_back(at);
                painter.setPen(QColor{255, 190, 255});
                painter.drawText(at + QPointF{10 * px, 4 * px}, QString::fromStdString(f.aircraft + " " + f.fixTimes.substr(0, f.fixTimes.find(','))));
            }
        }
      }
    }
}

void HurricaneViewer::paintLegend(QPainter& painter) {
    const double px = view->unitsPerPixel();
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    double x = -490.0;
    const double y = 735.0;
    painter.setRenderHint(QPainter::Antialiasing, true);
    static const char * names[] = {"TD", "TS", "Cat 1", "Cat 2", "Cat 3", "Cat 4", "Cat 5"};
    painter.setPen(QColor{235, 235, 235});
    painter.drawText(QPointF{x, y + 4.0}, reconCheck != nullptr && reconCheck->isChecked() ? (comboRecon.getIndex() == 1 ? "SFMR wind:" : "Flight-level wind:") : "Intensity:");
    x += reconCheck != nullptr && reconCheck->isChecked() ? 118.0 : 62.0;
    for (int c = 0; c <= 6; c++) {
        painter.setPen(QPen{QColor{0, 0, 0, 200}, 0.9 * px});
        painter.setBrush(categoryColor(c));
        painter.drawEllipse(QPointF{x + 6.0, y}, 6.0, 6.0);
        painter.setPen(QColor{235, 235, 235});
        const QString label = names[c];
        painter.drawText(QPointF{x + 16.0, y + 4.0}, label);
        x += 30.0 + QFontMetricsF{font}.horizontalAdvance(label);
    }
}

void HurricaneViewer::showHover(const QPointF& pixels) {
    if (!storm) {
        return;
    }
    Hit best{QString{}, 11.0};   // pixels
    string bestTech;
    const auto check = [&] (double lat, double lon, const QString& text, const string& tech) {
        const auto at = view->toPixels(lat, lon);
        const double distance = std::hypot(at.x() - pixels.x(), at.y() - pixels.y());
        if (distance < best.distance) {
            best = {text, distance};
            bestTech = tech;
        }
    };
    for (const auto& track : storm->guidance) {
        const auto group = UtilityAtcf::groupOf(track.tech);
        if (!groupShown(group)) {
            continue;
        }
        const auto found = storm->longNames.find(track.tech);
        const auto title = QString::fromStdString(track.tech + (found != storm->longNames.end() ? " - " + found->second : string{}));
        for (const auto& f : track.fixes) {
            check(f.lat, f.lon, title + "\nrun " + QString::fromStdString(UtilityAtcf::formatTime(track.cycle)) + ", +" + QString::number(f.tau) + " h, " + knots(f.wind), track.tech);
        }
    }
    if (ensembles) {
        for (const auto& set : ensembles->sets) {
            if (set.label == "GEFS") {
                continue;
            }
            const bool isEnsemble = set.label == "AIFS ENS" || set.label == "IFS ENS";
            const int box = set.label == "AIFS ENS" ? 0 : set.label == "IFS ENS" ? 1 : 2;
            for (const auto& member : set.storm.members) {
                const bool shownMember = member.type >= 2 ? isEnsemble && ensembleChecks[box]->isChecked()
                    : ensembleChecks[2]->isChecked();
                if (!shownMember) {
                    continue;
                }
                const QString who = member.type >= 2 ? QString::fromStdString(set.label) + " member " + QString::number(member.number)
                    : QString::fromStdString(set.label) + (isEnsemble ? " unperturbed run" : " run");
                for (const auto& s : member.steps) {
                    if (UtilityEcmwfTracks::has(s.lat) && UtilityEcmwfTracks::has(s.lon)) {
                        check(s.lat, s.lon, who + "\nrun " + QString::fromStdString(UtilityAtcf::formatTime(set.cycle)) + ", +" + QString::number(s.hour) + " h" +
                            (UtilityEcmwfTracks::has(s.wind) ? ", " + knotsOf(s.wind) + " (10 m)" : QString{}) +
                            (UtilityEcmwfTracks::has(s.pressure) ? ", " + QString::number(static_cast<int>(std::lround(s.pressure))) + " mb" : QString{}), "");
                    }
                }
            }
        }
    }
    if (groupShown(Group::Official)) {
        for (const auto& f : storm->official.fixes) {
            check(f.lat, f.lon, "NHC official forecast\nrun " + QString::fromStdString(UtilityAtcf::formatTime(storm->official.cycle)) + ", +" + QString::number(f.tau) + " h, " + knots(f.wind) +
                (f.pressure > 0 ? ", " + QString::number(f.pressure) + " mb" : QString{}), "");
        }
    }
    for (const auto& f : storm->best) {
        check(f.lat, f.lon, "Best track\n" + QString::fromStdString(UtilityAtcf::formatTime(f.time)) + ", " + knots(f.wind) + (f.pressure > 0 ? ", " + QString::number(f.pressure) + " mb" : QString{}), "");
    }
    if (outlookCheck->isChecked() && outlook) {
        for (const auto& area : outlook->areas) {
            if (area.basin == (basinCode() == "al" ? "Atlantic" : "Pacific")) {
                check(area.centerLat, area.centerLon, "Tropical Weather Outlook area " + QString::fromStdString(area.area) + "\nChance of formation: " + QString::number(area.prob2) + " % in 2 days (" +
                      QString::fromStdString(area.risk2) + "), " + QString::number(area.prob7) + " % in 7 days (" + QString::fromStdString(area.risk7) + ")", "");
            }
        }
    }
    if (wwCheck->isChecked() && gis) {
        for (const auto& w : gis->watchWarnings) {
            for (const auto& ring : w.lines) {
                for (const auto& [lon, lat] : ring) {
                    check(lat, lon, QString::fromStdString(w.kind.empty() ? UtilityNhcGis::nameFor(w.code) : w.kind), "");
                }
            }
        }
    }
    if (fixCheck->isChecked() && vdm) {
        for (const auto& m : vdm->messages) {
            if (UtilityVdm::has(m.lat) && UtilityVdm::has(m.lon)) {
                QString text = "Recon centre fix " + VdmViewer::timeText(m.seconds) + "  " + QString::fromStdString(m.aircraft);
                if (UtilityVdm::has(m.pressure)) text += "\nMinimum pressure " + QString::number(static_cast<int>(m.pressure)) + " mb" + (m.extrapolated ? " (extrapolated)" : "");
                if (UtilityVdm::has(m.maxFlightWind())) text += "\nStrongest flight-level wind " + knotsOf(m.maxFlightWind());
                if (!m.eyeCharacter.empty()) text += "\nEye " + QString::fromStdString(m.eyeCharacter + " " + m.eyeShape);
                check(m.lat, m.lon, text, "");
            }
        }
    }
    if (reconCheck->isChecked() && recon) {
        for (const auto& message : recon->messages) {
            for (const auto& ob : message.obs) {
                if (!reconNear(ob)) {
                    continue;
                }
                QString text = "Recon " + QString::fromStdString(message.mission.substr(0, message.mission.find(' '))) + "  " + QString::fromStdString(UtilityHdob::timeText(ob.seconds));
                if (UtilityHdob::has(ob.windSpeed)) {
                    text += "\nFlight-level wind " + knotsOf(ob.windSpeed);
                    if (UtilityHdob::has(ob.windDirection)) text += " from " + QString::number(static_cast<int>(ob.windDirection)) + " deg";
                }
                if (UtilityHdob::has(ob.sfmrWind)) text += "\nSFMR surface wind " + knotsOf(ob.sfmrWind);
                if (UtilityHdob::has(ob.rainRate)) text += ", rain " + QString::number(static_cast<int>(ob.rainRate)) + " mm/h";
                if (UtilityHdob::has(ob.surfacePressure)) text += "\nSurface pressure (extrapolated) " + QString::number(ob.surfacePressure, 'f', 1) + " mb";
                if (UtilityHdob::has(ob.height)) text += "\nAltitude " + QString::number(static_cast<int>(ob.height)) + " m";
                check(ob.lat, ob.lon, text, "");
            }
        }
    }
    if (best.text.isEmpty()) {
        hoverLabel->hide();
        view->map()->setCursor(Qt::ArrowCursor);
        if (!hoverTech.empty()) {
            hoverTech.clear();
            view->map()->update();
        }
        return;
    }
    if (hoverTech != bestTech) {
        hoverTech = bestTech;
        view->map()->update();
    }
    hoverLabel->setText(best.text);
    hoverLabel->adjustSize();
    hoverLabel->move(12, 12);
    hoverLabel->show();
    hoverLabel->raise();
}
