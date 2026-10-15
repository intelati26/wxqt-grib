// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mrms/MrmsViewer.h"
#include <QButtonGroup>
#include <QDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QKeyEvent>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QTreeWidget>
#include "settings/Location.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <future>
#include <numbers>
#include <set>
#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QTimeZone>
#include "models/UtilityGrib.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "radar/Projection.h"
#include "settings/Location.h"
#include "util/To.h"
#include "util/Utility.h"
#include "util/UtilityUI.h"

namespace {
    const int loopCounts[] = {6, 12, 24};

    string timeText(const QDateTime& utc) {
        return utc.toString("ddd yyyy-MM-dd HH:mm").toStdString() + "Z  (" +
            utc.toLocalTime().toString("h:mm AP").toStdString() + " " +
            QTimeZone::systemTimeZone().abbreviation(utc.toLocalTime()).toStdString() + ")";
    }

    // what one background job hands back to the UI thread
    struct Loaded {
        vector<UtilityMrms::Scan> scans;
        UtilityMrms::Frame frame;
        string error;
    };
}

MrmsViewer::MrmsViewer(Window * parent)
    : Window{parent}
    , comboProduct{this, {"Loading products..."}}
    , comboScan{this, {"Loading..."}}
    , backForward{this, [this] { moveScan(1); }, [this] { moveScan(-1); }}
    , comboLoop{this, {"Loop: last 6 scans", "Loop: last 12 scans", "Loop: last 24 scans"}}
    , comboUnits{this, {"US units", "Metric units"}}
    , comboAuto{this, {"Auto-update: 2 min", "Auto-update: 5 min", "Auto-update: off"}}
    , comboLightning{this, {"Lightning: off", "Lightning: last 1 min", "Lightning: last 5 min (by age)", "Lightning: last 15 min (by age)", "Lightning: last 30 min (by age)"}}
    , comboChance{this, {"Lightning chance: off", "Lightning chance: next 30 min", "Lightning chance: next 60 min"}}
    , comboHail{this, {"Hail swaths: off", "Hail: last 30 min", "Hail: last hour", "Hail: last 2 hours", "Hail: last 6 hours", "Hail: last 24 hours"}}
    , comboRotation{this, {"Rotation tracks: off", "Rotation 0-2 km: last 30 min", "Rotation 0-2 km: last hour", "Rotation 0-2 km: last 2 hours", "Rotation 0-2 km: last 6 hours",
                            "Rotation 0-2 km: last 24 hours", "Rotation 3-6 km: last hour", "Rotation 3-6 km: last 2 hours", "Rotation 3-6 km: last 6 hours"}}
    , buttonOverlays{this, Icon::None, "Overlays: off"}
    , buttonLoop{this, Icon::Play, "Loop"}
    , buttonSave{this, Icon::None, "Save"}
    , shortcutSave{QKeySequence{"Ctrl+S"}, this}
    , textStatus{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("MRMS");
    const auto dimens = UtilityUI::getScreenBounds();
    const auto side = std::max(300, std::min(dimens[0] - 20, dimens[1] - 160));
    radar = new MapWidget{
        this, 0, 1, true, Location::radarSite(), side, side,
        [this] (double z, [[maybe_unused]] int pane) { changeZoom(z); },
        [this] (double x, double y, [[maybe_unused]] int pane) { changePosition(x, y); }};
    radar->setFixedSize(side, side);   // resized to the window by fitRadar()
    radar->mapState.setRadar(Location::radarSite());
    radar->mapState.reset();
    radar->mapDraw.initGeom();
    showConus();
    radarList.push_back(radar);
    layerDownload = std::make_unique<MapLayerDownload>(this, &radarList);
    layerDownload->downloadLayers();
    radar->setMouseTracking(true);
    radar->installEventFilter(this);
    hoverLabel = new QLabel{radar};
    hoverLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    hoverLabel->setStyleSheet("QLabel { background-color: rgba(15, 15, 15, 205); color: #f2f2f2; padding: 4px 8px; border-radius: 3px; }");
    hoverLabel->hide();
    radar->dataLayer = [this] (QPainter& painter) { paintData(painter); };
    radar->topLayer = [this] (QPainter& painter) { paintLegend(painter); };

    comboProduct.getView()->installEventFilter(this);   // its list is the grouped tree (showProductTree)
    comboProduct.connect([this] {
        stopLoop();
        userPickedProduct = true;
        Utility::writePref("MRMS_LAST_PRODUCT", product().id);
        loadScans();
    });
    comboUnits.setIndex(static_cast<size_t>(Utility::readPref("MRMS_UNITS", "us") == "metric" ? 1 : 0));
    comboUnits.connect([this] {
        Utility::writePref("MRMS_UNITS", us() ? "us" : "metric");
        radar->update();
        textStatus.setText(product().label + "  " + timeText(current.utc));
        updateLocations();
    });
    comboLoop.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_LOOP", 0), 0, 2)));
    comboLoop.connect([this] { Utility::writePrefInt("MRMS_LOOP", comboLoop.getIndex()); });
    comboAuto.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_AUTO", 0), 0, 2)));
    const auto applyAuto = [this] {
        static const int seconds[] = {120, 300, 0};
        const int chosen = seconds[std::clamp(comboAuto.getIndex(), 0, 2)];
        autoTimer.stop();
        if (chosen > 0) {
            autoTimer.start(chosen * 1000);
        }
    };
    comboAuto.connect([this, applyAuto] { Utility::writePrefInt("MRMS_AUTO", comboAuto.getIndex()); applyAuto(); });
    QObject::connect(&autoTimer, &QTimer::timeout, this, [this] { refreshNewest(); loadLightning(); loadChance(); loadHail(); loadRotation(); });
    comboChance.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_LIGHTNING_CHANCE", 0), 0, 2)));
    comboChance.getView()->setToolTip("The chance of lightning in the next 30 or 60 minutes (MRMS), as contour lines at 10, 30, 50 and 70 percent over any product");
    comboChance.connect([this] {
        Utility::writePrefInt("MRMS_LIGHTNING_CHANCE", comboChance.getIndex());
        chanceLines.clear();
        loadChance();
        radar->update();
    });
    comboHail.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_HAIL", 0), 0, 5)));
    comboHail.getView()->setToolTip("MRMS hail swaths over any product: the largest estimated hail size (MESH) of the period, of at least 1 inch (marginally severe)");
    comboHail.connect([this] {
        Utility::writePrefInt("MRMS_HAIL", comboHail.getIndex());
        hailLevels.clear();
        loadHail();
        radar->update();
    });
    comboRotation.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_ROTATION", 0), 0, 8)));
    comboRotation.getView()->setToolTip("MRMS rotation tracks over any product: where the strongest rotation passed (supercells, mesocyclones), from 6 thousandths per second up");
    comboRotation.connect([this] {
        Utility::writePrefInt("MRMS_ROTATION", comboRotation.getIndex());
        rotationIndices.clear();
        rotationLevels.clear();
        loadRotation();
        radar->update();
    });
    comboLightning.setIndex(static_cast<size_t>(std::clamp(Utility::readPrefInt("MRMS_LIGHTNING", 0), 0, 4)));
    comboLightning.connect([this] {
        Utility::writePrefInt("MRMS_LIGHTNING", comboLightning.getIndex());
        lightningPoints.clear();
        loadLightning();
        radar->update();
    });
    applyAuto();
    comboScan.connect([this] { stopLoop(); showScan(comboScan.getIndex()); });
    buttonLoop.getView()->setToolTip("Play the latest scans as a loop (they are downloaded and decoded first)");
    buttonLoop.connect([this] { looping ? stopLoop() : startLoop(); });
    buttonSave.getView()->setToolTip("Save the map - or the loaded loop - as PNG / animated PNG / WebP ..., with the product, valid time and save time in a border (Ctrl+S)");
    buttonSave.connect([this] { onSave(); });
    shortcutSave.connect([this] { onSave(); });
    loopTimer.setInterval(350);
    QObject::connect(&loopTimer, &QTimer::timeout, this, [this] { stepLoop(); });

    rowTop.addWidget(comboProduct);
    rowTop.addLayout(backForward);
    rowTop.addWidget(comboScan);
    rowTop.addWidget(comboLoop);
    rowTop.addWidget(comboUnits);
    rowTop.addWidget(comboAuto);
    for (auto * combo : {&comboLightning, &comboChance, &comboHail, &comboRotation}) {   // the choices live in these; the panel sets them
        combo->setVisible(false);
    }
    buttonOverlays.getView()->setToolTip("Lightning strikes, lightning chance, hail swaths and rotation tracks over the product: several at once");
    buttonOverlays.connect([this] { showOverlays(); });
    rowTop.addWidget(buttonOverlays);
    rowTop.addWidget(buttonLoop);
    rowTop.addWidget(buttonSave);
    rowTop.addStretch();
    box.addLayout(rowTop);
    textStatus.setWordWrap(false);   // a wrapped status would make the controls taller than fitRadar() allowed for
    box.addWidget(textStatus);
    labelLocations = new QLabel{this};
    labelLocations->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);   // a long line is cut, not made the width of the window (the full text is its tooltip)
    box.addWidgetReal(labelLocations, 0, Qt::Alignment{});
    box.addWidgetReal(radar, 0, Qt::AlignTop | Qt::AlignLeft);
    box.addStretch();
    box.getAndShow(this);
    fitRadar();
    rebuildProducts(Utility::readPref("MRMS_LAST_PRODUCT", UtilityMrms::products().front().id));
    loadScans();
    loadLightning();
    loadChance();
    loadHail();
    loadRotation();
    refreshOverlayLabel();
    // the rest of the server's products, found in the background
    auto found = std::make_shared<vector<UtilityMrms::Product>>();
    new FutureVoid{this,
        [found] { string error; UtilityMrms::discoverMore(*found, error); },
        [this, found] {
            if (closed || found->empty()) {
                return;
            }
            extraProducts = *found;
            // the product saved last time may be one of the newly found ones: switch to it unless another was picked since
            const auto before = product().id;
            rebuildProducts(userPickedProduct ? before : Utility::readPref("MRMS_LAST_PRODUCT", before));
            if (product().id != before) {
                stopLoop();
                loadScans();
            }
        }};
}

void MrmsViewer::rebuildProducts(const string& selectId) {
    static const vector<string> order{"Reflectivity", "More reflectivity", "Dual-polarization", "Hail", "Rotation", "Precipitation", "Precipitation estimates (QPE)", "Precipitation type and melting layer",
                                      "Storm structure", "Lightning", "Model environment", "Quality and gauge indices", "Other products"};
    auto list = UtilityMrms::products();
    // what the server lists beyond the hand-set products goes into bins by the family of its name
    const auto bin = [] (const string& id) -> string {
        const auto starts = [&id] (const char * prefix) { return id.rfind(prefix, 0) == 0; };
        if (starts("MultiSensor_QPE") || starts("RadarOnly_QPE")) return "Precipitation estimates (QPE)";
        if (starts("GaugeInflIndex") || starts("RadarAccumulationQualityIndex") || starts("RadarQualityIndex")) return "Quality and gauge indices";
        if (starts("MergedRhoHV") || starts("MergedZdr")) return "Dual-polarization";
        if (id.find("Reflectivity") != string::npos || starts("BREF_") || starts("CREF_") || starts("SeamlessHSR")) return "More reflectivity";
        if (starts("RotationTrack")) return "Rotation";
        if (starts("Lightning") || starts("Ltg") || starts("NLDN")) return "Lightning";
        if (starts("H50_") || starts("H60_") || starts("SHI") || starts("VII") || starts("VIL") || starts("EchoTop") || starts("LVL3_")) return "Storm structure";
        if (starts("Model_")) return "Model environment";
        if (starts("PrecipFlag") || starts("SyntheticPrecipRate") || starts("WarmRain") || starts("BrightBand")) return "Precipitation type and melting layer";
        return {};
    };
    for (auto extra : extraProducts) {
        if (extra.group == "Other products") {
            const auto binned = bin(extra.id);
            if (!binned.empty()) {
                extra.group = binned;
            }
        }
        list.push_back(extra);
    }
    const auto rank = [] (const UtilityMrms::Product& p) {
        const auto found = std::find(order.begin(), order.end(), p.group);
        return found == order.end() ? order.size() : static_cast<size_t>(found - order.begin());
    };
    std::stable_sort(list.begin(), list.end(), [&rank] (const auto& a, const auto& b) { return rank(a) < rank(b); });
    productList = list;
    vector<string> labels;
    size_t selected = 0;
    for (size_t i = 0; i < productList.size(); i += 1) {
        labels.push_back(productList[i].group + " - " + productList[i].label);
        if (productList[i].id == selectId) {
            selected = i;
        }
    }
    comboProduct.block();
    comboProduct.setList(labels);
    comboProduct.setIndex(selected);
    comboProduct.unblock();
}

// auto-update: only while the newest scan is the one shown (not looping, not looking at an older scan)
void MrmsViewer::refreshNewest() {
    if (closed || looping || refreshing || comboScan.getIndex() != 0 || scans.empty()) {
        return;
    }
    layerDownload->downloadLayers();   // the warnings and the rest move on with the radar
    refreshing = true;
    const auto chosen = product();
    const auto shownUtc = scans.back().utc;
    const auto gen = generation;
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, shownUtc, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error) && result->scans.back().utc > shownUtc) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            } else {
                result->frame.packed.clear();
            }
        },
        [this, result, gen] {
            refreshing = false;
            if (closed || gen != generation || result->frame.packed.isEmpty() || !result->error.empty()) {
                return;
            }
            scans = result->scans;
            comboToScan.clear();
            vector<string> labels;
            for (int i = static_cast<int>(scans.size()) - 1; i >= 0; i -= 1) {
                labels.push_back(timeText(scans[static_cast<size_t>(i)].utc));
                comboToScan.push_back(i);
            }
            comboScan.block();
            comboScan.setList(labels);
            comboScan.setIndex(0);
            comboScan.unblock();
            setFrame(result->frame);
        }};
}

const UtilityMrms::Product& MrmsViewer::product() const {
    const auto index = std::clamp(comboProduct.getIndex(), 0, static_cast<int>(productList.size()) - 1);
    return productList[static_cast<size_t>(index)];
}

// the radar widget only reports wheel / click / drag requests; the owner changes the view (as the radar screen does)
void MrmsViewer::changeZoom(double factor) {
    auto& state = radar->mapState;
    if (factor < 1.0 && state.zoom <= 0.02) {
        return;
    }
    const double oldZoom = state.zoom;
    state.zoom = std::min(state.zoom * factor, 40.0);
    const double change = state.zoom / oldZoom;
    // zoom about the pointer (the middle of the map when it is outside): the spot under it stays where it is
    double u = 0.0;
    double v = 0.0;
    if (pointerInside) {
        u = pointer.x() * 1000.0 / std::max(1, radar->width()) - 500.0;
        v = pointer.y() * 1000.0 / std::max(1, radar->height()) - 250.0;
    }
    state.xPos = u - (u - state.xPos) * change;
    state.yPos = v - (v - state.yPos) * change;
    radar->mapTextObject.add();
    radar->update();
}

void MrmsViewer::changePosition(double dx, double dy) {
    auto& state = radar->mapState;
    const double unitsPerPixel = 1000.0 / std::max(1, radar->width());   // the map is drawn on a 1000-unit window
    state.xPos += dx * unitsPerPixel;
    state.yPos += dy * unitsPerPixel;
    radar->mapTextObject.add();
    radar->update();
}

// the whole lower 48 in the middle of the square map (the view the viewer opens with)
void MrmsViewer::showConus() {
    auto& state = radar->mapState;
    const auto [ax, bx, ay, by] = projection();
    const double centerLon = -96.0;
    const double centerLat = 37.5;
    const double lonSpan = 62.0;   // the lower 48 is about 59 degrees wide, with a little room round it
    const double mercatorCenter = 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + centerLat * std::numbers::pi / 360.0));
    state.zoom = 1000.0 / (std::abs(ax) * lonSpan);
    state.xPos = -(ax * centerLon + bx) * state.zoom;
    state.yPos = 250.0 - (ay * mercatorCenter + by) * state.zoom;   // the map window runs from -250 to 750 vertically
}

// the map is a square (the radar widget's projection assumes one): as large as fits under the controls
void MrmsViewer::fitRadar() {
    if (radar == nullptr) {
        return;
    }
    // the controls and the status line above the map (each one line: the status does not wrap)
    const int above = ControlRow::heightIn(rowTop.getView(), width()) + textStatus.getView()->sizeHint().height() +
        (labelLocations != nullptr ? labelLocations->sizeHint().height() : 0);
    const int side = std::max(300, std::min(width() - 16, height() - above - 56));
    if (radar->width() != side) {
        radar->setFixedSize(side, side);
        radar->mapState.originalWidth = side;
        radar->mapState.originalHeight = side;
        radar->mapTextObject.add();
    }
}

void MrmsViewer::resizeEventCustom() {
    fitRadar();
}

void MrmsViewer::closeEventCustom() {
    closed = true;
    loopTimer.stop();
}

void MrmsViewer::loadScans() {
    const auto gen = ++generation;
    loopFrames.clear();
    const auto chosen = product();
    textStatus.setText("Loading " + chosen.label + "...");
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, result] {
            if (UtilityMrms::scans(chosen, result->scans, result->error)) {
                UtilityMrms::frame(chosen, result->scans.back(), result->frame, result->error);
            }
        },
        [this, gen, result] {
            if (closed || gen != generation) {
                return;
            }
            scans = result->scans;
            comboToScan.clear();
            vector<string> labels;
            for (int i = static_cast<int>(scans.size()) - 1; i >= 0; i -= 1) {   // newest first
                labels.push_back(timeText(scans[static_cast<size_t>(i)].utc));
                comboToScan.push_back(i);
            }
            comboScan.block();
            comboScan.setList(labels.empty() ? vector<string>{"No scans"} : labels);
            comboScan.setIndex(0);
            comboScan.unblock();
            if (!result->error.empty()) {
                textStatus.setText(result->error);
                return;
            }
            setFrame(result->frame);
        }};
}

void MrmsViewer::showScan(int comboIndex) {
    if (comboIndex < 0 || comboIndex >= static_cast<int>(comboToScan.size())) {
        return;
    }
    const auto gen = ++generation;
    loopFrames.clear();   // a loop belongs to the scan list it was made from
    const auto chosen = product();
    const auto scan = scans[static_cast<size_t>(comboToScan[static_cast<size_t>(comboIndex)])];
    textStatus.setText("Loading " + timeText(scan.utc) + "...");
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [chosen, scan, result] { UtilityMrms::frame(chosen, scan, result->frame, result->error); },
        [this, gen, result] {
            if (closed || gen != generation) {
                return;
            }
            if (!result->error.empty()) {
                textStatus.setText(result->error);
                return;
            }
            setFrame(result->frame);
        }};
}

void MrmsViewer::moveScan(int step) {
    const auto next = comboScan.getIndex() + step;
    if (next >= 0 && next < static_cast<int>(comboToScan.size())) {
        stopLoop();
        comboScan.setIndex(static_cast<size_t>(next));   // its signal loads the scan
    }
}

void MrmsViewer::setFrame(const UtilityMrms::Frame& frame) {
    current = frame;
    currentIndices = frame.indices();
    currentColors = UtilityMrms::colorTable(product(), frame);
    setTitle("MRMS - " + product().label + " - " + timeText(frame.utc));
    textStatus.setText(product().label + "  " + timeText(frame.utc));
    updateLocations();
    radar->update();
}

// the lightning overlay: MRMS's cloud-to-ground strike density of the last 1, 5, 15 and 30 minutes (as far as the choice goes), each cell with a strike kept as a point of the
// shortest period it is in, so the colors give a little time series
void MrmsViewer::loadLightning() {
    static const char * ids[] = {"NLDN_CG_001min_AvgDensity", "NLDN_CG_005min_AvgDensity", "NLDN_CG_015min_AvgDensity", "NLDN_CG_030min_AvgDensity"};
    const int which = std::clamp(comboLightning.getIndex(), 0, 4);
    if (which == 0 || lightningLoading || closed) {
        if (which == 0) {
            lightningPoints.clear();
        }
        return;
    }
    lightningLoading = true;
    const int periods = which;   // 1: the last minute only ... 4: all four
    const int mine = ++lightningGeneration;
    struct Result {
        vector<Strike> points;
        QDateTime utc;
        bool ok{false};
    };
    auto result = std::make_shared<Result>();
    new FutureVoid{this,
        [periods, result] {
            vector<UtilityMrms::Product> more;
            string error;
            UtilityMrms::discoverMore(more, error);
            // the periods are separate small files on the open data server: fetched and decoded side by side
            struct Period {
                UtilityMrms::Frame frame;
                bool ok{false};
            };
            vector<std::future<Period>> pending;
            for (int period = 0; period < periods; period++) {
                const string wanted = ids[period];
                const auto found = std::find_if(more.begin(), more.end(), [&wanted] (const auto& p) { return p.id == wanted; });
                if (found == more.end()) {
                    pending.emplace_back();   // not offered: an invalid future
                    continue;
                }
                const auto product = *found;
                pending.push_back(std::async(std::launch::async, [product] {
                    Period out;
                    string e;
                    vector<UtilityMrms::Scan> list;
                    // the open data bucket on AWS first (the lightning products are on one level), the NCEP server when it gives nothing
                    bool listed = UtilityMrms::scansAws(product, "00.00", list, e);
                    if (!listed) {
                        listed = UtilityMrms::scans(product, list, e);
                    }
                    out.ok = listed && !list.empty() && UtilityMrms::frame(product, list.back(), out.frame, e);
                    return out;
                }));
            }
            std::set<std::pair<int, int>> seen;   // cells already placed by a shorter period
            for (int period = 0; period < periods; period++) {
                if (!pending[static_cast<size_t>(period)].valid()) {
                    continue;
                }
                const auto data = pending[static_cast<size_t>(period)].get();
                if (!data.ok) {
                    continue;
                }
                const auto& frame = data.frame;
                result->ok = true;
                if (period == 0) {
                    result->utc = frame.utc;
                }
                const auto cells = frame.indices();
                const auto& grid = frame.grid;
                const int firstStrike = frame.valueAt(1) > 0.0 ? 1 : 2;   // the lowest index that is a density above zero (index 1 can be zero)
                for (int row = 0; row < grid.rows; row++) {
                    const auto * line = reinterpret_cast<const uchar *>(cells.constData()) + static_cast<qsizetype>(row) * grid.columns;
                    for (int column = 0; column < grid.columns; column++) {
                        if (line[column] < firstStrike || result->points.size() >= 300000) {
                            continue;
                        }
                        if (!seen.insert({column, row}).second) {
                            continue;   // a shorter period has it
                        }
                        result->points.push_back({static_cast<float>(grid.west + (column + 0.5) * grid.cell), static_cast<float>(grid.north - (row + 0.5) * grid.cell),
                                                  static_cast<unsigned char>(period)});
                    }
                }
            }
        },
        [this, result, mine] {
            lightningLoading = false;
            if (closed || mine != lightningGeneration || !result->ok) {
                return;
            }
            lightningPoints = std::move(result->points);
            lightningTime = result->utc;
            radar->update();
        }};
}

// the value of the shown scan at each saved location ("Home: 0.82 in   Springfield: none")
void MrmsViewer::updateLocations() {
    if (labelLocations == nullptr) {
        return;
    }
    const auto names = Location::listOfNames();
    const auto places = Location::getListLatLons();
    QStringList parts;
    const auto& grid = current.grid;
    for (size_t i = 0; i < places.size() && !currentIndices.isEmpty(); i++) {
        const int column = static_cast<int>(std::floor((places[i].lon() - grid.west) / grid.cell));
        const int row = static_cast<int>(std::floor((grid.north - places[i].lat()) / grid.cell));
        if (column < 0 || column >= grid.columns || row < 0 || row >= grid.rows) {
            continue;   // outside the MRMS grid
        }
        const int index = static_cast<uchar>(currentIndices[static_cast<qsizetype>(row) * grid.columns + column]);
        const QString name = i < names.size() ? QString::fromStdString(names[i]) : QString{"Location %1"}.arg(i + 1);
        parts << name + ": " + (index == 0 ? QString{"none"} : QString::number(UtilityMrms::shown(product(), current.valueAt(index), us()), 'g', 3) + " " +
                                                           QString::fromStdString(UtilityMrms::unitsShown(product(), us())));
    }
    labelLocations->setText(parts.join("     "));
    labelLocations->setToolTip(parts.join("\n"));
    labelLocations->setVisible(!parts.isEmpty());
}

// the products by group, each group opening and closing; the group of the shown product starts open
void MrmsViewer::refreshOverlayLabel() {
    static const char * names[] = {"lightning", "chance", "hail", "rotation"};
    ComboBox * combos[] = {&comboLightning, &comboChance, &comboHail, &comboRotation};
    string text;
    for (int i = 0; i < 4; i++) {
        if (combos[i]->getIndex() > 0) {
            text += (text.empty() ? "" : ", ") + string{names[i]};
        }
    }
    buttonOverlays.setText("Overlays: " + (text.empty() ? string{"off"} : text));
}

void MrmsViewer::showOverlays() {
    if (overlayDialog) {
        overlayDialog->raise();
        overlayDialog->activateWindow();
        return;
    }
    auto * dialog = new QDialog{this};
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle("MRMS overlays");
    auto * layout = new QGridLayout{dialog};
    struct Entry {
        ComboBox * combo;
        const char * title;
    };
    const Entry entries[] = {{&comboLightning, "Lightning strikes"}, {&comboChance, "Lightning chance (contours)"}, {&comboHail, "Hail swaths (1 in and up)"},
                             {&comboRotation, "Rotation tracks"}};
    int slot = 0;
    for (const auto& entry : entries) {
        auto * box = new QGroupBox{entry.title, dialog};
        auto * inner = new QVBoxLayout{box};
        auto * group = new QButtonGroup{box};
        const auto items = entry.combo->getView();
        for (int i = 0; i < items->count(); i++) {
            auto * radio = new QRadioButton{i == 0 ? QString{"Off"} : items->itemText(i), box};
            radio->setChecked(i == entry.combo->getIndex());
            group->addButton(radio, i);
            inner->addWidget(radio);
        }
        inner->addStretch();
        ComboBox * combo = entry.combo;
        QObject::connect(group, &QButtonGroup::idClicked, box, [this, combo] (int id) {
            combo->setIndex(static_cast<size_t>(id));   // its own handler stores the pref and loads the data
            refreshOverlayLabel();
        });
        layout->addWidget(box, slot / 2, slot % 2);
        slot++;
    }
    auto * close = new QPushButton{"Close", dialog};
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::close);
    layout->addWidget(close, 2, 0, 1, 2, Qt::AlignRight);
    overlayDialog = dialog;
    dialog->show();
}

void MrmsViewer::showProductTree() {
    auto * combo = comboProduct.getView();
    auto * tree = new QTreeWidget{this};
    tree->setWindowFlags(Qt::Popup);
    tree->setAttribute(Qt::WA_DeleteOnClose);
    tree->setHeaderHidden(true);
    tree->setIndentation(14);
    tree->setRootIsDecorated(true);
    const int shownIndex = comboProduct.getIndex();
    std::vector<string> groups;
    for (const auto& p : productList) {
        if (std::find(groups.begin(), groups.end(), p.group) == groups.end()) {
            groups.push_back(p.group);
        }
    }
    for (const auto& group : groups) {
        int count = 0;
        for (const auto& p : productList) {
            count += p.group == group ? 1 : 0;
        }
        auto * top = new QTreeWidgetItem{tree, QStringList{QString::fromStdString(group) + QString{"  (%1)"}.arg(count)}};
        QFont bold = top->font(0);
        bold.setBold(true);
        top->setFont(0, bold);
        top->setData(0, Qt::UserRole, -1);
        bool holdsShown = false;
        for (size_t i = 0; i < productList.size(); i++) {
            if (productList[i].group != group) {
                continue;
            }
            auto * item = new QTreeWidgetItem{top, QStringList{QString::fromStdString(productList[i].label)}};
            item->setData(0, Qt::UserRole, static_cast<int>(i));
            if (static_cast<int>(i) == shownIndex) {
                holdsShown = true;
                QFont current = item->font(0);
                current.setBold(true);
                item->setFont(0, current);
                tree->setCurrentItem(item);
            }
        }
        top->setExpanded(holdsShown);
    }
    QObject::connect(tree, &QTreeWidget::itemClicked, tree, [this, tree] (QTreeWidgetItem * item, int) {
        const int index = item->data(0, Qt::UserRole).toInt();
        if (index < 0) {
            item->setExpanded(!item->isExpanded());   // a group: open or close it
            return;
        }
        tree->close();
        comboProduct.setIndex(static_cast<size_t>(index));
    });
    const int width = std::max(combo->width(), 380);
    tree->resize(width, std::min(520, std::max(200, height() - 120)));
    tree->move(combo->mapToGlobal(QPoint{0, combo->height()}));
    tree->show();
    tree->setFocus();
}

// one scan drawn the way it is on screen (map, lines, legend), with the information bars of the other model viewers:
// product (and units) and when this file was saved on top; valid time and what it is at the bottom
QByteArray MrmsViewer::renderFrame(const UtilityMrms::Frame& frame, const QString& units, const QString& extra) {
    setFrame(frame);
    const QImage map = radar->grab().toImage();
    // a border above and below the map (the bars are drawn into it, so nothing - legend included - is covered)
    const int bar = UtilityGrib::headerBarHeight(map.width());
    QImage image{map.width(), map.height() + 2 * bar, QImage::Format_ARGB32_Premultiplied};
    image.fill(Qt::white);
    {
        QPainter painter{&image};
        painter.drawImage(0, bar, map);
    }
    UtilityGrib::MapHeader header;
    header.topLeft = "MRMS  " + QString::fromStdString(product().label) + (units.isEmpty() ? QString{} : " (" + units + ")");
    header.topRight = "Saved " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + "Z";
    const auto local = frame.utc.toLocalTime();
    header.bottomLeft = "Valid " + frame.utc.toUTC().toString("ddd yyyy-MM-dd HH:mm") + "Z  (" + local.toString("ddd h:mm AP") + " " +
        QTimeZone::systemTimeZone().abbreviation(local) + ")";
    header.bottomRight = extra;
    UtilityGrib::drawMapHeader(image, header);
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return bytes;
}

void MrmsViewer::onSave() {
    if (currentIndices.isEmpty()) {
        return;
    }
    const bool wasLooping = looping && !loopFrames.empty();
    loopTimer.stop();
    const auto shownFrame = current;
    const auto units = QString::fromStdString(UtilityMrms::unitsShown(product(), us()));
    const bool animated = loopFrames.size() >= 2;
    vector<QByteArray> frames;
    QByteArray still;
    QDateTime first = current.utc;
    QDateTime last;
    if (animated) {
        for (size_t i = 0; i < loopFrames.size(); i += 1) {
            frames.push_back(renderFrame(loopFrames[i], units, "NOAA MRMS   frame " + QString::number(i + 1) + " of " + QString::number(loopFrames.size())));
        }
        first = loopFrames.front().utc;
        last = loopFrames.back().utc;
    } else {
        still = renderFrame(current, units, "NOAA MRMS");
    }
    setFrame(shownFrame);   // back to what was on screen
    if (wasLooping) {
        loopTimer.start();
    }
    const auto name = UtilityAnimationExport::validName(first, last, "mrms_" + UtilityAnimationExport::slug(QString::fromStdString(product().id), 40));
    UtilityAnimationExport::saveWithDialog(this, frames, 350, still, name, QByteArray{}, false);
}

void MrmsViewer::startLoop() {
    if (scans.empty()) {
        return;
    }
    const auto gen = ++generation;
    const auto chosen = product();
    const int wanted = loopCounts[std::clamp(comboLoop.getIndex(), 0, 2)];
    const int newestIndex = comboToScan.empty() ? 0 : comboToScan[static_cast<size_t>(std::max(0, comboScan.getIndex()))];
    const int first = std::max(0, newestIndex - wanted + 1);
    auto list = std::make_shared<vector<UtilityMrms::Scan>>(scans.begin() + first, scans.begin() + newestIndex + 1);
    loopFrames.clear();
    looping = true;
    buttonLoop.setText("Stop");
    // decode one scan at a time so the status can say how far along it is
    auto loadNext = std::make_shared<std::function<void(size_t)>>();
    *loadNext = [this, gen, chosen, list, loadNext] (size_t index) {
        if (closed || gen != generation || !looping) {
            return;
        }
        if (index >= list->size()) {
            loopPosition = 0;
            textStatus.setText(product().label + "  loop of " + To::string(static_cast<int>(loopFrames.size())) + " scans");
            loopTimer.start();
            return;
        }
        textStatus.setText("Loading loop frame " + To::string(static_cast<int>(index) + 1) + " of " + To::string(static_cast<int>(list->size())) + "...");
        auto result = std::make_shared<Loaded>();
        new FutureVoid{this,
            [chosen, list, index, result] { UtilityMrms::frame(chosen, (*list)[index], result->frame, result->error); },
            [this, gen, index, result, loadNext] {
                if (closed || gen != generation || !looping) {
                    return;
                }
                if (!result->error.empty()) {
                    textStatus.setText(result->error);
                    stopLoop();
                    return;
                }
                loopFrames.push_back(result->frame);
                (*loadNext)(index + 1);
            }};
    };
    (*loadNext)(0);
}

void MrmsViewer::stopLoop() {
    if (looping) {
        ++generation;   // abandons a loop that is still loading
    }
    looping = false;
    loopTimer.stop();
    buttonLoop.setText("Loop");
}

void MrmsViewer::stepLoop() {
    if (loopFrames.empty()) {
        return;
    }
    loopPosition = (loopPosition + 1) % loopFrames.size();
    // the last frame lingers a little so the end of the loop is visible
    loopTimer.setInterval(loopPosition + 1 == loopFrames.size() ? 1100 : 350);
    setFrame(loopFrames[loopPosition]);
}

// The radar projection is linear in longitude and in Mercator y (degrees); its coefficients come from three map points.
MrmsViewer::Projection2 MrmsViewer::projection() const {
    const auto& pn = radar->mapState.getPn();
    const auto project = [&pn] (double lat, double lon) {
        return Projection::computeMercatorNumbersFromLatLon(LatLon{lat, lon}.reverseLon(), pn);
    };
    const auto mercator = [] (double lat) {
        return 180.0 / std::numbers::pi * std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0));
    };
    const auto a = project(30.0, -100.0);
    const auto b = project(30.0, -99.0);
    const auto c = project(40.0, -100.0);
    const double ax = b[0] - a[0];
    const double ay = (c[1] - a[1]) / (mercator(40.0) - mercator(30.0));
    return {ax, a[0] + 100.0 * ax, ay, a[1] - ay * mercator(30.0)};
}

// Draws the scan at the grid's own resolution: every screen pixel is turned back into a latitude / longitude through
// the radar widget's projection and looked up in the grid. That projection is Mercator, so longitude depends only on the
// pixel's column and latitude only on its row - two small tables, then a plain lookup per pixel.
void MrmsViewer::paintData(QPainter& painter) {
    if (currentIndices.isEmpty()) {
        return;
    }
    drawGrid(painter, current.grid, currentIndices, currentColors, false);
    if (comboHail.getIndex() > 0 && !hailLevels.empty()) {   // the hail swaths, under the rotation tracks
        const auto [ax, bx, ay, by] = projection();
        const auto& state = radar->mapState;
        const double ratio = std::max(1.0, radar->devicePixelRatioF());
        const double degreesPerPixel = std::abs(1000.0 / (radar->width() * ratio) / state.zoom / ax);
        size_t level = 0;
        for (size_t i = 0; i < hailLevels.size(); i++) {
            if (hailLevels[i].first.cell <= degreesPerPixel * 1.5) {
                level = i;
            }
        }
        drawGrid(painter, hailLevels[level].first, hailLevels[level].second, hailColors, true);
    }
    if (comboRotation.getIndex() > 0 && !rotationLevels.empty()) {   // the rotation tracks over the product: strong rotation only, in the colors of its key
        // the coarsest level whose cell is no bigger than a pixel of the map: each pixel shows the strongest rotation under it
        const auto [ax, bx, ay, by] = projection();
        const auto& state = radar->mapState;
        const double ratio = std::max(1.0, radar->devicePixelRatioF());
        const double degreesPerPixel = std::abs(1000.0 / (radar->width() * ratio) / state.zoom / ax);
        size_t level = 0;
        for (size_t i = 0; i < rotationLevels.size(); i++) {
            if (rotationLevels[i].first.cell <= degreesPerPixel * 1.5) {
                level = i;
            }
        }
        drawGrid(painter, rotationLevels[level].first, rotationLevels[level].second, rotationColors, true);
    }
}

// a grid of 8-bit indices drawn through a colour table over the map: one image pixel per device pixel of the map widget. Where each pixel is comes from the map's own
// state (the same arithmetic as the hover read-out), not from the painter: a grab for a picture paints through a device whose transform and ratio differ from the screen's
void MrmsViewer::drawGrid(QPainter& painter, const UtilityMrms::Grid& grid, const QByteArray& indices, const QVector<QRgb>& colors, bool outline) {
    const auto [ax, bx, ay, by] = projection();
    const auto& state = radar->mapState;
    const double ratio = std::max(1.0, radar->devicePixelRatioF());
    const int pixelWidth = static_cast<int>(std::lround(radar->width() * ratio));
    const int pixelHeight = static_cast<int>(std::lround(radar->height() * ratio));
    const auto projectedX = [&] (double px) { return (px * 1000.0 / pixelWidth - 500.0 - state.xPos) / state.zoom; };
    const auto projectedY = [&] (double py) { return (py * 1000.0 / pixelHeight - 250.0 - state.yPos) / state.zoom; };
    vector<int> columnOf(static_cast<size_t>(pixelWidth));
    for (int i = 0; i < pixelWidth; i += 1) {
        const double x = projectedX(i + 0.5);
        const double lon = (x - bx) / ax;
        const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
        columnOf[static_cast<size_t>(i)] = (column >= 0 && column < grid.columns) ? column : -1;
    }
    QImage image{pixelWidth, pixelHeight, QImage::Format_ARGB32_Premultiplied};
    const auto * cells = reinterpret_cast<const uchar *>(indices.constData());
    for (int j = 0; j < pixelHeight; j += 1) {
        const double y = projectedY(j + 0.5);
        const double lat = std::atan(std::sinh((y - by) / ay * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
        const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
        if (row < 0 || row >= grid.rows) {
            std::fill(out, out + pixelWidth, qRgba(0, 0, 0, 0));
            continue;
        }
        const auto * source = cells + static_cast<qsizetype>(row) * grid.columns;
        for (int i = 0; i < pixelWidth; i += 1) {
            const int column = columnOf[static_cast<size_t>(i)];
            out[i] = column < 0 ? qRgba(0, 0, 0, 0) : colors[source[column]];
        }
    }
    if (outline) {   // an overlay over the colors of a product: its shapes edged in black, so they read whatever is under them
        const QImage filled = image.copy();
        const int edge = std::max(1, static_cast<int>(std::lround(ratio)));
        for (int j = 0; j < pixelHeight; j++) {
            auto * out = reinterpret_cast<QRgb *>(image.scanLine(j));
            const auto * in = reinterpret_cast<const QRgb *>(filled.constScanLine(j));
            for (int i = 0; i < pixelWidth; i++) {
                if (qAlpha(in[i]) == 0) {
                    continue;
                }
                bool border = false;
                for (int d = 1; d <= edge && !border; d++) {
                    border = i - d < 0 || i + d >= pixelWidth || j - d < 0 || j + d >= pixelHeight || qAlpha(in[i - d]) == 0 || qAlpha(in[i + d]) == 0 ||
                        qAlpha(reinterpret_cast<const QRgb *>(filled.constScanLine(j - d))[i]) == 0 || qAlpha(reinterpret_cast<const QRgb *>(filled.constScanLine(j + d))[i]) == 0;
                }
                if (border) {
                    out[i] = qRgba(0, 0, 0, 255);
                }
            }
        }
    }
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.drawImage(QRectF{QPointF{projectedX(0.0), projectedY(0.0)}, QPointF{projectedX(pixelWidth), projectedY(pixelHeight)}}, image, QRectF{image.rect()});
}

namespace {
    // the rotation key: 1e-3 / s of azimuthal shear, nothing under 8 (the weak shear of most storms and of clutter), then strong to very strong rotation (supercells)
    struct RotationBand {
        double from;
        QColor color;
        const char * label;
    };
    const RotationBand rotationBands[] = {{8.0, QColor{255, 255, 255, 235}, "8"}, {12.0, QColor{120, 230, 255, 245}, "12"}, {16.0, QColor{40, 120, 255, 250}, "16"},
                                          {20.0, QColor{150, 40, 255, 255}, "20+"}};
}


// the lightning chance as contour lines: the newest scan of the next 30 / 60 minute probability, taken at 5 x 5 cell blocks and traced at four levels (marching squares)
void MrmsViewer::loadChance() {
    static const char * ids[] = {"", "LightningProbabilityNext30min", "LightningProbabilityNext60min"};
    const int which = std::clamp(comboChance.getIndex(), 0, 2);
    if (which == 0 || chanceLoading || closed) {
        if (which == 0) {
            chanceLines.clear();
        }
        return;
    }
    chanceLoading = true;
    const string wanted = ids[which];
    const int mine = ++chanceGeneration;
    auto lines = std::make_shared<vector<ChanceLine>>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [wanted, lines, ok] {
            vector<UtilityMrms::Product> list = UtilityMrms::products();
            string error;
            if (std::none_of(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; })) {
                vector<UtilityMrms::Product> more;
                UtilityMrms::discoverMore(more, error);
                list.insert(list.end(), more.begin(), more.end());
            }
            const auto found = std::find_if(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; });
            if (found == list.end()) {
                return;
            }
            vector<UtilityMrms::Scan> scans;
            bool listed = UtilityMrms::scansAws(*found, "Grid_scale_1", scans, error);
            if (!listed) {
                listed = UtilityMrms::scans(*found, scans, error);
            }
            UtilityMrms::Frame frame;
            if (!listed || scans.empty() || !UtilityMrms::frame(*found, scans.back(), frame, error)) {
                return;
            }
            *ok = true;
            const int block = 5;
            const auto& grid = frame.grid;
            const int columns = grid.columns / block, rows = grid.rows / block;
            const auto cells = frame.indices();
            const auto * in = reinterpret_cast<const uchar *>(cells.constData());
            vector<float> field(static_cast<size_t>(columns) * rows, 0.0f);   // the largest chance in each block
            for (int row = 0; row < rows; row++) {
                for (int column = 0; column < columns; column++) {
                    int best = 0;
                    for (int dr = 0; dr < block; dr++) {
                        const auto * line = in + static_cast<qsizetype>(row * block + dr) * grid.columns + column * block;
                        for (int dc = 0; dc < block; dc++) {
                            best = std::max<int>(best, line[dc]);
                        }
                    }
                    field[static_cast<size_t>(row) * columns + column] = best == 0 ? 0.0f : static_cast<float>(frame.valueAt(best));
                }
            }
            const double cell = grid.cell * block;
            const auto lon = [&] (double c) { return static_cast<float>(grid.west + (c + 0.5) * cell); };
            const auto lat = [&] (double r) { return static_cast<float>(grid.north - (r + 0.5) * cell); };
            for (const int level : {10, 30, 50, 70}) {
                const float t = static_cast<float>(level);
                for (int row = 0; row + 1 < rows; row++) {
                    for (int column = 0; column + 1 < columns; column++) {
                        const float a = field[static_cast<size_t>(row) * columns + column], b = field[static_cast<size_t>(row) * columns + column + 1];
                        const float d = field[static_cast<size_t>(row + 1) * columns + column], c = field[static_cast<size_t>(row + 1) * columns + column + 1];
                        const int code = (a >= t ? 1 : 0) | (b >= t ? 2 : 0) | (c >= t ? 4 : 0) | (d >= t ? 8 : 0);
                        if (code == 0 || code == 15) {
                            continue;
                        }
                        // the crossing of each edge (top, right, bottom, left), by linear interpolation
                        const auto cross = [t] (float v1, float v2) { return v1 == v2 ? 0.5f : std::clamp((t - v1) / (v2 - v1), 0.0f, 1.0f); };
                        const double x0 = column, y0 = row;
                        const double topX = x0 + cross(a, b), topY = y0;
                        const double rightX = x0 + 1, rightY = y0 + cross(b, c);
                        const double bottomX = x0 + cross(d, c), bottomY = y0 + 1;
                        const double leftX = x0, leftY = y0 + cross(a, d);
                        const auto add = [&] (double x1, double y1, double x2, double y2) {
                            lines->push_back({static_cast<unsigned char>(level), lon(x1), lat(y1), lon(x2), lat(y2)});
                        };
                        switch (code) {
                            case 1: case 14: add(leftX, leftY, topX, topY); break;
                            case 2: case 13: add(topX, topY, rightX, rightY); break;
                            case 3: case 12: add(leftX, leftY, rightX, rightY); break;
                            case 4: case 11: add(rightX, rightY, bottomX, bottomY); break;
                            case 6: case 9: add(topX, topY, bottomX, bottomY); break;
                            case 7: case 8: add(leftX, leftY, bottomX, bottomY); break;
                            case 5: add(leftX, leftY, topX, topY); add(rightX, rightY, bottomX, bottomY); break;
                            case 10: add(topX, topY, rightX, rightY); add(leftX, leftY, bottomX, bottomY); break;
                            default: break;
                        }
                    }
                }
            }
        },
        [this, lines, ok, mine] {
            chanceLoading = false;
            if (closed || mine != chanceGeneration || !*ok) {
                return;
            }
            chanceLines = std::move(*lines);
            radar->update();
        }};
}

namespace {
    // the hail key: MESH in mm (inches in the label): at least marginally severe (1 in), then golf ball, baseball, softball
    struct HailBand {
        double from;
        QColor color;
        const char * label;
    };
    const HailBand hailBands[] = {{25.4, QColor{255, 240, 0, 245}, "1"}, {44.5, QColor{255, 150, 0, 250}, "1.75"},
                                  {63.5, QColor{255, 30, 30, 255}, "2.5"}, {101.6, QColor{255, 0, 255, 255}, "4+"}};
}

// the hail overlay: the newest scan of MRMS's MESH maximum of the chosen period, at least marginally severe size (1 in) and up
void MrmsViewer::loadHail() {
    static const char * ids[] = {"", "MESH_Max_30min", "MESH_Max_60min", "MESH_Max_120min", "MESH_Max_360min", "MESH_Max_1440min"};
    const int which = std::clamp(comboHail.getIndex(), 0, 5);
    if (which == 0 || hailLoading || closed) {
        if (which == 0) {
            hailLevels.clear();
        }
        return;
    }
    hailLoading = true;
    const string wanted = ids[which];
    const int mine = ++hailGeneration;
    auto frame = std::make_shared<UtilityMrms::Frame>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [wanted, frame, ok] {
            vector<UtilityMrms::Product> list = UtilityMrms::products();
            const auto found = std::find_if(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; });
            if (found == list.end()) {
                return;
            }
            vector<UtilityMrms::Scan> scans;
            string error;
            bool listed = UtilityMrms::scansAws(*found, "00.50", scans, error);
            if (!listed) {
                listed = UtilityMrms::scans(*found, scans, error);
            }
            *ok = listed && !scans.empty() && UtilityMrms::frame(*found, scans.back(), *frame, error);
        },
        [this, frame, ok, mine] {
            hailLoading = false;
            if (closed || mine != hailGeneration || !*ok) {
                return;
            }
            // only 1 in and up (25.4 mm): the indices rise with the value, so the first index at or above it is the cut
            int first = 256;
            for (int index = 1; index < 256; index++) {
                if (frame->valueAt(index) >= hailBands[0].from) {
                    first = index;
                    break;
                }
            }
            QByteArray cut = frame->indices();
            auto * cells = reinterpret_cast<uchar *>(cut.data());
            for (qsizetype i = 0; i < cut.size(); i++) {
                if (cells[i] < first) {
                    cells[i] = 0;
                }
            }
            hailLevels.clear();
            hailLevels.emplace_back(frame->grid, cut);
            for (const int factor : {4, 16}) {
                const auto& fine = frame->grid;
                UtilityMrms::Grid coarse = fine;
                coarse.columns = (fine.columns + factor - 1) / factor;
                coarse.rows = (fine.rows + factor - 1) / factor;
                coarse.cell = fine.cell * factor;
                QByteArray pooled(static_cast<qsizetype>(coarse.columns) * coarse.rows, 0);
                auto * dst = reinterpret_cast<uchar *>(pooled.data());
                const auto * src = reinterpret_cast<const uchar *>(cut.constData());
                for (int row = 0; row < fine.rows; row++) {
                    const auto * line = src + static_cast<qsizetype>(row) * fine.columns;
                    auto * target = dst + static_cast<qsizetype>(row / factor) * coarse.columns;
                    for (int column = 0; column < fine.columns; column++) {
                        if (line[column] > target[column / factor]) {
                            target[column / factor] = line[column];
                        }
                    }
                }
                hailLevels.emplace_back(coarse, pooled);
            }
            hailColors = QVector<QRgb>(256, qRgba(0, 0, 0, 0));
            for (int index = 1; index < 256; index++) {
                const double value = frame->valueAt(index);
                for (const auto& band : hailBands) {
                    if (value >= band.from) {
                        hailColors[index] = qPremultiply(band.color.rgba());
                    }
                }
            }
            radar->update();
        }};
}

// the rotation overlay: the newest scan of MRMS's low-level (0-2 km) or mid-level (3-6 km) rotation track over the chosen period
void MrmsViewer::loadRotation() {
    static const char * ids[] = {"", "RotationTrack30min", "RotationTrack60min", "RotationTrack120min", "RotationTrack360min", "RotationTrack1440min",
                                 "RotationTrackML60min", "RotationTrackML120min", "RotationTrackML360min"};
    const int which = std::clamp(comboRotation.getIndex(), 0, 8);
    if (which == 0 || rotationLoading || closed) {
        if (which == 0) {
            rotationIndices.clear();
            rotationLevels.clear();
        }
        return;
    }
    rotationLoading = true;
    const string wanted = ids[which];
    const int mine = ++rotationGeneration;
    auto frame = std::make_shared<UtilityMrms::Frame>();
    auto cleaned = std::make_shared<QByteArray>();
    auto ok = std::make_shared<bool>(false);
    new FutureVoid{this,
        [wanted, frame, cleaned, ok] {
            vector<UtilityMrms::Product> list = UtilityMrms::products();
            vector<UtilityMrms::Product> more;
            string error;
            if (std::none_of(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; })) {
                UtilityMrms::discoverMore(more, error);
                list.insert(list.end(), more.begin(), more.end());
            }
            const auto found = std::find_if(list.begin(), list.end(), [&wanted] (const auto& p) { return p.id == wanted; });
            if (found == list.end()) {
                return;
            }
            vector<UtilityMrms::Scan> scans;
            bool listed = UtilityMrms::scansAws(*found, "00.50", scans, error);   // the open data bucket first
            if (!listed) {
                listed = UtilityMrms::scans(*found, scans, error);
            }
            *ok = listed && !scans.empty() && UtilityMrms::frame(*found, scans.back(), *frame, error);
            if (!*ok) {
                return;
            }
            // strong rotation only where the radars see well: MRMS's radar quality index (0 to 1) masks the beam-blocked and far-range areas, where clutter and the
            // edges of the coverage make false rotation (Canada, the mountains, the coasts)
            *cleaned = frame->indices();
            UtilityMrms::Frame quality;
            bool haveQuality = false;
            {
                const auto rqi = std::find_if(list.begin(), list.end(), [] (const auto& p) { return p.id == "RadarQualityIndex"; });
                UtilityMrms::Product product;
                if (rqi != list.end()) {
                    product = *rqi;
                } else {
                    product.id = "RadarQualityIndex";
                    product.label = "Radar quality index";
                    product.validMin = -1.0;
                    product.hi = 1.0;
                    product.autoRange = true;
                    product.stops = {{0.0, 0, 0, 0}, {1.0, 255, 255, 255}};
                }
                vector<UtilityMrms::Scan> qualityScans;
                string e;
                bool qualityListed = UtilityMrms::scansAws(product, "00.00", qualityScans, e);
                if (!qualityListed) {
                    qualityListed = UtilityMrms::scans(product, qualityScans, e);
                }
                haveQuality = qualityListed && !qualityScans.empty() && UtilityMrms::frame(product, qualityScans.back(), quality, e) && quality.grid.columns > 0;
            }
            const auto& grid = frame->grid;
            auto * out = reinterpret_cast<uchar *>(cleaned->data());
            int firstStrong = 256;
            for (int index = 1; index < 256; index++) {
                if (frame->valueAt(index) >= rotationBands[0].from) {
                    firstStrong = index;
                    break;
                }
            }
            const QByteArray qualityCells = haveQuality ? quality.indices() : QByteArray{};
            const auto * good = reinterpret_cast<const uchar *>(qualityCells.constData());
            const auto& q = quality.grid;   // the quality grid is coarser (0.01 against the tracks' 0.005 degree): the cell under each track cell
            vector<int> qualityColumn(static_cast<size_t>(grid.columns), -1);
            for (int column = 0; column < grid.columns && haveQuality; column++) {
                const int c = static_cast<int>(std::floor((grid.west + (column + 0.5) * grid.cell - q.west) / q.cell));
                qualityColumn[static_cast<size_t>(column)] = c >= 0 && c < q.columns ? c : -1;
            }
            // north of the border the grid has Canada's radars and its edges: their rotation is mostly clutter, so only the United States side is kept (the border taken
            // as a few straight pieces: 49N in the west, the Lakes, the St. Lawrence and Maine)
            const auto borderLat = [] (double lon) {
                if (lon < -95.2) return 49.05;
                if (lon < -89.0) return 49.4;
                if (lon < -84.5) return 48.3;
                if (lon < -79.0) return 46.0;
                if (lon < -74.7) return 45.05;
                if (lon < -71.0) return 45.3;
                if (lon < -67.8) return 47.5;
                return 0.0;   // New Brunswick and the sea east of Maine
            };
            for (int row = 0; row < grid.rows; row++) {
                const int qr = haveQuality ? static_cast<int>(std::floor((q.north - (grid.north - (row + 0.5) * grid.cell)) / q.cell)) : -1;
                const double lat = grid.north - (row + 0.5) * grid.cell;
                auto * line = out + static_cast<qsizetype>(row) * grid.columns;
                for (int column = 0; column < grid.columns; column++) {
                    if (line[column] >= firstStrong && lat > borderLat(grid.west + (column + 0.5) * grid.cell)) {
                        line[column] = 0;
                        continue;
                    }
                    if (line[column] < firstStrong) {
                        line[column] = 0;
                    } else if (haveQuality) {
                        const int qc = qualityColumn[static_cast<size_t>(column)];
                        const int index = qr >= 0 && qr < q.rows && qc >= 0 ? good[static_cast<qsizetype>(qr) * q.columns + qc] : 0;
                        if (index == 0 || quality.valueAt(index) < 0.6) {
                            line[column] = 0;
                        }
                    }
                }
            }
        },
        [this, frame, cleaned, ok, mine] {
            rotationLoading = false;
            if (closed || mine != rotationGeneration || !*ok) {
                return;
            }
            rotationFrame = *frame;
            rotationIndices = *cleaned;
            rotationLevels.clear();
            rotationLevels.emplace_back(frame->grid, *cleaned);
            for (const int factor : {4, 16}) {   // blocks of the finest grid, keeping the largest index (the indices rise with the value)
                const auto& fine = frame->grid;
                UtilityMrms::Grid coarse = fine;
                coarse.columns = (fine.columns + factor - 1) / factor;
                coarse.rows = (fine.rows + factor - 1) / factor;
                coarse.cell = fine.cell * factor;
                QByteArray pooled(static_cast<qsizetype>(coarse.columns) * coarse.rows, 0);
                auto * dst = reinterpret_cast<uchar *>(pooled.data());
                const auto * src = reinterpret_cast<const uchar *>(cleaned->constData());
                for (int row = 0; row < fine.rows; row++) {
                    const auto * line = src + static_cast<qsizetype>(row) * fine.columns;
                    auto * target = dst + static_cast<qsizetype>(row / factor) * coarse.columns;
                    for (int column = 0; column < fine.columns; column++) {
                        if (line[column] > target[column / factor]) {
                            target[column / factor] = line[column];
                        }
                    }
                }
                rotationLevels.emplace_back(coarse, pooled);
            }
            rotationColors = QVector<QRgb>(256, qRgba(0, 0, 0, 0));
            for (int index = 1; index < 256; index++) {
                const double value = frame->valueAt(index);
                for (const auto& band : rotationBands) {
                    if (value >= band.from) {
                        rotationColors[index] = qPremultiply(band.color.rgba());
                    }
                }
            }
            radar->update();
        }};
}

bool MrmsViewer::eventFilter(QObject * object, QEvent * event) {
    if (object == comboProduct.getView()) {   // the product list: the grouped tree
        if (event->type() == QEvent::MouseButtonPress && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
            showProductTree();
            return true;
        }
        if (event->type() == QEvent::KeyPress) {
            const auto * key = static_cast<QKeyEvent *>(event);
            if (key->key() == Qt::Key_F4 || key->key() == Qt::Key_Space || (key->key() == Qt::Key_Down && (key->modifiers() & Qt::AltModifier))) {
                showProductTree();
                return true;
            }
        }
        return false;
    }
    if (object == radar && (event->type() == QEvent::MouseMove || event->type() == QEvent::MouseButtonPress)) {
        pointer = static_cast<QMouseEvent *>(event)->position();
        pointerInside = true;
        if (event->type() == QEvent::MouseMove) {
            showHover(pointer);
        }
    } else if (object == radar && event->type() == QEvent::Wheel) {
        pointer = static_cast<QWheelEvent *>(event)->position();
        pointerInside = true;
    } else if (object == radar && event->type() == QEvent::Leave) {
        pointerInside = false;
        hoverShown = false;
        if (hoverLabel != nullptr) {
            hoverLabel->hide();
        }
        radar->update();
    }
    return false;
}

// the value under the pointer: widget pixel -> window units -> projected coordinates -> latitude / longitude -> grid cell
void MrmsViewer::showHover(const QPointF& widgetPos) {
    if (currentIndices.isEmpty() || looping) {
        return;
    }
    const auto [ax, bx, ay, by] = projection();
    const auto& grid = current.grid;
    const auto& state = radar->mapState;
    const double u = widgetPos.x() * 1000.0 / radar->width() - 500.0;
    const double v = widgetPos.y() * 1000.0 / radar->height() - 250.0;
    const double x = (u - state.xPos) / state.zoom;
    const double y = (v - state.yPos) / state.zoom;
    const double lon = (x - bx) / ax;
    const double lat = std::atan(std::sinh((y - by) / ay * std::numbers::pi / 180.0)) * 180.0 / std::numbers::pi;
    const int column = static_cast<int>(std::floor((lon - grid.west) / grid.cell));
    const int row = static_cast<int>(std::floor((grid.north - lat) / grid.cell));
    // the popup: coordinates, then the value (the same two lines as the other map viewers)
    QString text = QString::number(std::abs(lat), 'f', 2) + QChar{0x00B0} + (lat >= 0 ? " N" : " S") + "   " +
        QString::number(std::abs(lon), 'f', 2) + QChar{0x00B0} + (lon < 0 ? " W" : " E") + "\n";
    if (column >= 0 && column < grid.columns && row >= 0 && row < grid.rows) {
        const int index = static_cast<uchar>(currentIndices[static_cast<qsizetype>(row) * grid.columns + column]);
        text += index == 0 ? QString{"no data"} : QString::number(UtilityMrms::shown(product(), current.valueAt(index), us()), 'g', 4) + " " +
            QString::fromStdString(UtilityMrms::unitsShown(product(), us()));
    } else {
        text += "outside the MRMS grid";
    }
    hoverShown = true;
    if (hoverLabel != nullptr) {
        hoverLabel->setText(text);
        hoverLabel->adjustSize();
        hoverLabel->move(12, 12);
        hoverLabel->show();
        hoverLabel->raise();
    }
    radar->update();
}

void MrmsViewer::paintLegend(QPainter& painter) {
    if (currentIndices.isEmpty()) {
        return;
    }
    if (!lightningPoints.empty() && comboLightning.getIndex() > 0) {   // the strikes, a small cross at each cell (the map's window units, as the crosshair is drawn), the oldest first
        const auto [ax, bx, ay, by] = projection();
        const auto& state = radar->mapState;
        const double arm = 2.6 * 1000.0 / radar->width();
        static const QColor colors[4] = {QColor{255, 255, 255}, QColor{255, 235, 40}, QColor{255, 150, 20}, QColor{235, 40, 40}};
        static const char * names[4] = {"last minute", "1-5 min", "5-15 min", "15-30 min"};
        const int periods = comboLightning.getIndex();
        for (int age = periods - 1; age >= 0; age--) {
            for (const auto& pen : {QPen{QColor{0, 0, 0, 170}, 3.2}, QPen{colors[age], 1.5}}) {
                painter.setPen(pen);
                for (const auto& strike : lightningPoints) {
                    if (strike.age != age) {
                        continue;
                    }
                    const double mercator = std::log(std::tan(std::numbers::pi / 4.0 + strike.lat * std::numbers::pi / 360.0)) * 180.0 / std::numbers::pi;
                    const double u = (ax * strike.lon + bx) * state.zoom + state.xPos;
                    const double v = (ay * mercator + by) * state.zoom + state.yPos;
                    if (u < -520.0 || u > 520.0 || v < -270.0 || v > 770.0) {
                        continue;
                    }
                    painter.drawLine(QPointF{u - arm, v}, QPointF{u + arm, v});
                    painter.drawLine(QPointF{u, v - arm}, QPointF{u, v + arm});
                }
            }
        }
        {   // the key: the colors by age
            QFont keyFont{painter.font()};
            keyFont.setPointSizeF(9.0);
            painter.setFont(keyFont);
            double x = -490.0;
            const double y = -236.0;
            painter.fillRect(QRectF{-496.0, y - 12.0, 20.0 + 100.0 * periods, 20.0}, QColor{0, 0, 0, 170});
            for (int age = 0; age < periods; age++) {
                painter.setPen(QPen{colors[age], 2.0});
                painter.drawLine(QPointF{x, y}, QPointF{x + 8.0, y});
                painter.drawLine(QPointF{x + 4.0, y - 4.0}, QPointF{x + 4.0, y + 4.0});
                painter.setPen(Qt::white);
                painter.drawText(QPointF{x + 12.0, y + 4.0}, names[age]);
                x += 100.0;
            }
        }
    }
    if (comboChance.getIndex() > 0 && !chanceLines.empty()) {   // the lightning chance: contour lines at 10, 30, 50 and 70 percent
        const auto [ax, bx, ay, by] = projection();
        const auto& state = radar->mapState;
        static const QColor colors[4] = {QColor{160, 220, 255}, QColor{255, 240, 90}, QColor{255, 150, 30}, QColor{255, 70, 70}};
        const auto mercator = [] (double lat) { return std::log(std::tan(std::numbers::pi / 4.0 + lat * std::numbers::pi / 360.0)) * 180.0 / std::numbers::pi; };
        for (int pass = 0; pass < 2; pass++) {   // a dark edge under the colour
            for (int level = 0; level < 4; level++) {
                painter.setPen(pass == 0 ? QPen{QColor{0, 0, 0, 190}, 3.0} : QPen{colors[level], 1.3});
                for (const auto& line : chanceLines) {
                    if (line.level != static_cast<unsigned char>(10 + 20 * level)) {
                        continue;
                    }
                    const double u1 = (ax * line.lon1 + bx) * state.zoom + state.xPos, v1 = (ay * mercator(line.lat1) + by) * state.zoom + state.yPos;
                    const double u2 = (ax * line.lon2 + bx) * state.zoom + state.xPos, v2 = (ay * mercator(line.lat2) + by) * state.zoom + state.yPos;
                    if ((u1 < -520.0 && u2 < -520.0) || (u1 > 520.0 && u2 > 520.0) || (v1 < -270.0 && v2 < -270.0) || (v1 > 770.0 && v2 > 770.0)) {
                        continue;
                    }
                    painter.drawLine(QPointF{u1, v1}, QPointF{u2, v2});
                }
            }
        }
        QFont keyFont{painter.font()};
        keyFont.setPointSizeF(9.0);
        painter.setFont(keyFont);
        const double y = -192.0;
        painter.fillRect(QRectF{-496.0, y - 12.0, 300.0, 20.0}, QColor{0, 0, 0, 170});
        painter.setPen(Qt::white);
        painter.drawText(QPointF{-490.0, y + 4.0}, comboChance.getIndex() == 1 ? "Lightning, next 30 min:" : "Lightning, next 60 min:");
        double x = -355.0;
        static const char * names[4] = {"10%", "30%", "50%", "70%"};
        for (int level = 0; level < 4; level++) {
            painter.setPen(QPen{colors[level], 2.0});
            painter.drawLine(QPointF{x, y}, QPointF{x + 14.0, y});
            painter.setPen(Qt::white);
            painter.drawText(QPointF{x + 18.0, y + 4.0}, names[level]);
            x += 50.0;
        }
    }
    if (comboHail.getIndex() > 0 && !hailLevels.empty()) {   // the hail key, under the rotation one
        QFont keyFont{painter.font()};
        keyFont.setPointSizeF(9.0);
        painter.setFont(keyFont);
        const double y = -236.0 + 22.0 * 3;
        painter.fillRect(QRectF{-496.0, y - 12.0, 300.0, 20.0}, QColor{0, 0, 0, 170});
        painter.setPen(Qt::white);
        painter.drawText(QPointF{-490.0, y + 4.0}, "Hail size (in):");
        double x = -400.0;
        for (const auto& band : hailBands) {
            painter.fillRect(QRectF{x, y - 6.0, 12.0, 12.0}, QColor{band.color.red(), band.color.green(), band.color.blue()});
            painter.setPen(Qt::white);
            painter.drawText(QPointF{x + 15.0, y + 4.0}, band.label);
            x += 48.0;
        }
    }
    if (comboRotation.getIndex() > 0 && !rotationLevels.empty()) {   // the rotation key, under the lightning one
        QFont keyFont{painter.font()};
        keyFont.setPointSizeF(9.0);
        painter.setFont(keyFont);
        const double y = -214.0;
        painter.fillRect(QRectF{-496.0, y - 12.0, 300.0, 20.0}, QColor{0, 0, 0, 170});
        painter.setPen(Qt::white);
        painter.drawText(QPointF{-490.0, y + 4.0}, comboRotation.getIndex() >= 6 ? "Rotation 3-6 km:" : "Rotation 0-2 km:");
        double x = -380.0;
        for (const auto& band : rotationBands) {
            painter.fillRect(QRectF{x, y - 6.0, 12.0, 12.0}, QColor{band.color.red(), band.color.green(), band.color.blue()});
            painter.setPen(Qt::white);
            painter.drawText(QPointF{x + 15.0, y + 4.0}, band.label);
            x += 42.0;
        }
    }
    if (hoverShown && !looping) {
        // the crosshair at the pointer (window units: the map is drawn on a 1000 x 1000 window)
        const double u = pointer.x() * 1000.0 / radar->width() - 500.0;
        const double v = pointer.y() * 1000.0 / radar->height() - 250.0;
        const double arm = 16.0 * 1000.0 / radar->width();
        const double gap = 4.0 * 1000.0 / radar->width();
        for (const auto& pen : {QPen{QColor{0, 0, 0, 200}, 3.5}, QPen{QColor{255, 255, 255}, 1.5}}) {
            painter.setPen(pen);
            painter.drawLine(QPointF{u - arm, v}, QPointF{u - gap, v});
            painter.drawLine(QPointF{u + gap, v}, QPointF{u + arm, v});
            painter.drawLine(QPointF{u, v - arm}, QPointF{u, v - gap});
            painter.drawLine(QPointF{u, v + gap}, QPointF{u, v + arm});
        }
    }
    const auto& stops = product().stops;
    // an automatic scale spans the scan's own range, so its stop values come from the scan
    const double lo = current.validMin;
    const double span = current.step * 254.0;
    const double boxWidth = 38.0;
    const double x0 = -490.0;
    const double y0 = 715.0;
    QFont font{painter.font()};
    font.setPointSizeF(9.0);
    painter.setFont(font);
    for (size_t i = 0; i < stops.size(); i += 1) {
        const double value = product().autoRange ? lo + stops[i].value * span : stops[i].value;
        const QRectF box{x0 + static_cast<double>(i) * boxWidth, y0, boxWidth, 14.0};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{stops[i].r, stops[i].g, stops[i].b});
        painter.drawRect(box);
        painter.setPen(Qt::white);
        painter.drawText(QRectF{box.left(), y0 + 14.0, boxWidth, 16.0}, Qt::AlignCenter,
                         QString::number(UtilityMrms::shown(product(), value, us()), 'g', 3));
    }
    painter.drawText(QPointF{x0 + static_cast<double>(stops.size()) * boxWidth + 8.0, y0 + 11.0},
                     QString::fromStdString(UtilityMrms::unitsShown(product(), us())));
}
