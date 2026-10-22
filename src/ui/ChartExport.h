// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CHARTEXPORT_H
#define CHARTEXPORT_H

#include <functional>
#include <QByteArray>
#include <QImage>
#include <QString>
#include <QWidget>

// Every chart can be saved: install() gives a widget a right-click menu with "Save as PNG...", "Save as PDF..." (vector, so it scales) and "Copy to the
// clipboard", all drawn by the widget's own paint code at twice the screen resolution, with a footer line naming the chart and the time it was saved.
namespace ChartExport {
    // What a screen can save: the one right-click menu for charts and pictures alike. A chart is saved by painting it (PNG at twice the size, vector PDF, the clipboard, a footer line); a picture
    // that is already encoded (downloaded, or rendered by the program) as itself; both end in "more formats and loops" (the animation exporter: APNG, GIF, WebP, AVIF, JPEG XL, MP4).
    struct Capabilities {
        QWidget * paint{nullptr};          // the widget to paint (a chart, a map)
        QByteArray still;                  // or the picture's encoded bytes
        QString title;                     // names the files (empty: the title of the window)
        std::function<void()> zoom;        // "Open zoomable view" (a picture that has one)
        std::function<void()> saveLoop;    // the screen's own save of its loop or picture with its names and headers (empty: the animation exporter on `still`)
    };
    void popupMenu(QWidget * anchor, const QPoint& globalPos, const Capabilities& caps);
    void install(QWidget * chart, const QString& title);   // title empty: the title of the window the chart is in
    // true while a chart is being drawn for a file or the clipboard AND the menu's "Plots only" is ticked: a chart with tables or explanatory lines leaves them out.
    // The default is the whole chart as on the screen
    bool exporting();
    QImage render(QWidget * chart, const QString& title, int scale = 2);
    QString startFolder();                     // where the last picture was saved (the pictures folder at first)
    void remember(const QString& path);
    // the one way to ask where to save anything a screen makes (a picture, a table, a loop): the folder of the last save, a name from the title and the time, and the folder remembered after.
    // Empty when cancelled. `selectedFilter` is filled with the filter chosen when given.
    QString askSave(QWidget * parent, const QString& caption, const QString& title, const QString& extension, const QString& filter, QString * selectedFilter = nullptr);
    QString fileName(const QString& title, const QString& extension);   // "master_map_20261008_1530.png"
    bool savePng(QWidget * chart, const QString& title, const QString& path);
    bool savePdf(QWidget * chart, const QString& title, const QString& path);
}

#endif  // CHARTEXPORT_H
