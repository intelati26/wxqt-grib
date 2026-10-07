// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef VDMVIEWER_H
#define VDMVIEWER_H

#include <memory>
#include <QString>
#include <QWidget>
#include "hurricane/HurricaneData.h"
#include "ui/VBox.h"
#include "ui/Window.h"

// The reconnaissance vortex data messages of a storm: the minimum pressure and the strongest flight-level wind of each centre fix against time, and
// each message as a table row (time, position, pressure, winds inbound and outbound, eye, temperatures, aircraft).
class VdmChart : public QWidget {
public:
    explicit VdmChart(QWidget * parent = nullptr) : QWidget{parent} { setMinimumSize(640, 230); }
    void setData(const std::shared_ptr<HurricaneData::VdmData>& data);

private:
    void paintEvent(QPaintEvent *) override;
    std::shared_ptr<HurricaneData::VdmData> data;
};

class VdmViewer : public Window {
public:
    VdmViewer(Window * parent, const std::shared_ptr<HurricaneData::VdmData>& data, const QString& storm);
    static QString summary(const HurricaneData::VdmData& data);   // "7 vortex messages, latest 00:11Z: 996 mb, max flight-level wind 51 kt"
    static QString timeText(long seconds);                         // "22 Jul 00:11Z"

private:
    VBox box;
};

#endif  // VDMVIEWER_H
