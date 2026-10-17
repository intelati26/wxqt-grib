// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CFSPLUMEVIEWER_H
#define CFSPLUMEVIEWER_H

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <QComboBox>
#include <QDate>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QPointer>
#include <QTimer>
#include <QWidget>
#include "cfs/UtilityCfs.h"
#include "ui/Window.h"

// The CFSv2 forecast at a place: a field's daily values for the next days from each of the four newest runs (member 1 of each cycle: whole nine month forecasts six hours apart; the other members are shorter) and, if asked, the same cycle of the days before, and below them the monthly
// means for the nine months ahead. A place is a saved location or any latitude and longitude. Each value is one field read by byte range and sampled, so a plume costs about 15 MB the first
// time and nothing after (the fields are kept by the model cache).
class CfsPlume;

// The plume as a widget: the Plume window holds one, and the Model Viewer holds one under the chart of the CFS and GEFS pages. `owner` is the window it works for (the background jobs end with it);
// `loadNow` false: it waits until it is asked (setPoint / setModel / reload), so that a page that has it hidden costs nothing.
class CfsPlumePanel : public QWidget {
public:
    CfsPlumePanel(Window * owner, QWidget * parent, double lat, double lon, const QString& modelId, bool loadNow = true);
    // set the model ("cfs", "gfs", "gefs"), the number of runs and the hours between them, and load (a route of the development aids, and a way to open it ready)
    void choose(const QString& modelId, int runs, int everyHours, int monthly = 0);
    // a new place or model: loaded a moment later (a click that moves on, or the page's model changes, asks again before the first is read)
    void setPoint(double lat, double lon);
    void setModel(const QString& modelId);
    void reload() { reloadSoon.start(300); }

    struct Line {
        QString name;
        int member{0};              // 1-4: the four newest runs, newest first; 0 the mean
        double runAgo{0.0};         // days before the newest run (a quarter of a day for the one before it)
        bool earlier{false};        // the same cycle of an earlier day, drawn in gray
        std::vector<float> values;
    };
    struct Result {
        QString title;
        double runFraction{0.0};                 // the run's hour of its day, as a part of a day
        QDate runDay;                            // the run's own day
        QString units;
        QString monthlyUnits;                    // the monthly panel's axis
        std::vector<double> days;                // days from the run's time, for the daily lines
        std::vector<Line> daily;
        bool ensemble{false};                    // the lines are the members of one ensemble run (drawn alike), not runs of a model
        std::vector<float> low, high;            // a band to shade at each day when the members are not drawn (the mean and its spread)
        std::vector<QString> runNames;           // the four runs: "00Z 16 Oct" ...
        std::vector<QString> months;             // "Nov" ...
        std::vector<Line> monthly;
    };

private:
    void load();
    void fillChoices();
    Window * owner{nullptr};
    QTimer reloadSoon;
    QString loadedKey;
    QComboBox * place{};
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * model{};
    QComboBox * field{};
    QComboBox * span{};
    QComboBox * count{};
    QComboBox * stride{};
    QComboBox * monthlyShow{};
    QPushButton * go{};
    QLabel * status{};
    CfsPlume * chart{};
    QTimer progress;
    std::shared_ptr<std::atomic<int>> done{std::make_shared<std::atomic<int>>(0)};
    std::shared_ptr<std::atomic<int>> total{std::make_shared<std::atomic<int>>(0)};
    bool loading{false};
    bool again{false};          // the choices changed while a load was going on: load again when it ends
    int generation{0};
};

// The Plume window: a panel in a window of its own.
class CfsPlumeViewer : public Window {
public:
    explicit CfsPlumeViewer(Window * parent, double lat = 39.1, double lon = -94.6, const QString& modelId = "gefs");
    void choose(const QString& modelId, int runs, int everyHours, int monthly = 0) { panel->choose(modelId, runs, everyHours, monthly); }

private:
    CfsPlumePanel * panel{nullptr};
};

#endif  // CFSPLUMEVIEWER_H
