// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ui/Toolbar.h"
#include <QPolygonF>
#include <QPixmap>
#include <QPainter>
#include "ui/ToolbarGroups.h"
#include <algorithm>
#include <string>
#include <vector>
#include <QAction>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include "common/GlobalVariables.h"
#include "misc/Hourly.h"
#include "misc/ObservationSites.h"
#include "misc/Observations.h"
#include "obs/SurfaceViewer.h"
#include "mapkit/MasterMapViewer.h"
#include "misc/Opc.h"
#include "misc/Rtma.h"
#include "misc/SevereDashboard.h"
#include "misc/UsAlerts.h"
#include "misc/WfoText.h"
#include "drought/DroughtViewer.h"
#include "models/CamsViewer.h"
#include "mrms/MrmsViewer.h"
#include "models/GribViewer.h"
#include "models/IndexViewer.h"
#include "models/RefsViewer.h"
#include "spcrefs/SpcRefsViewer.h"
#include "climate/ClimateViewer.h"
#include "rivers/RiverMapViewer.h"
#include "dashboard/Dashboards.h"
#include "hurricane/HurricaneViewer.h"
#include "tropical/TropicalViewer.h"
#include "models/SpcPostViewer.h"
#include "models/ModelViewer.h"
#include "nhc/Nhc.h"
#include "objects/Route.h"
#include "objects/WString.h"
#include "radar/RadarMosaic.h"
#include "settings/SettingsMain.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"
#include "spc/SpcCompMap.h"
#include "spc/SpcFireSummary.h"
#include "spc/SpcMeso.h"
#include "models/SoundingViewer.h"
#include "spc/SpcStormReports.h"
#include "spc/SpcSwoDay1.h"
#include "spc/SpcSwoSummary.h"
#include "spc/SpcTstormOutlooks.h"
#include "spc/SvrComparison.h"
#include "util/To.h"
#include "vis/GoesGlobal.h"
#include "vis/GoesViewer.h"
#include "wpc/NationalImages.h"
#include "wpc/NationalText.h"
#include "wpc/RainfallOutlookSummary.h"

using std::string;

Toolbar::Toolbar(Window * parent, const function<void()>& reloadFn)
    : parent{parent}
    , reloadFn{reloadFn}
    , autoUpdate{parent, "MAIN_SCREEN_DATA_REFRESH_INTERVAL", 10, reloadFn}
{
    // routeItems.emplace_back("reload.png", "Reload data, Ctrl-u", [reloadFn] { reloadFn(); });
    routeItems.emplace_back("baseline_settings_black_48dp.png", "Settings", [this] { launchSettings(); });

    routeItems.emplace_back("baseline_warning_black_48dp.png", "Severe Dashboard, Ctrl-d", [this] { launchSevereDashboard(); });
    routeItems.emplace_back("baseline_cloud_black_48dp.png", "GOES imagery, Ctrl-c", [this] { launchGoesViewer(); });
    routeItems.emplace_back("baseline_date_range_black_48dp.png", "Hourly Forecast, Ctrl-h", [this] { launchHourly(); });
    routeItems.emplace_back("baseline_info_black_48dp.png", "WFO Text products, Ctrl-a", [this] { launchWfoText(); });


    routeItems.emplace_back("grib.png", "RRFS GRIB Viewer", [parent] { new GribViewer{parent}; });
    routeItems.emplace_back("refs.png", "REFS Ensemble Viewer (4-panel mean/spread comparison)", [parent] { new RefsViewer{parent}; });
    routeItems.emplace_back("refs.png", "SPC REFS (SPC's ensemble products: probabilities, paintballs, updraft helicity)", [parent] { new SpcRefsViewer{parent}; });
    routeItems.emplace_back("nsslwrf.png", "NSSL CAMs (experimental convection-allowing models: MPAS, WRF, HRRR, RRFS)", [parent] { new CamsViewer{parent}; });
    routeItems.emplace_back("tor.png", "SPC Post Slideshow (thunder / severe / lightning probability)", [parent] { new SpcPostViewer{parent}; });
    routeItems.emplace_back("tstorm.png", "Parametric Index Viewer (SHIP hail parameter)", [parent] { new IndexViewer{parent}; });

    routeItems.emplace_back("spc_sum.png", "SPC Convective Outlook Summary, Ctrl-s", [this] { launchSpcSwoSummary(); });
    for (const int day : {1, 2, 3, 48}) {
        routeItems.emplace_back("day" + To::string(day) + ".png", std::string{"SPC Convective Outlook Day "} + (day == 48 ? "4-8" : To::string(day)), [this, day] { launchSpcSwoDay1(day); });
    }
    routeItems.emplace_back("ntor.png", "Severe outlook comparison (SPC / CSU-MLP / CIPS)", [parent] { new SvrComparison{parent}; });
    routeItems.emplace_back("fmap.png", "National Images, Ctrl-i", [this] { launchNationalImages(); });
    routeItems.emplace_back("meso.png", "SPC Mesoanalysis, Ctrl-z", [this] { launchSpcMeso(); });
    routeItems.emplace_back("nwsobssites.png", "Observation Sites", [this] { launchObservationSites(); });
    routeItems.emplace_back("nwsobs.png", "Observations", [this] { launchObservations(); });
    routeItems.emplace_back("nwsobs.png", "Surface observations map: airports and the MADIS mesonets, wind barbs, temperatures", [parent] { new SurfaceViewer{parent}; });
    routeItems.emplace_back("fmap.png", "Master map: surface stations, river gauges, dams, buoys and tropical storms as layers on one map", [parent] { new MasterMapViewer{parent}; });
    routeItems.emplace_back("rtma.png", "RTMA", [this] { launchRtma(); });
    routeItems.emplace_back("spcsoundings.png", "Soundings", [parent] { new SoundingViewer{parent, string{}}; });

    routeItems.emplace_back("mcd_tile.png", "MRMS (radar-derived: reflectivity, hail, rotation, rain)", [parent] { new MrmsViewer{parent}; });
    routeItems.emplace_back("radarmosaicnws.png", "Radar Mosaic", [this] { launchRadarMosaicViewer(); });
    routeItems.emplace_back("srfd.png", "National Text", [this] { launchNationalText(); });
    routeItems.emplace_back("uswarn.png", "US Alerts", [this] { launchUsAlerts(); });
    routeItems.emplace_back("wpc_rainfall.png", "WPC Rainfall Outlook Summary", [this] { launchRainfallOutlookSummary(); });
    routeItems.emplace_back("spccompmap.png", "SPC Compmap", [this] { launchSpcCompmap(); });
    routeItems.emplace_back("tstorm.png", "SPC Thunderstorm Outlooks", [this] { launchSpcTstormOutlooks(); });

    routeItems.emplace_back("lightning.png", "Lightning, Ctrl-l", [this] { launchLightning(); });
    routeItems.emplace_back("fire_outlook.png", "SPC Fire Weather Outlooks, Ctrl-f", [this] { launchSpcFireWeatherOutlookSummary(); });
    routeItems.emplace_back("report_today.png", "SPC Storm Reports - today", [this] { launchSpcStormReports("today"); });
    routeItems.emplace_back("report_yesterday.png", "SPC Storm Reports - yesterday", [this] { launchSpcStormReports("yesterday"); });
    routeItems.emplace_back("nhc.png", "NHC product viewer, Ctrl-o", [this] { launchNhc(); });
    routeItems.emplace_back("nhc.png", "Tropical: active storms worldwide (CIRA / RAMMB), with the NHC tool", [parent] { new TropicalViewer{parent}; });

    routeItems.emplace_back("ncep.png", "NCEP Models, Ctrl-m", [this] { launchModelViewer(); });
    routeItems.emplace_back("spchrrr.png", "SPC HRRR", [this] { launchModelViewerGeneric("SPCHRRR"); });
    routeItems.emplace_back("spcsref.png", "SPC SREF", [this] { launchModelViewerGeneric("SPCSREF"); });
    routeItems.emplace_back("hrrrviewer.png", "ESRL HRRR/RAP", [this] { launchModelViewerGeneric("ESRL"); });
    routeItems.emplace_back("opc.png", "Ocean Prediction Center", [this] { launchOpc(); });
    routeItems.emplace_back("opc.png", "Climate and ocean: sea surface temperature and anomaly, El Niño / La Niña, cycles", [parent] { new ClimateViewer{parent}; });
    routeItems.emplace_back("tropstorm.png", "Tropical cyclones (Atlantic, East and Central Pacific): track, model guidance (spaghetti) and recon flights", [parent] { new HurricaneViewer{parent}; });
    routeItems.emplace_back("hurricane.png", "Tropical Hub: active storms, outlook, recon plan, season", [parent] { Dashboards::openTropicalHub(parent); });
    routeItems.emplace_back("goes16.png", "Space weather: storm scales, Kp, flares, solar wind, aurora, sun", [parent] { Dashboards::openSpaceWeather(parent); });
    routeItems.emplace_back("twtornado.png", "Tornado history: SPC tornado tracks since 1950 by year, rating, state and area; counts by day, week, month, year and decade", [parent] { Dashboards::openTornadoHistory(parent); });
    routeItems.emplace_back("widget_afd.png", "Forecast discussions (planned): all centres, history, what changed", [parent] { Dashboards::openForecastDiscussions(parent); });
    routeItems.emplace_back("fire_outlook.png", "Drought: the U.S. Drought Monitor and how it changed, precipitation and its departure from normal, outlooks, soil moisture", [parent] { new DroughtViewer{parent}; });
    routeItems.emplace_back("rain_showers.png", "Rivers: NWS river gauges, flood stages, forecasts and the National Water Model", [parent] { new RiverMapViewer{parent}; });
    routeItems.emplace_back("nsslwrf.png", "NSSL WRF", [this] { launchModelViewerGeneric("NSSLWRF"); });
    routeItems.emplace_back("wpcgefs.png", "WPC GEFS", [this] { launchModelViewerGeneric("WPCGEFS"); });
    // routeItems.emplace_back("spchref.png", "SPC HREF", [this] { launchModelViewerGeneric("SPCHREF"); });
    routeItems.emplace_back("goesfulldisk.png", "Global GOES", [parent] { new GoesGlobal{parent}; });

    // a repeated icon (CAMs / NSSL WRF, the two thunderstorm items) still needs its own id
    vector<string> seen;
    for (auto& item : routeItems) {
        if (std::find(seen.begin(), seen.end(), item.id) != seen.end()) {
            item.id += "#2";
        }
        seen.push_back(item.id);
    }
    applySavedOrder();
    vector<string> allIds;
    for (const auto& item : routeItems) {
        allIds.push_back(item.id);
    }
    ToolbarGroups::load(allIds);
    rebuildButtons();
}

const vector<RouteItem>& Toolbar::getRouteItems() const {
    return routeItems;
}

// swaps the tiles at fromIndex/toIndex (wrapping, so "move up" from the top
// and "move down" from the bottom cycle to the other end), persists the new
// order, and rebuilds the visible button strip immediately
void Toolbar::moveRouteItem(int fromIndex, int toIndex) {
    const auto count = static_cast<int>(routeItems.size());
    if (count < 2 || fromIndex < 0 || fromIndex >= count) {
        return;
    }
    toIndex = ((toIndex % count) + count) % count;
    std::swap(routeItems[fromIndex], routeItems[toIndex]);
    persistOrder();
    rebuildButtons();
}

// applies a previously saved order (a comma-joined list of icon filenames -
// each RouteItem's icon is unique, so it doubles as a stable id). Routes not
// mentioned (added since the order was last saved) keep their built-in
// relative order, appended at the end.
void Toolbar::applySavedOrder() {
    const auto savedPref = Utility::readPref(orderPrefToken, "");
    if (savedPref.empty()) {
        return;   // first run - keep the built-in default order
    }
    const auto saved = WString::split(savedPref, ",");
    vector<RouteItem> ordered;
    for (const auto& key : saved) {
        for (const auto& item : routeItems) {
            if (item.id == key) {
                ordered.push_back(item);
                break;
            }
        }
    }
    for (const auto& item : routeItems) {
        const auto known = std::find(saved.begin(), saved.end(), item.id) != saved.end();
        if (!known) {
            ordered.push_back(item);
        }
    }
    routeItems = ordered;
}

void Toolbar::persistOrder() {
    vector<string> keys;
    for (const auto& item : routeItems) {
        keys.push_back(item.id);
    }
    Utility::writePref(orderPrefToken, WString::join(keys, ","));
}

// Draws the toolbar in the chosen style (ToolbarGroups::mode): the original column of icons, icons with their
// names under group headings, or just the auto-update control with the entries in a menu bar of group menus.
void Toolbar::rebuildButtons() {
    getView()->removeWidget(autoUpdate.getView());   // it is kept and added again: removeChildren() would delete it, and the next rebuild would add a dead widget
    removeChildren();
    buttons.clear();
    parent->menuBar()->clear();
    const auto mode = ToolbarGroups::mode();
    parent->menuBar()->setVisible(mode == ToolbarGroups::MenuBar);
    addWidget(autoUpdate);
    const auto itemFor = [this] (const string& id) -> const RouteItem * {
        for (const auto& item : routeItems) {
            if (item.id == id) {
                return &item;
            }
        }
        return nullptr;
    };
    if (mode == ToolbarGroups::Icons) {
        // a group set to be a dropdown is one button, where its first entry would be, that opens a menu of its entries
        vector<string> placed;
        for (const auto& item : routeItems) {
            const auto& groups = ToolbarGroups::groups();
            const auto index = ToolbarGroups::groupOf(item.id);
            if (index >= 0 && index < static_cast<int>(groups.size()) && ToolbarGroups::isDropdown(groups[static_cast<size_t>(index)].name)) {
                const auto& group = groups[static_cast<size_t>(index)];
                if (std::find(placed.begin(), placed.end(), group.name) == placed.end()) {
                    placed.push_back(group.name);
                    addDropdownButton(group.name, group.ids);
                }
                continue;
            }
            buttons.emplace_back(parent, item.iconString, item.toolTip);
            buttons.back().connect(item.fn);
            addWidget(buttons.back());
        }
    } else if (mode == ToolbarGroups::IconsText) {
        for (const auto& group : ToolbarGroups::groups()) {
            if (group.ids.empty()) {
                continue;
            }
            auto * heading = new QLabel{QString::fromStdString(group.name), parent};
            heading->setStyleSheet("font-weight: bold; margin-top: 6px;");
            addWidgetReal(heading);
            for (const auto& id : group.ids) {
                const auto * item = itemFor(id);
                if (item == nullptr) {
                    continue;
                }
                buttons.emplace_back(parent, item->iconString, item->toolTip);
                buttons.back().setText(item->label);
                buttons.back().getView()->setStyleSheet("text-align: left; padding: 2px 6px;");
                buttons.back().connect(item->fn);
                addWidget(buttons.back());
            }
        }
    } else {
        for (const auto& group : ToolbarGroups::groups()) {
            if (group.ids.empty()) {
                continue;
            }
            auto * menu = parent->menuBar()->addMenu(QString::fromStdString(group.name));
            for (const auto& id : group.ids) {
                const auto * item = itemFor(id);
                if (item == nullptr) {
                    continue;
                }
                auto * action = menu->addAction(QIcon{QString::fromStdString(GlobalVariables::imageDir + item->iconString)},
                                                QString::fromStdString(item->label));
                action->setToolTip(QString::fromStdString(item->toolTip));
                QObject::connect(action, &QAction::triggered, parent, item->fn);
            }
        }
    }
    addStretch();
}

// One button for a whole group: its menu lists the entries with their icons and names. The glyph is drawn (the blocks of a dashboard) so it needs no picture file.
void Toolbar::addDropdownButton(const string& groupName, const vector<string>& ids) {
    buttons.emplace_back(parent, "", groupName + " (menu)");
    auto& button = buttons.back();
    const int size = std::max(16, ButtonFlat::getIconSize());
    QPixmap glyph{size, size};
    glyph.fill(Qt::transparent);
    {
        QPainter p{&glyph};
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor{30, 30, 30});
        const double u = size / 24.0;   // the 24 unit grid of the other icons
        const double r = 1.2 * u;
        p.drawRoundedRect(QRectF{3 * u, 3 * u, 8 * u, 11 * u}, r, r);     // the tall block
        p.drawRoundedRect(QRectF{13 * u, 3 * u, 8 * u, 5 * u}, r, r);     // top right
        p.drawRoundedRect(QRectF{13 * u, 10 * u, 8 * u, 11 * u}, r, r);   // tall right
        p.drawRoundedRect(QRectF{3 * u, 16 * u, 8 * u, 5 * u}, r, r);     // bottom left
        // a small arrow: it opens a menu
        p.setBrush(QColor{30, 30, 30});
        QPolygonF arrow;
        arrow << QPointF{15 * u, 14.5 * u} << QPointF{19 * u, 14.5 * u} << QPointF{17 * u, 17.5 * u};
        p.setBrush(Qt::white);
        p.drawPolygon(arrow);
    }
    button.getView()->setIcon(QIcon{glyph});
    button.getView()->setIconSize(QSize{size, size});
    QPushButton * view = button.getView();
    QObject::connect(view, &QPushButton::released, parent, [this, view, ids] {
        QMenu menu;
        for (const auto& id : ids) {
            for (const auto& item : routeItems) {
                if (item.id != id) {
                    continue;
                }
                auto * action = menu.addAction(QIcon{QString::fromStdString(GlobalVariables::imageDir + item.iconString)}, QString::fromStdString(item.label));
                action->setToolTip(QString::fromStdString(item.toolTip));
                QObject::connect(action, &QAction::triggered, parent, item.fn);
                break;
            }
        }
        menu.exec(view->mapToGlobal(QPoint{view->width(), 0}));   // opens to the right of the column
    });
    addWidget(button);
}

void Toolbar::launchRoute(const string& id) {
    for (const auto& item : routeItems) {
        if (item.id == id) {
            item.fn();
            return;
        }
    }
}

void Toolbar::rebuild() {
    rebuildButtons();
}

void Toolbar::launchHourly() {
    new Hourly{parent};
}

void Toolbar::launchWfoText() {
    new WfoText{parent};
}

void Toolbar::launchSpcSwoSummary() {
    new SpcSwoSummary{parent};
}

void Toolbar::launchSpcSwoDay1(int day) {
    new SpcSwoDay1{parent, day};
}

void Toolbar::launchGoesViewer() {
    Route::vis(parent);
}

void Toolbar::launchNationalText() {
    new NationalText{parent};
}

void Toolbar::launchSpcTstormOutlooks() {
    new SpcTstormOutlooks{parent};
}

void Toolbar::launchSpcCompmap() {
    new SpcCompMap{parent};
}

void Toolbar::launchSevereDashboard() {
    new SevereDashboard{parent};
}

void Toolbar::launchNhc() {
    new Nhc{parent};
}

void Toolbar::launchRadarMosaicViewer() {
    new RadarMosaic{parent};
}

void Toolbar::launchLightning() {
    Route::lightning(parent);
}

void Toolbar::launchObservationSites() {
    new ObservationSites{parent};
}

void Toolbar::launchObservations() {
    new Observations{parent};
}

void Toolbar::launchSpcMeso(const string& product) {
    new SpcMeso{parent, product};
}

void Toolbar::launchModelViewer() {
    new ModelViewer{parent, "NCEP"};
}

void Toolbar::launchModelViewerGeneric(const string& modelType) {
    new ModelViewer{parent, modelType};
}

void Toolbar::launchNationalImages() {
    new NationalImages{parent};
}

void Toolbar::launchUsAlerts() {
    new UsAlerts{parent};
}

void Toolbar::launchRainfallOutlookSummary() {
    new RainfallOutlookSummary{parent};
}

void Toolbar::launchSpcFireWeatherOutlookSummary() {
    new SpcFireSummary{parent};
}

void Toolbar::launchSpcStormReports(const string& day) {
    new SpcStormReports{parent, day};
}

void Toolbar::launchOpc() {
    new Opc{parent};
}

void Toolbar::launchRtma() {
    new Rtma{parent};
}

void Toolbar::launchSettings() {
    new SettingsMain{parent, reloadFn, true, false, this};
}

void Toolbar::refresh() {
    if (UIPreferences::toolbarIconSize != ButtonFlat::getIconSize()) {
        for (auto& button : buttons) {
            button.refresh();
        }
    }
}


