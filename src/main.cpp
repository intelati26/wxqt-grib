// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include <future>
#include <QApplication>
#include <QCursor>
#include <QElapsedTimer>
#include <QFile>
#include <QKeyEvent>
#include <QWheelEvent>
#include "cfs/CfsArchiveIndex.h"
#include "cfs/CfsPlumeViewer.h"
#include "cfs/UtilityCfs.h"
#include "gfs/GfsClimate.h"
#include "hurricane/ReconViewer.h"
#include "radar/MapWidget.h"
#include "drought/DroughtViewer.h"
#include "hurricane/SarWindsViewer.h"
#include "hurricane/AtcfFeedViewer.h"
#include "hurricane/AscatUhrViewer.h"
#include "space/SatelliteOrbit.h"
#include "space/SatellitePassViewer.h"
#include "space/SatellitePasses.h"
#include "vis/GoesViewer.h"
#include "util/HomeThumbnails.h"
#include "objects/NetManager.h"
#include "objects/URL.h"
#include "ui/NetStatus.h"
#include "ui/ZoomImage.h"
#include <QMainWindow>
#include <exception>
#include <QMetaObject>
#include <QDebug>
#include <QPixmap>
#include <QTabWidget>
#include <QThreadPool>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QDialog>
#include <QTimer>
#include <QThread>
#include "common/GlobalVariables.h"
#include "gfs/GfsModels.h"
#include <QDir>
#include <QStandardPaths>
#include "gfs/GfsRender.h"
#include "tropical/PhaseViewer.h"
#include "misc/AhdViewer.h"
#include "rivers/RiverForecastList.h"
#include "misc/ProductViewer.h"
#include "spc/AbpgExtended.h"
#include "spc/WatchProbabilityViewer.h"
#include "space/UtilitySun.h"
#include "space/SunViewer.h"
#include "spcrefs/SpcRefsLoader.h"
#include "hurricane/HafsIntensity.h"
#include "hurricane/AceForecastViewer.h"
#include <QBuffer>
#include "ui/ChartExport.h"
#include "ui/ExportDialog.h"
#include "sounding/SoundingHazard.h"
#include "models/UtilityModelNcepInterface.h"
#include "gfs/GfsChart.h"
#include "hurricane/HurricaneViewer.h"
#include "mapkit/MasterMapViewer.h"
#include "climate/TempHistoryViewer.h"
#include "mapkit/WorldRender.h"
#include "mapkit/ModelChartLayer.h"
#include "mapkit/UpperAirLayer.h"
#include "dashboard/Dashboards.h"
#include "models/ModelScreen.h"
#include "tropical/CiraStorm.h"
#include "models/ModelViewer.h"
#include "history/EventsViewer.h"
#include "history/HistoryCursor.h"
#include "settings/CacheSummaryViewer.h"
#include "ui/NetStatus.h"
#include "models/SoundingViewer.h"
#include "util/Utility.h"
#include "hurricane/ReconViewer.h"
#include "hurricane/SeasonViewer.h"
#include "spc/SpcSwoStateGraphics.h"
#include "tornado/TornadoYearsViewer.h"
#include "ui/MainWindow.h"
#include "util/AppState.h"
#include "util/ContactPrompt.h"
#include "util/CrashLog.h"
#include "util/MyApplication.h"
#include "util/UtilityTheme.h"

namespace {
    // an exception thrown inside an event handler would otherwise unwind through Qt and end the program: log it, keep running
    class WxqtApplication : public QApplication {
    public:
        using QApplication::QApplication;
        bool notify(QObject * receiver, QEvent * event) override {
            try {
                return QApplication::notify(receiver, event);
            } catch (const std::exception& e) {
                CrashLog::write(std::string{"exception in an event handler (ignored): "} + e.what() + " [" +
                                (receiver != nullptr ? receiver->metaObject()->className() : "?") + "]");
            } catch (...) {
                CrashLog::write("unknown exception in an event handler (ignored)");
            }
            return false;
        }
    };
}

namespace {
    // Ctrl + / Ctrl - / Ctrl 0 zoom whatever is zoomable under the pointer (a map, a chart picture, the recon map), as a PDF viewer does:
    // a wheel step is sent to the widget, so everything that zooms with the wheel zooms with the keys; 0 puts the picture back
    class ZoomKeys : public QObject {
    public:
        explicit ZoomKeys(QObject * parent) : QObject{parent} {}
        bool eventFilter(QObject *, QEvent * event) override {
            if (event->type() != QEvent::KeyPress) {
                return false;
            }
            const auto * key = static_cast<QKeyEvent *>(event);
            if (!(key->modifiers() & Qt::ControlModifier)) {
                return false;
            }
            if (key->key() == Qt::Key_N && (key->modifiers() & Qt::ShiftModifier)) {   // Ctrl+Shift+N: the Network window
                NetStatus::show(QApplication::activeWindow());
                return true;
            }
            const bool in = key->key() == Qt::Key_Plus || key->key() == Qt::Key_Equal, out = key->key() == Qt::Key_Minus, reset = key->key() == Qt::Key_0;
            if (!in && !out && !reset) {
                return false;
            }
            QWidget * target = QApplication::widgetAt(QCursor::pos());
            if (!target || target->window() != QApplication::activeWindow()) {   // the pointer is elsewhere: the middle of the active window
                const auto * w = QApplication::activeWindow();
                target = w ? w->childAt(w->rect().center()) : nullptr;
            }
            for (auto * w = target; w != nullptr; w = w->parentWidget()) {
                if (auto * recon = dynamic_cast<ReconMap *>(w)) {
                    if (reset) recon->resetZoom();
                    else send(w, target, in);
                    return true;
                }
                if (auto * image = dynamic_cast<ZoomImage *>(w)) {
                    if (reset) image->fitToViewport();
                    else send(w, target, in);
                    return true;
                }
                if (dynamic_cast<MapWidget *>(w)) {
                    if (!reset) send(w, target, in);
                    return true;
                }
            }
            return false;
        }
    private:
        static void send(QWidget * zoomable, QWidget * under, bool in) {
            const QPoint global = under->rect().contains(under->mapFromGlobal(QCursor::pos())) ? QCursor::pos() : under->mapToGlobal(under->rect().center());
            const QPoint local = under->mapFromGlobal(global);
            QWheelEvent wheel{local, global, {}, QPoint{0, in ? 120 : -120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false};
            QApplication::sendEvent(under, &wheel);
            (void) zoomable;
        }
    };
}

int main(int argc, char * argv[]) {
    WxqtApplication a{argc, argv};
    bool debug = qEnvironmentVariable("WXQT_DEBUG") == "1";
    for (const auto& argument : a.arguments()) {
        debug = debug || argument == "--debug";
    }
    a.installEventFilter(new ZoomKeys{&a});
    {   // the user's own model files (*.model), read when the registry is first made: before anything asks for it
        const auto folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models";
        QDir{}.mkpath(folder);
        GfsModels::setUserFolder(folder.toStdString());
    }
    SpcRefsLoader::registerLoader();   // SPC's REFS Zarr store as a Model Viewer model
    GfsRender::startCacheCleanup();   // the model field cache is tidied in the background, a few seconds in
    CrashLog::install(debug);
    MyApplication::onCreate();
    UtilityTheme::apply();
    a.setWindowIcon(QIcon{QString::fromStdString(GlobalVariables::imageDir) + "wx_launcher.png"});
    if (a.arguments().size() == 3 && a.arguments()[1] == "-r") {
        return a.exec();
    } else {
        MainWindow w;
        w.show();
        QObject::connect(&a, &QCoreApplication::aboutToQuit, &a, [] {
            AppState::quitting = true;   // queued background jobs skip their work
            // let the running ones finish now, while the application object still exists: a download that is still going when
            // it is destroyed corrupts memory
            QThreadPool::globalInstance()->waitForDone();
            CrashLog::write("---- wxqt closing normally ----");
        });
        // development aids (run with QT_QPA_PLATFORM=offscreen): WXQT_OPEN=<toolbar entry id, e.g. ntor.png> opens that
        // tool; WXQT_GRAB=<file.png>[,<milliseconds>] saves a picture of the newest tool window (else the main
        // window) and quits, so a screen can be checked without a display; WXQT_SIZE=<w>x<h> sizes the tool window first; WXQT_TAB=<n> picks a tab
        const auto extraLayers = [] (const QString& list) {
            std::vector<std::string> ids;
            for (const auto& id : list.split(',', Qt::SkipEmptyParts)) {
                ids.push_back(id.toStdString());
            }
            return ids.empty() ? std::vector<std::string>{"radar/mrms"} : ids;
        };
        QWidget * opened = nullptr;
        if (const auto route = qEnvironmentVariable("WXQT_OPEN"); !route.isEmpty()) {
            const auto before = QApplication::topLevelWidgets();
            if (route.startsWith("mrmsat:")) {   // WXQT_OPEN=mrmsat:<yyyyMMddHHmm UTC>[:lat:lon:km[:layer,layer]]: the master map at that time, MRMS (or the layers named) only
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto when = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm");
                when.setTimeSpec(Qt::UTC);
                map->openAt(when, parts.value(2, "35.3").toDouble(), parts.value(3, "-97.5").toDouble(), parts.value(4, "300").toDouble(), extraLayers(parts.value(5)));
                if (const auto out = qEnvironmentVariable("WXQT_EXPORT_PNG"); !out.isEmpty()) {   // WXQT_EXPORT_PNG=<file> [WXQT_EXPORT_ACIS=<measure id>]: the map saved after WXQT_EXPORT_MS (a picture; the ACIS context under it when asked)
                    QTimer::singleShot(qEnvironmentVariableIntValue("WXQT_EXPORT_MS") > 0 ? qEnvironmentVariableIntValue("WXQT_EXPORT_MS") : 20000, map, [map, out] { map->exportWithContext(out, qEnvironmentVariable("WXQT_EXPORT_ACIS").toStdString(), qEnvironmentVariable("WXQT_EXPORT_OFFICE").toStdString()); });
                }
            } else if (route.startsWith("mcexport:")) {   // WXQT_OPEN=mcexport:<yyyyMMddHH UTC>:<lat>:<lon>:<km>:<out.png>[:PREF=value...]: the master map with the model chart, and, after a while, its titled export
                const auto parts = route.split(':');
                for (int i = 6; i < parts.size(); i++) {   // further parts: "PREF=value" of the chart layer (MM_MODELCHART_MODEL, _PARAM, _OVERLAYS)
                    const auto pair = parts[i].split('=');
                    if (pair.size() == 2) {
                        Utility::writePref(pair[0].toStdString(), pair[1].toStdString());
                    }
                }
                auto * map = new MasterMapViewer{&w};
                auto when = QDateTime::fromString(parts.value(1), "yyyyMMddHH");
                when.setTimeSpec(Qt::UTC);
                map->openAt(when, parts.value(2, "39").toDouble(), parts.value(3, "-98").toDouble(), parts.value(4, "2200").toDouble(), {"model/chart"});
                const QString out = parts.value(5, "/tmp/mcexport.png");
                QTimer::singleShot(40000, map, [map, out] {
                    std::string problem;
                    qWarning("mcexport: %s %s", map->saveModelChart(out, problem) ? "saved" : "failed:", problem.c_str());
                });
            } else if (route.startsWith("goesat:")) {   // WXQT_OPEN=goesat:<yyyyMMddHHmm UTC>: the master map at that time with the GOES history layer
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto when = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm");
                when.setTimeSpec(Qt::UTC);
                map->openAt(when, 37.0, -97.0, 900.0, {"satellite/goes-history"});
            } else if (route.startsWith("sun:")) {   // WXQT_OPEN=sun:<out.png>[:width]: the labelled Sun picture saved, then the program goes on
                const auto parts = route.split(':');
                UtilitySun::labelled(parts.value(2, "900").toInt()).save(parts.value(1));
            } else if (route == "sunviewer") {   // WXQT_OPEN=sunviewer: the Sun window with the hover on its regions
                new SunViewer{&w};
            } else if (route == "abpgext") {   // WXQT_OPEN=abpgext: the extended (days 6-14) analog guidance window
                new AbpgExtended{&w};
            } else if (route == "product") {   // WXQT_OPEN=product WXQT_PRODUCT_IMAGE=<url> WXQT_PRODUCT_TEXT=<url>: the text-and-picture window
                new ProductViewer{&w, "Product", qEnvironmentVariable("WXQT_PRODUCT_IMAGE").toStdString(), "", qEnvironmentVariable("WXQT_PRODUCT_TEXT").toStdString()};
            } else if (route.startsWith("watchprob:")) {   // WXQT_OPEN=watchprob:<number>[:<number>...]: the watch probabilities table
                std::vector<WatchProbabilityViewer::Watch> list;
                for (const auto& n : route.split(':').mid(1)) {
                    list.push_back({n.toInt(), "Watch " + n, 0});
                }
                new WatchProbabilityViewer{&w, list};
            } else if (route == "riverforecast") {   // WXQT_OPEN=riverforecast: the table of gauges forecast to reach action stage (with WXQT_TABLE_OUT=<stem> the export files are written)
                auto * list = new RiverForecastList{&w};
                if (!qEnvironmentVariable("WXQT_TABLE_OUT").isEmpty()) {
                    QTimer::singleShot(qEnvironmentVariableIntValue("WXQT_TABLE_DELAY") > 0 ? qEnvironmentVariableIntValue("WXQT_TABLE_DELAY") : 20000, list, [list] { QMetaObject::invokeMethod(list, [list] { list->exportNow(); }); });
                }
            } else if (route == "ahd") {   // WXQT_OPEN=ahd: the Area Hydrologic Discussion window
                new AhdViewer{&w};
            } else if (route == "radar") {   // WXQT_OPEN=radar: the radar doorway (the master map at the current location with MRMS, warnings and discussions)
                MasterMapViewer::openRadar(&w);
            } else if (route.startsWith("phase:")) {   // WXQT_OPEN=phase:<model>:<lat>:<lon>[:<atcf id>]: the phase space window of a storm at that place in that model
                const auto parts = route.split(':');
                auto * viewer = PhaseViewer::pick(&w, parts.value(4).isEmpty() ? "test" : parts.value(4).toStdString(), parts.value(2, "15").toDouble(), parts.value(3, "-60").toDouble(), parts.value(1, "GFS").toStdString(), parts.value(4).toStdString());
                for (const auto& extra : qEnvironmentVariable("WXQT_PHASE_EXTRA").split('|', Qt::SkipEmptyParts)) {   // WXQT_PHASE_EXTRA="lat lon name|lat lon name": more systems to compare
                    const auto f = extra.split(' ', Qt::SkipEmptyParts);
                    if (f.size() >= 3) {
                        PhaseViewer::pick(&w, f.mid(2).join(' ').toStdString(), f[0].toDouble(), f[1].toDouble());
                    }
                }
                (void) viewer;
            } else if (route.startsWith("modelat:")) {   // WXQT_OPEN=modelat:<model>:<yyyyMMddHHmm UTC>: the Model Viewer on that model at the run and hour of that time
                const auto parts = route.split(':');
                auto when = QDateTime::fromString(parts.value(2), "yyyyMMddHHmm");
                when.setTimeSpec(Qt::UTC);
                MasterMapViewer::openModels(&w, parts.value(1).toStdString(), {}, {}, when);
            } else if (route == "events" && !qEnvironmentVariable("WXQT_KEEP").isEmpty()) {   // WXQT_OPEN=events WXQT_KEEP=1: the first saved event is selected and its offline copy is made
                auto * viewer = new EventsViewer{&w};
                QTimer::singleShot(1500, viewer, [viewer] {
                    for (auto * list : viewer->findChildren<QListWidget *>()) { list->setCurrentRow(0); }
                    for (auto * button : viewer->findChildren<QPushButton *>()) { if (button->text() == "Keep offline") { button->click(); } }
                });
            } else if (route == "events") {   // WXQT_OPEN=events: the history page
                new EventsViewer{&w};
            } else if (route.startsWith("cursorat:")) {   // WXQT_OPEN=cursorat:<yyyyMMddHHmm>:<yyyyMMddHHmm>: a master map (GOES history) opened at the first time, the history cursor moved to the second after 4 s
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto first = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm");
                first.setTimeSpec(Qt::UTC);
                auto second = QDateTime::fromString(parts.value(2), "yyyyMMddHHmm");
                second.setTimeSpec(Qt::UTC);
                map->openAt(first, 37.0, -97.0, 900.0, {"satellite/goes-history"});
                QTimer::singleShot(4000, map, [second] { HistoryCursor::get().set(second, "test", nullptr); });
            } else if (route == "catalog") {   // WXQT_OPEN=catalog: the history page's catalog of tornado outbreaks
                auto * viewer = new EventsViewer{&w};
                viewer->openCatalog();
                if (const auto kind = qEnvironmentVariable("WXQT_KIND"); !kind.isEmpty()) {   // WXQT_KIND=<n>: the catalog of that number (0 tornadoes, 1 biggest days, 2 storms); WXQT_WHAT=<n> its measure
                    for (auto * dialog : viewer->findChildren<QDialog *>()) {
                        if (auto * box = dialog->findChild<QComboBox *>()) {
                            if (const auto what = qEnvironmentVariable("WXQT_WHAT"); !what.isEmpty()) {
                                const auto boxes = dialog->findChildren<QComboBox *>();
                                if (boxes.size() > 1) {
                                    boxes[1]->blockSignals(true);
                                }
                            }
                            box->setCurrentIndex(kind.toInt());
                        }
                    }
                }
                if (const auto file = qEnvironmentVariable("WXQT_PICKER_PNG"); !file.isEmpty()) {   // a picture of the catalog dialog itself, after WXQT_WAIT seconds (the database is read first)
                    for (auto * dialog : viewer->findChildren<QDialog *>()) {
                        QTimer::singleShot(qEnvironmentVariableIntValue("WXQT_WAIT") * 1000, dialog, [dialog, file] { dialog->grab().save(file); });
                    }
                }
            } else if (route.startsWith("colsound:")) {   // WXQT_OPEN=colsound:<model>:<yyyyMMddHH>:<hour>:<lat>:<lon>: the sounding of that model's column over the point
                const auto parts = route.split(':');
                GfsRender::Session session;
                SoundingProfile profile;
                std::string problem;
                const bool ok = GfsRender::columnProfile(session, parts.value(1).toStdString(), parts.value(2).toStdString(), parts.value(3).toInt(), parts.value(4).toDouble(), parts.value(5).toDouble(), profile, problem);
                qWarning("colsound: %s %zu levels %s", ok ? "ok" : "failed", profile.size(), problem.c_str());
                for (size_t i = 0; i < profile.size(); i++) {
                    qWarning("  p %.0f h %.0f t %.1f td %.1f wd %.0f ws %.1f", profile.pres[i], profile.hght[i], profile.tmpc[i], profile.dwpc[i], profile.wdir[i], profile.wspd[i]);
                }
                if (ok) {
                    new SoundingViewer{&w, profile, "test column"};
                }
            } else if (route.startsWith("raobsound:")) {   // WXQT_OPEN=raobsound:<station>:<yyyyMMddHH UTC>: the observed balloon sounding from the archive
                const auto parts = route.split(':');
                auto when = QDateTime::fromString(parts.value(2), "yyyyMMddHH");
                when.setTimeZone(QTimeZone::utc());
                SoundingProfile profile;
                std::string problem;
                const bool ok = UpperAirLayer::fetchProfile(parts.value(1).toStdString(), static_cast<long>(when.toSecsSinceEpoch()), profile, problem);
                qWarning("raobsound: %s %zu levels %s", ok ? "ok" : "failed", profile.size(), problem.c_str());
                if (ok) {
                    new SoundingViewer{&w, profile, "observed " + parts.value(1).toStdString()};
                }
            } else if (route.startsWith("idxprobe:")) {   // WXQT_OPEN=idxprobe:<model>:<url of an index handled by CfsArchiveIndex>: the index text it makes, and the model's chart ids
                const auto parts = route.split(':');
                const auto text = CfsArchiveIndex::index(route.section(':', 2).toStdString());
                qWarning("idxprobe: %d bytes\n%s", static_cast<int>(text.size()), text.constData());
                std::string ids;
                for (const auto& product : GfsChart::products()) {
                    if (product.source == parts.value(1).toStdString()) {
                        ids += product.id + " ";
                    }
                }
                qWarning("idxprobe charts: %s", ids.c_str());
            } else if (route.startsWith("temphist:")) {   // WXQT_OPEN=temphist:<lat>:<lon>[:<yyyyMMdd up to>]: the temperature history window
                const auto parts = route.split(':');
                (new TempHistoryViewer{&w, parts.value(1).toDouble(), parts.value(2).toDouble(), QDate::fromString(parts.value(3), "yyyyMMdd"), parts.value(4) == "station"})->setMetric(parts.value(5).toInt());
            } else if (route.startsWith("globemap:")) {   // WXQT_OPEN=globemap:<yyyyMMddHHmm UTC>:<lat>:<lon>:<km>:<layer,layer>: the master map in globe mode
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto when = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm");
                when.setTimeZone(QTimeZone::utc());
                map->openAt(when, parts.value(2).toDouble(), parts.value(3).toDouble(), parts.value(4).toDouble(), extraLayers(parts.value(5)));
                const auto projection = parts.value(6) == "robinson" ? GlobeMapView::Projection::Robinson : parts.value(6) == "equal" ? GlobeMapView::Projection::EqualEarth : parts.value(6) == "mollweide" ? GlobeMapView::Projection::Mollweide : parts.value(6) == "north" ? GlobeMapView::Projection::PolarNorth : parts.value(6) == "south" ? GlobeMapView::Projection::PolarSouth : GlobeMapView::Projection::Globe;
                map->setGlobe(projection == GlobeMapView::Projection::Globe);
                if (const auto area = qEnvironmentVariable("WXQT_AREA"); !area.isEmpty()) {   // WXQT_AREA=<id>[,go]: the area previewed on the globe (as the area picker does on hover) after a while
                    QTimer::singleShot(8000, map, [map, area] { map->previewArea(area.section(',', 0, 0).toStdString(), area.endsWith(",go")); });
                }
                if (parts.value(7) == "export") {   // globemap:...:<globe|equal|mollweide>:export:<out.png>:<center lon>: also the whole world as a picture, after a while
                    const QString out = parts.value(8);
                    const double centre = parts.value(9).toDouble();
                    if (GlobeMapView::isPolar(projection)) {   // WXQT_POLAR_REACH=<latitude> WXQT_POLAR_WIDE=1
                        GlobeMapView::setPolarFrame(qEnvironmentVariableIntValue("WXQT_POLAR_REACH") > 0 ? qEnvironmentVariableIntValue("WXQT_POLAR_REACH") : 20, qEnvironmentVariableIntValue("WXQT_POLAR_WIDE") > 0);
                    }
                    QTimer::singleShot(20000, map, [map, out, projection, centre] { map->exportWorldTo(out, projection == GlobeMapView::Projection::Globe ? GlobeMapView::Projection::EqualEarth : projection, centre, qEnvironmentVariableIntValue("WXQT_WORLD_WIDTH") > 0 ? qEnvironmentVariableIntValue("WXQT_WORLD_WIDTH") : 3000, qEnvironmentVariableIntValue("WXQT_WORLD_HIGH") > 0, qEnvironmentVariableIntValue("WXQT_GRATICULE") > 0); });
                }
            } else if (route.startsWith("seamcheck:")) {   // WXQT_OPEN=seamcheck:<chart>:<model,model>:<yyyyMMddHH UTC>: where each model's fill (and wind) grid has no value along a sweep of longitudes (a gap in the wrap), then quits
                const auto parts = route.split(':');
                auto when = QDateTime::fromString(parts.value(3), "yyyyMMddHH");
                when.setTimeZone(QTimeZone::utc());
                std::vector<std::string> models;
                for (const auto& m : parts.value(2).split(',', Qt::SkipEmptyParts)) {
                    models.push_back(m.toStdString());
                }
                const auto sources = WorldRender::loadSources(parts.value(1).toStdString(), models, when);
                for (const auto& s : sources) {
                    qWarning("seamcheck %-9s grid %dx%d step %.4f lon0 %.3f global %d", s.model.c_str(), s.grid.columns, s.grid.rows, s.grid.step, s.grid.lon0, s.grid.global() ? 1 : 0);
                    for (const double lat : {10.0, 40.0, -40.0}) {
                        double gapStart = -999.0;
                        for (double lon = -180.0; lon <= 180.001; lon += 0.05) {
                            float v = s.grid.sample(lon, lat);
                            if (std::isnan(v)) { v = s.grid.sample(lon + 360.0, lat); }
                            if (std::isnan(v)) { v = s.grid.sample(lon - 360.0, lat); }
                            if (std::isnan(v) && gapStart < -900.0) { gapStart = lon; }
                            if (!std::isnan(v) && gapStart > -900.0) { qWarning("   lat %5.1f no value from %.2f to %.2f", lat, gapStart, lon); gapStart = -999.0; }
                        }
                        if (gapStart > -900.0) { qWarning("   lat %5.1f no value from %.2f to the end", lat, gapStart); }
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("magcheck:")) {   // WXQT_OPEN=magcheck: the model guidance site's chart lists of the picture models against the charts the GRIB models draw (what is missing), then quits
                const struct { const char * picture; const std::vector<std::string> * ids; std::vector<const char *> grib; } lists[] = {
                    {"RAP", &UtilityModelNcepInterface::paramsRap, {"RRFS", "HRRR"}},
                    {"NAM", &UtilityModelNcepInterface::paramsNam, {"RRFS", "HRRR"}}, {"NAM-HIRES", &UtilityModelNcepInterface::paramsNamHires, {"RRFS", "HRRR"}},
                    {"HRW-ARW", &UtilityModelNcepInterface::paramsHrwNmm, {"RRFS", "HRRR"}}, {"HRW-ARW2", &UtilityModelNcepInterface::paramsHrwArw2, {"RRFS", "HRRR"}},
                    {"HRW-FV3", &UtilityModelNcepInterface::modelHrwFv3Params, {"RRFS", "HRRR"}}};
                for (const auto& list : lists) {
                    for (const auto * grib : list.grib) {
                        std::string missing;
                        int have = 0;
                        for (const auto& id : *list.ids) {
                            if (GfsChart::product(id, grib) != nullptr) { have++; } else { missing += " " + id; }
                        }
                        fprintf(stderr, "MAGCHECK %s vs %s: %d of %d drawn; missing:%s\n", list.picture, grib, have, static_cast<int>(list.ids->size()), missing.c_str());
                    }
                }
                if (qEnvironmentVariableIsSet("WXQT_HAZARD_TEST")) {   // WXQT_HAZARD_TEST=1: the hazard flow chart on made-up parameters (every outcome), printed
                    SoundingProfile profile;
                    profile.pres = {1000, 500};
                    profile.hght = {100, 5500};
                    profile.tmpc = {-3, -30};
                    profile.dwpc = {-5, -35};
                    profile.wdir = {270, 270};
                    profile.wspd = {40, 60};
                    profile.finalize();
                    const auto run = [&] (const char * name, const std::function<void(SoundingAnalysis&)>& set, double pwMean = -9999.0) {
                        SoundingAnalysis a;
                        a.effective.valid = true;
                        a.effective.botAgl = 0.0;
                        a.ml.valid = a.mu.valid = a.sb.valid = true;
                        a.ml.cin = a.mu.cin = -10.0;
                        a.sb.lclHght = 500.0;
                        a.ml.lclHght = 700.0;
                        set(a);
                        SoundingHazard::Input in;
                        in.pwMeanIn = pwMean;
                        in.pwSigmaIn = pwMean > 0 ? 0.2 : -9999.0;
                        fprintf(stderr, "HAZTEST %s -> %s\n", name, SoundingHazard::classify(profile, a, in).type.c_str());
                    };
                    run("pds", [] (SoundingAnalysis& a) { a.stpEffective = 4; a.stpFixed = 4; a.srh01 = 300; a.effectiveSrh = 300; a.kin46.stormRelative = {20, 0}; a.kin8.shear = {30, 20}; a.lowRh = a.midRh = 70; });
                    run("tor (stpE>=3)", [] (SoundingAnalysis& a) { a.stpEffective = 3.2; a.stpFixed = 1; a.srh01 = 100; });
                    run("tor (shear)", [] (SoundingAnalysis& a) { a.stpEffective = 1.2; a.kin46.stormRelative = {20, 0}; });
                    run("mrgl tor", [] (SoundingAnalysis& a) { a.stpEffective = 1.2; a.stpFixed = 0.3; });
                    run("mrgl tor 2", [] (SoundingAnalysis& a) { a.stpEffective = 1.2; a.effectiveSrh = 160; a.ml.cin = -140; });
                    run("svr", [] (SoundingAnalysis& a) { a.supercell = 5; });
                    run("mrgl svr", [] (SoundingAnalysis& a) { a.hail = 0.6; });
                    run("flash flood", [] (SoundingAnalysis& a) { a.precipitableWaterIn = 2.0; a.corfidi.upshear = {5, 5}; a.corfidi.downshear = {10, 10}; }, 1.0);
                    run("blizzard", [] (SoundingAnalysis& a) { a.precipitableWaterIn = 0.2; });
                    run("heat", [] (SoundingAnalysis& a) { a.surfaceRh = 60; });
                    run("none", [] (SoundingAnalysis&) {});
                }
                if (qEnvironmentVariableIsSet("WXQT_EXPORTDLG")) {   // WXQT_EXPORTDLG=<file>: the reusable export window with sample kinds and options, as a picture, then quits
                    const auto file = qEnvironmentVariable("WXQT_EXPORTDLG");
                    ExportDialog dialog{nullptr, "Export", "Save what is on the map. An area at full color applies to the picture of the view and to a model chart; the lighter ACIS map to the picture of the view."};
                    dialog.addKind("view", "The view as it is (PNG or PDF)", "");
                    dialog.addKind("world", "The whole world on a world map projection", "");
                    dialog.addKind("titled", "The model chart, titled (the Model Viewer's picture)", "");
                    dialog.addKind("loop", "The model chart as a loop of hours, titled", "");
                    dialog.selectKind("titled");
                    dialog.addOption("area", "Area at full color (the rest lighter)", {{"", "None (the whole picture at full color)"}, {"CWA:SGF", "Office SGF  Springfield, MO"}}, "CWA:SGF", {"view", "titled"});
                    dialog.addOption("fill", "Lighter ACIS map behind the whole picture", {{"", "No fill"}, {"avgt-dep", "Average temperature, departure from normal"}}, "", {"view"});
                    QTimer::singleShot(600, &dialog, [&dialog, file] { dialog.grab().save(file); dialog.reject(); });
                    dialog.exec();
                }
                if (qEnvironmentVariableIsSet("WXQT_MENU")) {   // WXQT_MENU=<file>: the one right-click save menu on a picture (and on a widget) as a picture, then quits
                    const auto file = qEnvironmentVariable("WXQT_MENU");
                    QImage sample{300, 200, QImage::Format_ARGB32};
                    sample.fill(QColor{80, 140, 200});
                    QByteArray bytes;
                    QBuffer buffer{&bytes};
                    buffer.open(QIODevice::WriteOnly);
                    sample.save(&buffer, "PNG");
                    QTimer::singleShot(600, [file] {
                        if (auto * popup = QApplication::activePopupWidget()) {
                            popup->grab().save(file);
                            popup->close();
                        }
                    });
                    QWidget anchor;
                    ChartExport::Capabilities caps;
                    caps.still = bytes;
                    caps.title = "sample picture";
                    caps.zoom = [] {};
                    caps.saveLoop = [] {};
                    anchor.show();
                    ChartExport::popupMenu(&anchor, QPoint{100, 100}, caps);
                }
                if (const auto spec = qEnvironmentVariable("WXQT_DRAW"); !spec.isEmpty()) {   // WXQT_DRAW=<model>:<chart>:<hour>: draws that chart once and says how long it took
                    GfsRender::Session session;
                    std::string error;
                    QElapsedTimer timer;
                    timer.start();
                    const auto bytes = GfsRender::png(session, spec.section(':', 0, 0).toStdString(), spec.section(':', 1, 1).toStdString(), qEnvironmentVariable("WXQT_SECTOR", "CONUS").toStdString(), qEnvironmentVariable("WXQT_DRAW_RUN").toStdString(), spec.section(':', 2, 2).toInt(), {}, error, nullptr, [] { GfsRender::Variant v; v.standardDeviations = qEnvironmentVariableIsSet("WXQT_DRAW_SIGMA"); v.scale = qEnvironmentVariableIntValue("WXQT_DRAW_WIDTH") / 1100.0; return v; }());
                    if (const auto out = qEnvironmentVariable("WXQT_DRAW_PNG"); !out.isEmpty()) {   // WXQT_DRAW_PNG=<file> (WXQT_SECTOR=<area id>): the picture too
                        QFile file{out};
                        if (file.open(QIODevice::WriteOnly)) file.write(bytes);
                    }
                    fprintf(stderr, "MAGDRAW %s %s in %.1f s %s\n", spec.toStdString().c_str(), bytes.isEmpty() ? "FAILED" : "ok", timer.elapsed() / 1000.0, error.c_str());
                }
                if (const auto listed = qEnvironmentVariable("WXQT_LIST").toStdString(); !listed.empty()) {   // WXQT_LIST=<model>: the model's charts, id | label | picker group
                    for (const auto& product : GfsChart::products()) {
                        if (product.source == listed) {
                            fprintf(stderr, "MAGLIST %s | %s | %s\n", product.id.c_str(), product.label.c_str(), GfsChart::category(product).c_str());
                        }
                    }
                }
                if (const auto list = qEnvironmentVariable("WXQT_SOURCECHECK"); !list.isEmpty()) {   // WXQT_SOURCECHECK=<model,model>: Settings' source check of those models
                    GfsRender::Session session;
                    for (const auto& model : list.split(',')) {
                        const auto check = GfsRender::checkSource(session, model.toStdString());
                        fprintf(stderr, "%s: %s %s (%.1f s) %s\n", model.toStdString().c_str(), check.ok ? "OK" : (check.skipped ? "skipped" : "PROBLEM"), check.run.c_str(), check.seconds, check.detail.c_str());
                    }
                }
                if (const auto spec = qEnvironmentVariable("WXQT_AVAIL"); !spec.isEmpty()) {   // WXQT_AVAIL=<model>:<chart>: the hours of the newest run the chart can be drawn for (from the run's indexes)
                    const auto model = spec.section(':', 0, 0).toStdString(), chart = spec.section(':', 1, 1).toStdString();
                    std::string cycle, date;
                    const auto * def = GfsModels::find(model);
                    if (def != nullptr && GfsRender::latestCycle(model, cycle, "", &date)) {
                        std::vector<int> asked;
                        for (const auto& h : def->hours) {
                            for (int t = h.from; t <= h.to; t += std::max(h.step, 1)) {
                                asked.push_back(t);
                            }
                        }
                        GfsRender::Session session;
                        QElapsedTimer timer;
                        timer.start();
                        const auto found = GfsRender::availableHours(session, model, chart, date + cycle.substr(0, 2), asked);
                        std::string list;
                        for (const int h : found) { list += " " + std::to_string(h); }
                        fprintf(stderr, "MAGAVAIL %s %s %s%s: %d of %d hours in %.1f s:%s\n", model.c_str(), chart.c_str(), date.c_str(), cycle.c_str(), static_cast<int>(found.size()), static_cast<int>(asked.size()), timer.elapsed() / 1000.0, list.c_str());
                    }
                }
                if (const auto model = qEnvironmentVariable("WXQT_RENDER").toStdString(); !model.empty()) {   // WXQT_RENDER=<model>: draw every chart of that model once (hour 3, newest run) and say which could not be
                    GfsRender::Session session;
                    for (const auto& product : GfsChart::products()) {
                        if (product.source == model) {
                            std::string error;
                            int hour = qEnvironmentVariableIntValue("WXQT_HOUR") > 0 ? qEnvironmentVariableIntValue("WXQT_HOUR") : 3;   // (WXQT_HOUR=<n>: another hour)
                            if (model == "SPCREFS") {   // a field of SPC's store has its own hours (the 24 hour periods end at 00 and 12Z): the first one it has, as the screen's hour list offers
                                std::string cycle, date;
                                if (GfsRender::latestCycle(model, cycle, "", &date)) {
                                    const auto hours = SpcRefsLoader::fieldHours(GfsData::Run{date, cycle.substr(0, 2)}, [&product] { std::vector<std::string> v; for (const auto& w : product.wants) { v.push_back(w.variable); } return v; }());
                                    if (!hours.empty() && std::find(hours.begin(), hours.end(), hour) == hours.end()) {
                                        hour = hours.front();
                                    }
                                }
                            }
                            const auto bytes = GfsRender::png(session, model, product.id, "CONUS", "", hour, {}, error, nullptr);
                            fprintf(stderr, "MAGRENDER %s %s %s\n", product.id.c_str(), bytes.isEmpty() ? "FAILED" : "ok", error.c_str());
                        }
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("scalecheck:")) {   // WXQT_OPEN=scalecheck:<chart>:<model,model>:<yyyyMMddHH UTC>:<lat>,<lon>;<lat>,<lon>: each model's fill value at those places (a check of the units and scale factors), then quits
                const auto parts = route.split(':');
                auto when = QDateTime::fromString(parts.value(3), "yyyyMMddHH");
                when.setTimeZone(QTimeZone::utc());
                std::vector<std::string> models;
                for (const auto& m : parts.value(2).split(',', Qt::SkipEmptyParts)) {
                    models.push_back(m.toStdString());
                }
                const auto sources = WorldRender::loadSources(parts.value(1).toStdString(), models, when);
                for (const auto& place : parts.value(4).split(';', Qt::SkipEmptyParts)) {
                    const auto ll = place.split(',');
                    const double lat = ll.value(0).toDouble(), lon = ll.value(1).toDouble();
                    for (const auto& s : sources) {
                        float v = s.grid.sample(lon, lat);
                        if (std::isnan(v)) {
                            v = s.grid.sample(lon + 360.0, lat);
                        }
                        qWarning("scalecheck %6.2f %7.2f  %-9s %s F%-3d  fill %s  grid %dx%d step %.4f", lat, lon, s.model.c_str(), s.run.c_str(), s.hour, std::isnan(v) ? "--" : QString::number(v, 'f', 2).toUtf8().constData(), s.grid.columns, s.grid.rows, s.grid.step);
                    }
                }
                QTimer::singleShot(100, &a, &QCoreApplication::quit);
            } else if (route.startsWith("looprange:")) {   // WXQT_OPEN=looprange:<yyyyMMddHHmm from>:<yyyyMMddHHmm to>:<layer id>: the master map looping between the two times
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto from = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm"), to = QDateTime::fromString(parts.value(2), "yyyyMMddHHmm");
                from.setTimeZone(QTimeZone::utc());
                to.setTimeZone(QTimeZone::utc());
                map->openAt(to, 36.0, -97.0, 900, {parts.value(3).toStdString()});
                map->applyRange(from, to);
            } else if (route.startsWith("mcoptions:")) {   // WXQT_OPEN=mcoptions:<out.png>: the model chart layer's option controls on their own, with the chart picker opened
                auto * layer = new ModelChartLayer;
                auto * options = layer->options(nullptr, [] {});
                options->setMinimumWidth(320);
                options->show();
                options->findChildren<QPushButton *>().at(1)->click();
                const QString out = route.mid(10);
                QTimer::singleShot(2000, options, [options, out] {
                    options->grab().save(out);
                    for (auto * dialog : QApplication::topLevelWidgets()) {
                        if (dialog != options && dialog->isVisible()) {
                            dialog->grab().save(out + ".picker.png");
                        }
                    }
                });
            } else if (route == "network") {   // WXQT_OPEN=network: the list of what the network client has queued and in flight
                NetStatus::show(&w);
            } else if (route == "cache") {   // WXQT_OPEN=cache: the summary of what is kept on disk
                new CacheSummaryViewer{&w};
            } else if (route.startsWith("master:")) {   // WXQT_OPEN=master:<the start of a view's name>: the master map on that view (the preset the box lists)
                new MasterMapViewer{&w, route.mid(7).toStdString()};
            } else if (route.startsWith("bufkit:")) {   // WXQT_OPEN=bufkit:KSGF[:RRFS]: a BUFKIT model sounding of that station
                const auto parts = route.split(':');
                new SoundingViewer{&w, parts.value(2, "RRFS").toStdString(), parts.value(1).toStdString(), true};
            } else if (route.startsWith("recon:")) {   // WXQT_OPEN=recon:<NHC id>:<name>, e.g. recon:al092026:Isaias: the one-flight recon page
                const auto parts = route.split(':');
                new ReconViewer{&w, parts.value(1).toStdString(), parts.value(2).toStdString()};
            } else if (route.startsWith("season:")) {   // WXQT_OPEN=season:al (or ep): the seasons screen, loaded here (it blocks until the data is read)
                auto seasons = std::make_shared<HurricaneData::SeasonData>();
                HurricaneData::loadSeason(*seasons, route.section(':', 1, 1).toStdString());
                new SeasonViewer{&w, seasons};
            } else if (route.startsWith("thumb:")) {   // WXQT_OPEN=thumb:<home thumbnail token>:<out file>: what that home tile would show
                const auto bytes = HomeThumbnails::fetch(route.section(':', 1, 1).toStdString());
                QFile out{route.section(':', 2, 2)};
                if (out.open(QIODevice::WriteOnly)) out.write(bytes);
                fprintf(stderr, "thumb: %lld bytes\n", static_cast<long long>(bytes.size()));
                std::_Exit(0);
            } else if (route.startsWith("sar:")) {   // WXQT_OPEN=sar:<NHC id>:<name>:<year>, e.g. sar:ep202026:Simon:2026: NOAA STAR's SAR winds of the storm
                new SarWindsViewer{&w, route.section(':', 1, 1).toStdString(), route.section(':', 2, 2).toStdString(), route.section(':', 3, 3).toInt()};
            } else if (route == "drought") {   // WXQT_OPEN=drought: the drought dashboard (WXQT_TAB=<n> for its other tabs)
                new DroughtViewer{&w};
            } else if (route.startsWith("fetchtest:")) {   // WXQT_OPEN=fetchtest:<url>: two requests for the url at once through the network client, with what it did (retries, sharing), then quit
                const auto url = route.mid(10).toStdString();
                QElapsedTimer timer;
                timer.start();
                auto first = std::async(std::launch::async, [&url] { return URL::getBytesWithStatus(url, *new int); });
                auto second = std::async(std::launch::async, [&url] { return URL::getBytesWithStatus(url, *new int); });
                const auto a = first.get(), b = second.get();
                const auto t = NetManager::totals();
                fprintf(stderr, "fetchtest: %lld and %lld bytes in %.2f s, finished %lld, shared %lld, retried %lld\n", static_cast<long long>(a.size()), static_cast<long long>(b.size()), timer.elapsed() / 1000.0,
                        t.requests, t.reused, t.retried);
                std::_Exit(0);
            } else if (route.startsWith("variant:")) {   // WXQT_OPEN=variant:<model>:<chart>:<area>:<hour>:<change|max>:<hours back | number of hours>:<out.png>: a change / maximum chart written to a file, then quit
                const auto parts = route.split(':');
                if (parts.size() >= 8) {
                    GfsRender::Session session;
                    GfsRender::Variant variant;
                    variant.member = qEnvironmentVariable("WXQT_MEMBER").toStdString();   // WXQT_MEMBER=p05 / m002: one member of an ensemble's chart
                    const int hour = parts[4].toInt(), n = parts[6].toInt();
                    if (parts[5] == "plain") {
                        variant.kind = GfsRender::Variant::Kind::None;
                    } else if (parts[5] == "change") {
                        variant.kind = GfsRender::Variant::Kind::Change;
                        variant.hoursBack = n;
                    } else {
                        variant.kind = GfsRender::Variant::Kind::Max;
                        for (int h = hour - n; h <= hour; h += 3) variant.hours.push_back(h);
                    }
                    std::string error;
                    QElapsedTimer timer;
                    timer.start();
                    auto bytes = GfsRender::png(session, parts[1].toStdString(), parts[2].toStdString(), parts[3].toStdString(), "", hour, {}, error, nullptr, variant);
                    // a chart that waits for something worked out in the background (the standard deviations): ask again every 20 s, up to WXQT_WAIT minutes
                    for (int waited = 0; bytes.isEmpty() && error.find("worked out") != std::string::npos && waited < qEnvironmentVariableIntValue("WXQT_WAIT") * 3; waited++) {
                        fprintf(stderr, "variant: %s\n", error.c_str());
                        QThread::sleep(20);
                        error.clear();
                        bytes = GfsRender::png(session, parts[1].toStdString(), parts[2].toStdString(), parts[3].toStdString(), "", hour, {}, error, nullptr, variant);
                    }
                    QFile out{parts[7]};
                    if (out.open(QIODevice::WriteOnly)) out.write(bytes);
                    fprintf(stderr, "variant: %lld bytes %.2f s, %.2f MB received %s\n", static_cast<long long>(bytes.size()), timer.elapsed() / 1000.0, NetManager::totals().bytes / 1048576.0, error.c_str());
                }
                std::_Exit(0);
            } else if (route.startsWith("cfsprobe:")) {   // WXQT_OPEN=cfsprobe:<variable key>:<month yyyymm>:<lat>:<lon>: prints the CFS run's field values (no window), then quits
                const auto parts = route.split(':');
                const auto * variable = UtilityCfs::find(parts.value(1, "t2m").toStdString());
                UtilityCfs::Run run;
                std::string error;
                GfsGrid::Grid grid;
                if (variable == nullptr || !UtilityCfs::latestRun(run)) {
                    qWarning("cfsprobe: no run (%s)", variable == nullptr ? "unknown variable" : "none found");
                } else {
                    qWarning("cfsprobe: newest run %s", UtilityCfs::runText(run).c_str());
                    const auto months = UtilityCfs::months(run);
                    const auto month = parts.value(2).isEmpty() ? months[1] : parts.value(2).toStdString();
                    const double lat = parts.value(3, "39.1").toDouble(), lon = parts.value(4, "-94.6").toDouble();
                    for (int member = 1; member <= 2; member++) {
                        if (UtilityCfs::monthly(run, member, *variable, month, grid, error)) {
                            qWarning("cfsprobe: member %d %s monthly mean %s: grid %dx%d, value at %.1f,%.1f = %.2f %s", member, variable->key.c_str(), month.c_str(), grid.columns, grid.rows, lat, lon,
                                     grid.sample(lon < 0 ? lon + 360.0 : lon, lat), variable->units.c_str());
                        } else {
                            qWarning("cfsprobe: member %d monthly %s: %s", member, month.c_str(), error.c_str());
                        }
                    }
                    for (const bool deviations : {false, true}) {
                        if (UtilityCfs::monthlyDeparture(run, 1, *variable, month, deviations, 1999, grid, error)) {
                            qWarning("cfsprobe: member 1 %s %s departure from the 1999-2010 normal at %.1f,%.1f = %.2f %s", variable->key.c_str(), month.c_str(), lat, lon, grid.sample(lon < 0 ? lon + 360.0 : lon, lat), deviations ? "standard deviations" : variable->units.c_str());
                        } else {
                            qWarning("cfsprobe: departure (%s): %s", deviations ? "deviations" : "units", error.c_str());
                        }
                    }
                    for (const int hour : {6}) {
                        if (UtilityCfs::daily(run, 1, *variable, hour, grid, error)) {
                            qWarning("cfsprobe: member 1 %s f%d: grid %dx%d lon0 %.1f lat0 %.1f, value at %.1f,%.1f = %.2f %s", variable->key.c_str(), hour, grid.columns, grid.rows, grid.lon0, grid.lat0, lat, lon,
                                     grid.sample(lon < 0 ? lon + 360.0 : lon, lat), variable->units.c_str());
                        } else {
                            qWarning("cfsprobe: f%d: %s", hour, error.c_str());
                        }
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("cfsscan:")) {   // WXQT_OPEN=cfsscan:<a GRIB2 file>: the index lines made from its messages (a check of CfsArchiveIndex::scan)
                QFile file{route.mid(8)};
                if (file.open(QIODevice::ReadOnly)) {
                    const auto text = CfsArchiveIndex::scan(file.readAll());
                    qWarning("cfsscan: %zu lines", static_cast<size_t>(std::count(text.begin(), text.end(), '\n')));
                    int shown = 0;
                    for (const auto& line : QString::fromStdString(text).split('\n')) {
                        if (shown++ < 400 && (line.contains(":HGT:500 mb") || line.contains(":TMP:850 mb") || line.contains("PRMSL") || line.contains(":UGRD:500") || line.contains(":VGRD:500") || shown < 4)) {
                            qWarning("cfsscan: %s", qPrintable(line));
                        }
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("cfsinterp:")) {   // WXQT_OPEN=cfsinterp:<variable key>:<lead month>: the blended normal of a start date (from the 1st and 16th) against the file of that date
                const auto parts = route.split(':');
                const auto * variable = UtilityCfs::find(parts.value(1, "t2m").toStdString());
                const int lead = parts.value(2, "1").toInt();
                if (variable != nullptr) {
                    for (const int month : {1, 4, 7, 10}) {
                        for (const int day : {6, 11, 21, 26}) {
                            const QDate d{2025, month, day};
                            GfsGrid::Grid truth, blended;
                            std::string error;
                            const auto name = d.toString("yyyyMMdd").toStdString();
                            if (!UtilityCfs::climatologyOn(*variable, name, lead, false, 1999, truth, error) || !UtilityCfs::climatology(*variable, {name, "00"}, lead, false, 1999, blended, error)) {
                                qWarning("cfsinterp: %s: %s", qPrintable(d.toString()), error.c_str());
                                continue;
                            }
                            double square = 0.0, usSquare = 0.0, biggest = 0.0;
                            size_t n = 0, usN = 0;
                            for (int r = 0; r < truth.rows; r++) {
                                for (int c = 0; c < truth.columns; c++) {
                                    const size_t i = static_cast<size_t>(r) * static_cast<size_t>(truth.columns) + static_cast<size_t>(c);
                                    const double e = blended.values[i] - truth.values[i];
                                    if (std::isnan(e)) {
                                        continue;
                                    }
                                    square += e * e;
                                    biggest = std::max(biggest, std::abs(e));
                                    n++;
                                    const double lat = truth.lat0 - r * truth.step, lon = truth.lon0 + c * truth.step;
                                    if (lat > 24.0 && lat < 50.0 && ((lon > -125.0 && lon < -66.0) || (lon > 235.0 && lon < 294.0))) {
                                        usSquare += e * e;
                                        usN++;
                                    }
                                }
                            }
                            if (day == 26) {   // the size of the field and of its spread, to read the error against
                                GfsGrid::Grid spread;
                                if (UtilityCfs::climatologyOn(*variable, name, lead, true, 1999, spread, error)) {
                                    double mean = 0.0, spreadMean = 0.0;
                                    size_t k = 0;
                                    for (size_t i = 0; i < truth.values.size(); i++) {
                                        if (!std::isnan(truth.values[i]) && !std::isnan(spread.values[i])) {
                                            mean += std::abs(truth.values[i]);
                                            spreadMean += spread.values[i];
                                            k++;
                                        }
                                    }
                                    qWarning("cfsinterp: %s %s: the normal averages %.2f in size, its year-to-year spread %.2f %s", variable->key.c_str(), qPrintable(d.toString("MM-dd")), mean / static_cast<double>(k), spreadMean / static_cast<double>(k), variable->units.c_str());
                                }
                            }
                            qWarning("cfsinterp: %s lead %d %s: rms %.3f (US %.3f), largest %.2f %s", variable->key.c_str(), lead, qPrintable(d.toString("MM-dd")), std::sqrt(square / static_cast<double>(n)),
                                     std::sqrt(usSquare / static_cast<double>(std::max<size_t>(1, usN))), biggest, variable->units.c_str());
                        }
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("cfsnormal:")) {   // WXQT_OPEN=cfsnormal:<variable key>:<out.png>[:<lead month>]: the CFS normal at 1 degree beside the same on a 2 degree grid, and their difference
                const auto parts = route.split(':');
                const auto * variable = UtilityCfs::find(parts.value(1, "t2m").toStdString());
                UtilityCfs::Run run;
                std::string error;
                GfsGrid::Grid full;
                const int lead = parts.value(3, "1").toInt();
                if (variable != nullptr && UtilityCfs::latestRun(run) && UtilityCfs::climatology(*variable, run, lead, false, 1999, full, error)) {
                    // the 2 degree grid: the mean of each block of four cells
                    GfsGrid::Grid coarse;
                    coarse.columns = full.columns / 2;
                    coarse.rows = full.rows / 2;
                    coarse.step = full.step * 2.0;
                    coarse.lon0 = full.lon0 + full.step / 2.0;
                    coarse.lat0 = full.lat0 - full.step / 2.0;
                    coarse.values.assign(static_cast<size_t>(coarse.columns) * static_cast<size_t>(coarse.rows), std::nanf(""));
                    for (int r = 0; r < coarse.rows; r++) {
                        for (int c = 0; c < coarse.columns; c++) {
                            double sum = 0.0;
                            int n = 0;
                            for (int dr = 0; dr < 2; dr++) {
                                for (int dc = 0; dc < 2; dc++) {
                                    const float v = full.at(c * 2 + dc, r * 2 + dr);
                                    if (!std::isnan(v)) {
                                        sum += v;
                                        n++;
                                    }
                                }
                            }
                            if (n > 0) {
                                coarse.values[static_cast<size_t>(r) * static_cast<size_t>(coarse.columns) + static_cast<size_t>(c)] = static_cast<float>(sum / n);
                            }
                        }
                    }
                    // back to 1 degree cells (bicubic) and the difference
                    GfsGrid::Grid back = full, diff = full;
                    double sumSquare = 0.0, biggest = 0.0, sumAbs = 0.0, usSquare = 0.0, usBiggest = 0.0;
                    size_t count = 0, usCount = 0;
                    for (int r = 0; r < full.rows; r++) {
                        for (int c = 0; c < full.columns; c++) {
                            const double lon = full.lon0 + c * full.step, lat = full.lat0 - r * full.step;
                            const float b = coarse.sampleCubic(lon, lat), f = full.at(c, r);
                            const size_t i = static_cast<size_t>(r) * static_cast<size_t>(full.columns) + static_cast<size_t>(c);
                            back.values[i] = b;
                            diff.values[i] = b - f;
                            if (!std::isnan(b) && !std::isnan(f)) {
                                const double d = b - f;
                                sumSquare += d * d;
                                sumAbs += std::abs(d);
                                biggest = std::max(biggest, std::abs(d));
                                count++;
                                if (lat > 24.0 && lat < 50.0 && lon > 235.0 - 360.0 && lon < 294.0 - 360.0 || lat > 24.0 && lat < 50.0 && lon > 235.0 && lon < 294.0) {   // the contiguous United States
                                    usSquare += d * d;
                                    usBiggest = std::max(usBiggest, std::abs(d));
                                    usCount++;
                                }
                            }
                        }
                    }
                    const auto fullPacked = qCompress(QByteArray::fromStdString(GfsClimate::pack(full)), 6).size();
                    const auto coarsePacked = qCompress(QByteArray::fromStdString(GfsClimate::pack(coarse)), 6).size();
                    qWarning("cfsnormal: %s lead %d: 1 degree %lld bytes stored, 2 degree %lld bytes (%.0f%%); 2 degree back on 1 degree minus the full: mean |d| %.3f, rms %.3f, largest %.2f %s",
                             variable->key.c_str(), lead, static_cast<long long>(fullPacked), static_cast<long long>(coarsePacked), 100.0 * static_cast<double>(coarsePacked) / static_cast<double>(fullPacked), sumAbs / static_cast<double>(count),
                             std::sqrt(sumSquare / static_cast<double>(count)), biggest, variable->units.c_str());
                    qWarning("cfsnormal: over the contiguous United States: rms %.3f, largest %.2f %s (%zu cells)", std::sqrt(usSquare / static_cast<double>(std::max<size_t>(1, usCount))), usBiggest, variable->units.c_str(), usCount);
                    // three maps, the difference at a tenth of the range of the field
                    double lo = 1e30, hi = -1e30;
                    for (const float v : full.values) {
                        if (!std::isnan(v)) {
                            lo = std::min<double>(lo, v);
                            hi = std::max<double>(hi, v);
                        }
                    }
                    const auto shade = [] (double f) {   // blue to red
                        f = std::clamp(f, 0.0, 1.0);
                        return f < 0.5 ? qRgb(static_cast<int>(40 + 430 * f), static_cast<int>(80 + 340 * f), static_cast<int>(200 + 50 * f)) : qRgb(static_cast<int>(250 - 60 * (f - 0.5) * 2), static_cast<int>(250 - 210 * (f - 0.5) * 2), static_cast<int>(250 - 210 * (f - 0.5) * 2));
                    };
                    const int w = full.columns * 2, h = full.rows * 2;
                    QImage out{w, h * 3 + 90, QImage::Format_RGB32};
                    out.fill(Qt::white);
                    const auto paintMap = [&] (const GfsGrid::Grid& g, int top, double a, double b, bool differenceMap) {
                        for (int y = 0; y < h; y++) {
                            for (int x = 0; x < w; x++) {
                                const float v = g.at(x / 2, y / 2);
                                out.setPixel(x, top + y, std::isnan(v) ? qRgb(230, 230, 230) : shade((v - a) / (b - a)));
                            }
                        }
                        Q_UNUSED(differenceMap)
                    };
                    paintMap(full, 30, lo, hi, false);
                    paintMap(back, 60 + h, lo, hi, false);
                    const double range = std::max(0.2, biggest);
                    paintMap(diff, 90 + 2 * h - 30, -range, range, true);
                    QPainter label{&out};
                    label.setPen(Qt::black);
                    label.drawText(8, 20, QString{"%1, lead %2: the CFS normal at 1 degree (%3 to %4 %5)"}.arg(variable->label.c_str()).arg(lead).arg(lo, 0, 'f', 0).arg(hi, 0, 'f', 0).arg(variable->units.c_str()));
                    label.drawText(8, 50 + h, "the same through a 2 degree grid (block mean, bicubic back)");
                    label.drawText(8, 80 + 2 * h - 30 + 0, QString{"2 degree minus 1 degree, blue to red = -%1 to +%1 %2"}.arg(range, 0, 'f', 2).arg(variable->units.c_str()));
                    label.end();
                    out.save(parts.value(2, "/tmp/cfsnormal.png"));
                } else {
                    qWarning("cfsnormal: %s", error.c_str());
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("gfsprobe")) {   // WXQT_OPEN=gfsprobe: the newest GFS runs' 2 m temperature at Kansas City every 6 hours to 5 days: how many values each could read
                UtilityCfs::Run run;
                std::vector<int> hours;
                for (int h = 6; h <= 120; h += 6) {
                    hours.push_back(h);
                }
                if (UtilityCfs::gfsLatestRun(run)) {
                    for (const auto& past : UtilityCfs::recentRuns(run, 8, 6)) {
                        std::vector<float> values;
                        std::string error;
                        const bool ok = UtilityCfs::gfsPointSeries(past, UtilityCfs::gfsVariables().front(), hours, 39.1, -94.6, values, error);
                        QString line;
                        int good = 0;
                        for (size_t i = 0; i < values.size(); i++) {
                            good += std::isnan(values[i]) ? 0 : 1;
                            line += std::isnan(values[i]) ? " -" : " +";
                        }
                        qWarning("gfsprobe: run %s ok=%d %d of %zu [%s]%s", UtilityCfs::runText(past).c_str(), ok ? 1 : 0, good, values.size(), error.c_str(), qPrintable(line));
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("cfsplume")) {   // WXQT_OPEN=cfsplume[:<cfs|gfs>:<runs>:<hours between>]: the plume window
                const auto parts = route.split(':');
                auto * plume = new CfsPlumeViewer{&w};
                if (parts.size() > 1) {
                    plume->choose(parts.value(1), parts.value(2, "4").toInt(), parts.value(3, "6").toInt(), parts.value(4, "0").toInt());
                }
            } else if (route.startsWith("storm:")) {   // WXQT_OPEN=storm:<basin>:<NHC id>: the track map on that storm
                new HurricaneViewer{&w, route.section(':', 1, 1).toStdString(), route.section(':', 2, 2).toStdString()};
            } else if (route.startsWith("systemmap:")) {   // WXQT_OPEN=systemmap:<ATCF id>:lat:lon:km: the master map of an other-basin storm, ensemble tracks of that system only
                auto * map = new MasterMapViewer{&w};
                map->openAt(QDateTime::currentDateTimeUtc(), route.section(':', 2, 2).toDouble(), route.section(':', 3, 3).toDouble(), route.section(':', 4, 4).toDouble(),
                    {"tropical/world-storms", "tropical/spaghetti", "satellite/sar-winds"});
                map->showSystem(route.section(':', 1, 1).toStdString());
            } else if (route.startsWith("aceforecast")) {   // WXQT_OPEN=aceforecast: the ACE forecast of the Weather Lab large ensemble
                new AceForecastViewer{&w};
            } else if (route.startsWith("hafs:")) {   // WXQT_OPEN=hafs:<NHC id>: the hurricane model screen on that storm
                Utility::writePref("NCEP", "HAFSA");
                Utility::writePref("MODELNCEPPARAMLASTUSED", qEnvironmentVariable("WXQT_PARAM", "wind_mslp").toStdString());
                Utility::writePref("MODELNCEPSECTORLASTUSED", Hafs::modelId(route.section(':', 1, 1).toStdString()));
                new ModelViewer{&w, "NCEP"};
            } else if (route.startsWith("hafsintensity:")) {   // WXQT_OPEN=hafsintensity:<model id, e.g. 15e>: the intensity of that storm from HAFS-A and HAFS-B
                const auto storm = route.section(':', 1, 1).toStdString();
                std::string cycle, other;
                const auto a = GfsRender::hafsTrack("HAFSA", storm, cycle), b = GfsRender::hafsTrack("HAFSB", storm, other);
                new HafsIntensityViewer{&w, storm, a, b, cycle};
            } else if (route.startsWith("modelpicker:")) {   // WXQT_OPEN=modelpicker:<GFS|RRFS|GEFS ...>: the model screen on that model with its chart picker open
                Utility::writePref("NCEP", route.section(':', 1, 1).toStdString());
                Utility::writePref("MODELNCEPPARAMLASTUSED", qEnvironmentVariable("WXQT_PARAM", "500_wnd_ht").toStdString());   // WXQT_PARAM=<chart id>: open on that chart
                Utility::writePref("MODELNCEPSECTORLASTUSED", "CONUS");
                auto * viewer = new ModelViewer{&w, "NCEP"};
                qEnvironmentVariable("WXQT_PICK") == "net" ? NetStatus::show(viewer) : qEnvironmentVariable("WXQT_PICK") == "sector" ? viewer->showSectorPicker() : qEnvironmentVariable("WXQT_PICK") == "model" ? viewer->showModelPicker() : viewer->showPicker();   // WXQT_PICK=sector: the area picker
                if (qEnvironmentVariable("WXQT_PICK") == "precip") {   // WXQT_PICK=precip: the chart picker's "Precip by parts..." opened
                    QTimer::singleShot(800, viewer, [viewer] {
                        for (auto * button : viewer->findChildren<QPushButton *>()) {
                            if (button->text().startsWith("Precip by parts")) {
                                button->click();
                            }
                        }
                    });
                }
                if (const auto file = qEnvironmentVariable("WXQT_PICKER_PNG"); !file.isEmpty()) {   // a picture of the picker itself (WXQT_PICKER_TITLE: the one whose title has this)
                    QTimer::singleShot(3000, viewer, [viewer, file] {
                        for (auto * picker : viewer->findChildren<QDialog *>()) {
                            if (qEnvironmentVariable("WXQT_PICKER_TITLE").isEmpty() || picker->windowTitle().contains(qEnvironmentVariable("WXQT_PICKER_TITLE"))) {
                                picker->grab().save(file);
                            }
                        }
                    });
                }
            } else if (route.startsWith("goesbig:")) {   // WXQT_OPEN=goesbig:<sector>: the GOES viewer as the home screen thumbnail opens it (the size nearest 1200 pixels wide)
                auto * goes = new GoesViewer{&w, "", "GEOCOLOR", route.section(':', 1, 1).toStdString(), false};
                goes->preferWidth(1200);
            } else if (route.startsWith("satcheck:")) {   // WXQT_OPEN=satcheck:<tle file>[:<hours since epoch, comma list>]: x y z (km) of each satellite, to compare with a reference SGP4
                QFile file{route.section(':', 1, 1)};
                file.open(QIODevice::ReadOnly);
                const auto sats = SatelliteOrbit::parseAll(file.readAll().toStdString());
                for (const auto& tle : sats) {
                    SatelliteOrbit::Propagator p{tle};
                    for (const double hours : {0.0, 6.0, 24.0, 48.0}) {
                        const auto s = p.at(tle.epochJd + hours / 24.0);
                        qWarning("satcheck %s %.0f %.4f %.4f %.4f %.6f %.6f %.6f", tle.name.c_str(), hours, s.x, s.y, s.z, s.vx, s.vy, s.vz);
                    }
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("satat:")) {   // WXQT_OPEN=satat:<catalog>:<lat>:<lon>:<yyyyMMddHHmm UTC>:<hours>: the passes of one satellite over a place, to check against a known time
                const auto parts = route.split(':');
                string problem;
                const auto tle = SatellitePasses::elements(parts.value(1).toInt(), problem);
                auto when = QDateTime::fromString(parts.value(4), "yyyyMMddHHmm");
                when.setTimeZone(QTimeZone::utc());
                SatelliteOrbit::Propagator propagator{tle};
                qWarning("satat %s epoch age %.1f days %s", tle.name.c_str(), when.toSecsSinceEpoch() / 86400.0 + 2440587.5 - tle.epochJd, problem.c_str());
                for (const auto& p : SatelliteOrbit::passes(propagator, parts.value(2).toDouble(), parts.value(3).toDouble(), when.toSecsSinceEpoch(), parts.value(5).toDouble(), {"SAR", 700, 0})) {
                    qWarning("satat pass %s  %.0f km %s", QDateTime::fromSecsSinceEpoch(p.time, QTimeZone::utc()).toString("yyyy-MM-dd HH:mm:ss").toUtf8().constData(), p.distanceKm, p.ascending ? "north" : "south");
                }
                QTimer::singleShot(100, [] { QApplication::quit(); });
            } else if (route.startsWith("satpass")) {   // WXQT_OPEN=satpass[:<storm id or place>]: the satellite pass schedule
                new SatellitePassViewer{&w, route.section(':', 1, 1).toStdString()};
            } else if (route.startsWith("ascat:")) {   // WXQT_OPEN=ascat:<ATCF id>: the ASCAT ultra high resolution winds of that storm (wp292026)
                new AscatUhrViewer{&w, route.section(':', 1, 1).toStdString()};
            } else if (route.startsWith("atcffeed:")) {   // WXQT_OPEN=atcffeed:<id>: the raw ATCF feed of a storm (wp292026, al932026)
                new AtcfFeedViewer{&w, route.section(':', 1, 1).toStdString(), route.section(':', 1, 1).toStdString()};
            } else if (route == "tropicalhub") {   // WXQT_OPEN=tropicalhub: the Tropical Hub
                Dashboards::openTropicalHub(&w);
            } else if (route.startsWith("cirastorm:")) {   // WXQT_OPEN=cirastorm:<id>:<title>: a storm's CIRA page (with the newest SAR winds)
                new CiraStorm{&w, route.section(':', 1, 1).toStdString(), route.section(':', 2).toStdString()};
            } else if (route == "tornadoyears") {   // WXQT_OPEN=tornadoyears: the tornado years ranked
                new TornadoYearsViewer{&w, TornadoData::load(), 0, std::string{}};
            } else if (route.startsWith("swostate:")) {   // WXQT_OPEN=swostate:<day>: the SPC convective outlook's state graphics for that day
                new SpcSwoStateGraphics{&w, route.section(':', 1, 1).toInt()};
            } else {
                w.openRoute(route.toStdString());
            }
            for (auto * widget : QApplication::topLevelWidgets()) {
                if (widget != &w && !before.contains(widget) && qobject_cast<QMainWindow *>(widget) != nullptr) {   // not a combo box's hidden popup
                    opened = widget;
                }
            }
        }
        if (const auto size = qEnvironmentVariable("WXQT_SIZE").split('x'); opened != nullptr && size.size() == 2) {
            opened->resize(size[0].toInt(), size[1].toInt());   // WXQT_SIZE=1400x1000: the tool window's size for the picture
        }
        if (const auto size = qEnvironmentVariable("WXQT_SIZE").split('x'); opened == nullptr && size.size() == 2) {
            w.resize(size[0].toInt(), size[1].toInt());   // no tool opened: the home screen itself
        }
        if (const auto tab = qEnvironmentVariable("WXQT_TAB"); opened != nullptr && !tab.isEmpty()) {
            for (auto * tabs : opened->findChildren<QTabWidget *>()) {
                tabs->setCurrentIndex(tab.toInt());   // WXQT_TAB=<n>: show that tab of a tabbed tool (Settings)
            }
        }
        const auto grab = qEnvironmentVariable("WXQT_GRAB").split(',');
        if (!grab[0].isEmpty()) {
            const auto delay = grab.size() > 1 ? grab[1].toInt() : 8000;
            QTimer::singleShot(delay, &a, [&w, &a, opened, file = grab[0]] {
                (opened != nullptr ? opened->grab() : w.grab()).save(file);
                a.quit();
            });
        }
        // the contact-email question, once the window is up (not in the development screenshot runs, which have nobody to answer)
        if (grab[0].isEmpty() && qEnvironmentVariable("QT_QPA_PLATFORM") != "offscreen") {
            QTimer::singleShot(0, &w, [&w] { ContactPrompt::askIfMissing(&w); });
        }
        return a.exec();
    }
}
