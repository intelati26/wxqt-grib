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
    void install(QWidget * chart, const QString& title);   // title empty: the title of the window the chart is in
    // true while a chart is being drawn for a file or the clipboard AND the menu's "Plots only" is ticked: a chart with tables or explanatory lines leaves them out.
    // The default is the whole chart as on the screen
    bool exporting();
    QImage render(QWidget * chart, const QString& title, int scale = 2);
    QString startFolder();                     // where the last picture was saved (the pictures folder at first)
    void remember(const QString& path);
    QString fileName(const QString& title, const QString& extension);   // "master_map_20261008_1530.png"
    bool savePng(QWidget * chart, const QString& title, const QString& path);
    bool savePdf(QWidget * chart, const QString& title, const QString& path);
}

#endif  // CHARTEXPORT_H
