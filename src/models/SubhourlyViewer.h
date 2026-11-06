// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SUBHOURLYVIEWER_H
#define SUBHOURLYVIEWER_H

#include <string>
#include <vector>
#include <QDateTimeEdit>
#include <QSpinBox>
#include "models/LoopWindow.h"

// The subhourly surface fields of the RRFS and the HRRR (their 15 minute files): one run, the minutes of its lead from one time to another, one to four charts side by side (the panes, pickers
// and hover values of LoopWindow, the Model Viewer's). Only the charts whose fields are in those files are offered (the radar, 2 m temperature and dew point, wind and gust, visibility,
// precipitation rate ...: the surface, no pressure levels, no hourly maxima). The times are drawn in the background and play as they arrive.
class SubhourlyViewer : public LoopWindow {
public:
    // run: "yyyyMMddHH" ("" the newest run that has the files); charts: the chart ids to begin with
    SubhourlyViewer(Window * parent, const std::string& model, const std::string& sector, const std::string& run, const std::vector<std::string>& charts);
    void draw() { build(); }   // as the button does (for a development aid)
    // the window on the newest run of that model, the radar and the surface fields in four panes
    static SubhourlyViewer * openNewest(Window * parent, const std::string& model);
    std::string summary() const;   // (a development aid: what the window was opened on)
    void setLeads(int fromMinutes, int toMinutes) {   // (a development aid)
        fromLead->setValue(fromMinutes);
        toLead->setValue(toMinutes);
    }

protected:
    std::string pickerModel() const override { return model; }
    std::vector<std::string> areaIds() const override;
    void build() override;
    void renderFrame(size_t index, int mine, bool again) override;
    QString captionOf(size_t frame, size_t pane) const override;
    std::string titleOf(size_t frame) const override;
    std::string saveName() const override { return model + "_15min_" + runId; }
    int delayMs() const override { return 300; }

private:
    void switchModel(const std::string& id);
    void findNewestRun();
    void listCharts();
    std::string model;
    std::string runId;                     // the run drawn ("yyyyMMddHH")
    std::vector<int> minutes;              // the lead of each step of the loop
    QComboBox * modelBox{};
    QDateTimeEdit * runEdit{};
    QSpinBox * fromLead{}, * toLead{};
    QComboBox * step{};
    QPushButton * newestButton{};
    int done{0}, failed{0};
    std::string firstError;
};

#endif  // SUBHOURLYVIEWER_H
