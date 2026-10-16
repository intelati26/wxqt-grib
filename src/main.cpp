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
#include "cfs/CfsPlumeViewer.h"
#include "cfs/UtilityCfs.h"
#include "gfs/GfsClimate.h"
#include "hurricane/ReconViewer.h"
#include "radar/MapWidget.h"
#include "drought/DroughtViewer.h"
#include "hurricane/SarWindsViewer.h"
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
#include "spcrefs/SpcRefsLoader.h"
#include "hurricane/HafsViewer.h"
#include "hurricane/HurricaneViewer.h"
#include "mapkit/MasterMapViewer.h"
#include "models/ModelScreen.h"
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
            } else if (route.startsWith("goesat:")) {   // WXQT_OPEN=goesat:<yyyyMMddHHmm UTC>: the master map at that time with the GOES history layer
                const auto parts = route.split(':');
                auto * map = new MasterMapViewer{&w};
                auto when = QDateTime::fromString(parts.value(1), "yyyyMMddHHmm");
                when.setTimeSpec(Qt::UTC);
                map->openAt(when, 37.0, -97.0, 900.0, {"satellite/goes-history"});
            } else if (route.startsWith("modelat:")) {   // WXQT_OPEN=modelat:<model>:<yyyyMMddHHmm UTC>: the Model Viewer on that model at the run and hour of that time
                const auto parts = route.split(':');
                auto when = QDateTime::fromString(parts.value(2), "yyyyMMddHHmm");
                when.setTimeSpec(Qt::UTC);
                auto * screen = new ModelScreen{&w, "NCEP"};
                screen->openAt(parts.value(1).toStdString(), when);
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
            } else if (route.startsWith("cfsinterp:")) {   // WXQT_OPEN=cfsinterp:<variable key>:<lead month>: how well the normal of a start date is had by interpolating between start dates further apart
                const auto parts = route.split(':');
                const auto * variable = UtilityCfs::find(parts.value(1, "t2m").toStdString());
                const int lead = parts.value(2, "1").toInt();
                if (variable != nullptr) {
                    const auto get = [&] (const QDate& d, GfsGrid::Grid& g) {
                        std::string error;
                        return UtilityCfs::climatology(*variable, {d.toString("yyyyMMdd").toStdString(), "00"}, lead, false, 1999, g, error);
                    };
                    struct Spacing {
                        const char * name;
                        int before, after;   // days either side of the 16th
                    };
                    const Spacing spacings[] = {{"5 days either side (the stored spacing)", 5, 5}, {"10 days either side", 10, 10}, {"15 days either side (1st and 1st)", 15, 15}, {"30 days either side (16th to 16th)", 30, 30}, {"60 days either side", 60, 60}};
                    for (const int month : {1, 4, 7, 10}) {
                        const QDate target{2025, month, 16};
                        GfsGrid::Grid truth;
                        if (!get(target, truth)) {
                            qWarning("cfsinterp: no normal for %s", qPrintable(target.toString()));
                            continue;
                        }
                        for (const auto& spacing : spacings) {
                            // the stored dates nearest the asked spacing (the 1st, 6th ... of a month)
                            const auto snap = [] (QDate d) {
                                const int day = std::min(26, (d.day() - 1) / 5 * 5 + 1);
                                return QDate{d.year(), d.month(), day};
                            };
                            const QDate a = snap(target.addDays(-spacing.before)), b = snap(target.addDays(spacing.after));
                            GfsGrid::Grid ga, gb;
                            if (!get(a, ga) || !get(b, gb) || ga.values.size() != truth.values.size()) {
                                continue;
                            }
                            const double wb = static_cast<double>(a.daysTo(target)) / static_cast<double>(a.daysTo(b));
                            double square = 0.0, biggest = 0.0, usSquare = 0.0;
                            size_t n = 0, usN = 0;
                            for (int r = 0; r < truth.rows; r++) {
                                for (int c = 0; c < truth.columns; c++) {
                                    const size_t i = static_cast<size_t>(r) * static_cast<size_t>(truth.columns) + static_cast<size_t>(c);
                                    const double guess = ga.values[i] * (1.0 - wb) + gb.values[i] * wb, d = guess - truth.values[i];
                                    if (std::isnan(d)) {
                                        continue;
                                    }
                                    square += d * d;
                                    biggest = std::max(biggest, std::abs(d));
                                    n++;
                                    const double lat = truth.lat0 - r * truth.step, lon = truth.lon0 + c * truth.step;
                                    if (lat > 24.0 && lat < 50.0 && lon > 235.0 - 360.0 && lon < 294.0 - 360.0 || lat > 24.0 && lat < 50.0 && lon > 235.0 && lon < 294.0) {
                                        usSquare += d * d;
                                        usN++;
                                    }
                                }
                            }
                            qWarning("cfsinterp: %s lead %d, the 16th of month %d from %s to %s (%s): rms %.3f (US %.3f), largest %.2f %s", variable->key.c_str(), lead, month, qPrintable(a.toString("MM-dd")), qPrintable(b.toString("MM-dd")),
                                     spacing.name, std::sqrt(square / static_cast<double>(n)), std::sqrt(usSquare / static_cast<double>(std::max<size_t>(1, usN))), biggest, variable->units.c_str());
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
            } else if (route.startsWith("hafs:")) {   // WXQT_OPEN=hafs:<NHC id>: the hurricane model screen on that storm
                new HafsViewer{&w, route.section(':', 1, 1).toStdString(), ""};
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
                if (const auto file = qEnvironmentVariable("WXQT_PICKER_PNG"); !file.isEmpty()) {   // a picture of the picker itself
                    for (auto * picker : viewer->findChildren<QDialog *>()) {
                        QTimer::singleShot(3000, picker, [picker, file] { picker->grab().save(file); });
                    }
                }
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
