// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SUBHOURLYVIEWER_H
#define SUBHOURLYVIEWER_H

#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <QByteArray>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QGridLayout>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTimer>
#include "gfs/GfsChart.h"
#include "models/CompareTile.h"
#include "models/ProductPicker.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The subhourly surface fields of the RRFS and the HRRR (their 15 minute files): one run, the minutes of its lead from one time to another, one to four charts side by side in the panes of the
// Model Viewer (the hover read-out, the chart and area pickers). Only the charts whose fields are in those files are offered (the radar, 2 m temperature and dew point, wind and gust,
// visibility, precipitation rate ...: the surface, no pressure levels, no hourly maxima). The times are drawn in the background and play as they arrive.
class SubhourlyViewer : public Window {
public:
    // run: "yyyyMMddHH" ("" the newest run that has the files); charts: the chart ids to begin with
    SubhourlyViewer(Window * parent, const std::string& model, const std::string& sector, const std::string& run, const std::vector<std::string>& charts);
    void draw() { build(); }   // as the button does (for a development aid)
    void hoverAt(double fx, double fy) {   // (a development aid)
        for (auto * tile : tiles) {
            if (tile->isVisible() && tile->hover) {
                tile->hover->show(fx, fy, 12, 12);
            }
        }
    }
    void setLeads(int fromMinutes, int toMinutes) {   // (a development aid)
        fromLead->setValue(fromMinutes);
        toLead->setValue(toMinutes);
    }

private:
    struct Cell {
        QByteArray bytes;
        std::shared_ptr<GfsChart::Probe> probe;
    };
    void closeEventCustom() override;
    void build();
    void stop();
    void renderStep(size_t index, int mine, bool again);
    void showFrame(int index);
    void advance();
    void save();
    void applyLayout();
    void pickChart(int pane);
    void pickArea();
    void refreshAreaButton();
    void invalidate(const QString& why);
    void trimProbes();
    void findNewestRun();
    void listCharts();
    QByteArray composite(size_t index) const;
    std::string chartLabel(const std::string& id) const;
    std::string model;
    std::string sector;
    VBox box;
    QComboBox * step{}, * panes{};
    QDateTimeEdit * runEdit{};
    QSpinBox * fromLead{}, * toLead{};
    QPushButton * areaButton{}, * newestButton{}, * buildButton{}, * playButton{}, * saveButton{};
    QLabel * status{};
    QWidget * area{};
    QGridLayout * grid{};
    std::array<CompareTile *, 4> tiles{};
    std::vector<std::string> paneCharts;
    QSlider * slider{};
    QTimer timer;
    std::vector<std::string> chartIds;     // the model's charts that have their fields in the subhourly files, and their names
    std::vector<std::string> chartNames;
    std::vector<int> minutes;              // the lead of each step of the loop
    std::vector<std::vector<Cell>> frames;
    std::vector<QString> frameNames;
    std::vector<size_t> redrawing;
    QPointer<ProductPicker> chartPicker;
    QPointer<ProductPicker> areaPicker;
    int shown{-1};
    int generation{0};
    int done{0}, failed{0};
    std::string firstError;
    std::string runId;                     // the run drawn ("yyyyMMddHH")
    std::shared_ptr<std::atomic<bool>> cancel;
    bool closed{false};
};

#endif  // SUBHOURLYVIEWER_H
