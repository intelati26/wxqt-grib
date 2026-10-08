// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTEXPORT_H
#define CHARTEXPORT_H

#include <QImage>
#include <QString>
#include <QWidget>

// Every chart can be saved: install() gives a widget a right-click menu with "Save as PNG...", "Save as PDF..." (vector, so it scales) and "Copy to the
// clipboard", all drawn by the widget's own paint code at twice the screen resolution, with a footer line naming the chart and the time it was saved.
namespace ChartExport {
    void install(QWidget * chart, const QString& title);
    QImage render(QWidget * chart, const QString& title, int scale = 2);
    bool savePng(QWidget * chart, const QString& title, const QString& path);
    bool savePdf(QWidget * chart, const QString& title, const QString& path);
}

#endif  // CHARTEXPORT_H
