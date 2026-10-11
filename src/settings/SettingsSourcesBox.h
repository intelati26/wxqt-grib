// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSSOURCESBOX_H
#define SETTINGSSOURCESBOX_H

#include <atomic>
#include <memory>
#include <QPointer>
#include <QWidget>
#include "gfs/GfsRender.h"

class QLabel;
class QPushButton;
class QTableWidget;

// The "Model sources" tab of the settings: for each model the Model Viewer draws, whether the server still has a run and lists the first chart's fields (the data moves and
// retires: a source that has gone is found here and not by a chart that fails). It looks only when the tab is entered, or on Check again; nothing runs in the background otherwise.
class SettingsSourcesBox : public QWidget {
public:
    explicit SettingsSourcesBox(QWidget * parent = nullptr);
    ~SettingsSourcesBox() override;
    void entered();   // the tab became the one shown: look (once; a look in progress is left to finish)

private:
    void start();
    void setRow(int row, const GfsRender::SourceCheck&);
    QTableWidget * table{};
    QLabel * summary{};
    QPushButton * again{};
    std::shared_ptr<GfsRender::Session> session;
    std::shared_ptr<std::atomic<int>> generation;   // a look that is not the newest one drops its answers
    bool running{false};
    bool looked{false};
};

#endif  // SETTINGSSOURCESBOX_H
