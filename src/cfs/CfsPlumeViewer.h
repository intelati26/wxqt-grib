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
#include <QTimer>
#include "cfs/UtilityCfs.h"
#include "ui/Window.h"

// The CFSv2 forecast at a place: a field's daily values for the next days from each of the four newest runs (member 1 of each cycle: whole nine month forecasts six hours apart; the other members are shorter) and, if asked, the same cycle of the days before, and below them the monthly
// means for the nine months ahead. A place is a saved location or any latitude and longitude. Each value is one field read by byte range and sampled, so a plume costs about 15 MB the first
// time and nothing after (the fields are kept by the model cache).
class CfsPlume;

class CfsPlumeViewer : public Window {
public:
    explicit CfsPlumeViewer(Window * parent, double lat = 39.1, double lon = -94.6);
    // set the model ("cfs", "gfs"), the number of runs and the hours between them, and load (a route of the development aids, and a way to open it ready)
    void choose(const QString& modelId, int runs, int everyHours);

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
        std::vector<double> days;                // days from the run's time, for the daily lines
        std::vector<Line> daily;
        std::vector<QString> runNames;           // the four runs: "00Z 16 Oct" ...
        std::vector<QString> months;             // "Nov" ...
        std::vector<Line> monthly;
    };

private:
    void load();
    void fillChoices();
    void closeEventCustom() override { closed = true; }
    QComboBox * place{};
    QDoubleSpinBox * latBox{};
    QDoubleSpinBox * lonBox{};
    QComboBox * model{};
    QComboBox * field{};
    QComboBox * span{};
    QComboBox * count{};
    QComboBox * stride{};
    QPushButton * go{};
    QLabel * status{};
    CfsPlume * chart{};
    QTimer progress;
    std::shared_ptr<std::atomic<int>> done{std::make_shared<std::atomic<int>>(0)};
    std::shared_ptr<std::atomic<int>> total{std::make_shared<std::atomic<int>>(0)};
    bool closed{false};
    bool loading{false};
    bool again{false};          // the choices changed while a load was going on: load again when it ends
    int generation{0};
};

#endif  // CFSPLUMEVIEWER_H
