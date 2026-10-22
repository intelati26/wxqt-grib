// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ChartExport.h"
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QFileDialog>
#include <QImage>
#include <QMenu>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QStandardPaths>
#include "objects/UtilityAnimationExport.h"
#include "util/Utility.h"

namespace {
    const int footer = 18;   // pixels of the saved picture's own line under the chart

    void drawFooter(QPainter& painter, int width, int top, const QString& title) {
        painter.fillRect(QRect{0, top, width, footer}, QColor{245, 245, 245});
        QFont font{painter.font()};
        font.setPixelSize(10);
        painter.setFont(font);
        painter.setPen(QColor{110, 110, 110});
        painter.drawText(QRect{6, top, width - 12, footer}, Qt::AlignVCenter | Qt::AlignLeft, title + "  -  saved " + QDateTime::currentDateTimeUtc().toString("yyyy-MM-dd HH:mm") + " UTC  -  exported by wxqt-grib");
    }

    QString fileNameImpl(const QString& title, const QString& extension) {
        QString base = title.toLower();
        for (auto& c : base) {
            if (!c.isLetterOrNumber()) {
                c = '_';
            }
        }
        return base + "_" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmm") + "." + extension;
    }

    QString startFolderImpl() {
        const auto saved = QString::fromStdString(Utility::readPref("CHART_EXPORT_DIR", ""));
        return saved.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) : saved;
    }

    void rememberImpl(const QString& path) {
        Utility::writePref("CHART_EXPORT_DIR", QFileInfo{path}.absolutePath().toStdString());
    }
}

namespace {
    int exportDepth = 0;
}

QString ChartExport::startFolder() { return startFolderImpl(); }
void ChartExport::remember(const QString& path) { rememberImpl(path); }
QString ChartExport::fileName(const QString& title, const QString& extension) { return fileNameImpl(title, extension); }

QString ChartExport::askSave(QWidget * parent, const QString& caption, const QString& title, const QString& extension, const QString& filter, QString * selectedFilter) {
    const auto path = QFileDialog::getSaveFileName(parent, caption, startFolderImpl() + "/" + fileNameImpl(title, extension), filter, selectedFilter);
    if (!path.isEmpty()) {
        rememberImpl(path);
    }
    return path;
}

bool ChartExport::exporting() {
    // only while a picture is being made, and only when "plots only" is chosen in the menu: the default is the whole chart as it is on the screen
    return exportDepth > 0 && Utility::readPref("CHART_EXPORT_PLOTS_ONLY", "false") == "true";
}

QImage ChartExport::render(QWidget * chart, const QString& titleGiven, int scale) {
    const QString title = titleGiven.isEmpty() ? chart->window()->windowTitle() : titleGiven;
    struct Scope {
        Scope() { exportDepth++; }
        ~Scope() { exportDepth--; }
    } scope;
    QImage image{(chart->width()) * scale, (chart->height() + footer) * scale, QImage::Format_ARGB32};
    image.fill(Qt::white);
    image.setDevicePixelRatio(scale);
    QPainter painter{&image};
    chart->render(&painter, QPoint{}, QRegion{}, QWidget::DrawWindowBackground);   // the chart itself, not the labels laid over it (a map's hover popup)
    drawFooter(painter, chart->width(), chart->height(), title);
    return image;
}

bool ChartExport::savePng(QWidget * chart, const QString& title, const QString& path) {
    return render(chart, title).save(path, "PNG");
}

bool ChartExport::savePdf(QWidget * chart, const QString& titleGiven, const QString& path) {
    const QString title = titleGiven.isEmpty() ? chart->window()->windowTitle() : titleGiven;
    struct Scope {
        Scope() { exportDepth++; }
        ~Scope() { exportDepth--; }
    } scope;
    QPdfWriter writer{path};
    writer.setResolution(150);
    writer.setPageSize(QPageSize{QSizeF{chart->width() * 25.4 / 96.0, (chart->height() + footer) * 25.4 / 96.0}, QPageSize::Millimeter});
    writer.setPageMargins(QMarginsF{0, 0, 0, 0});
    QPainter painter{&writer};
    if (!painter.isActive()) {
        return false;
    }
    const double factor = static_cast<double>(writer.width()) / chart->width();
    painter.scale(factor, factor);
    chart->render(&painter, QPoint{}, QRegion{}, QWidget::DrawWindowBackground);   // the chart itself, not the labels laid over it (a map's hover popup)
    drawFooter(painter, chart->width(), chart->height(), title);
    return true;
}

namespace {
    bool saveImagePdf(const QImage& image, const QString& title, const QString& path) {
        QPdfWriter writer{path};
        writer.setResolution(150);
        writer.setPageSize(QPageSize{QSizeF{image.width() * 25.4 / 150.0, (image.height() + footer * 1.5) * 25.4 / 150.0}, QPageSize::Millimeter});
        writer.setPageMargins(QMarginsF{0, 0, 0, 0});
        QPainter painter{&writer};
        if (!painter.isActive()) {
            return false;
        }
        painter.drawImage(0, 0, image);
        drawFooter(painter, image.width(), image.height(), title);
        return true;
    }
}

void ChartExport::popupMenu(QWidget * anchor, const QPoint& globalPos, const Capabilities& caps) {
    // an empty title is the name of the window the chart is in (a map: "Rivers - NWS gauges")
    const QString title = caps.title.isEmpty() ? anchor->window()->windowTitle() : caps.title;
    const bool painted = caps.paint != nullptr;
    QMenu menu;
    QAction * zoom = caps.zoom ? menu.addAction("Open zoomable view") : nullptr;
    if (zoom != nullptr) {
        menu.addSeparator();
    }
    auto * png = menu.addAction("Save as PNG...");
    auto * pdf = menu.addAction("Save as PDF...");
    menu.addSeparator();
    auto * copy = menu.addAction("Copy to the clipboard");
    QAction * more = nullptr;
    if (caps.saveLoop || !caps.still.isEmpty()) {
        more = menu.addAction(caps.saveLoop ? "More formats, or the loop..." : "More formats (WebP, AVIF, JPEG XL...)...");
    }
    QAction * plots = nullptr;
    if (painted) {
        menu.addSeparator();
        plots = menu.addAction("Plots only (leave out tables and notes)");
        plots->setCheckable(true);
        plots->setChecked(Utility::readPref("CHART_EXPORT_PLOTS_ONLY", "false") == "true");
    }
    const auto * chosen = menu.exec(globalPos);
    if (chosen == nullptr) {
        return;
    }
    if (chosen == zoom) {
        caps.zoom();
    } else if (chosen == plots) {
        Utility::writePref("CHART_EXPORT_PLOTS_ONLY", plots->isChecked() ? "true" : "false");
    } else if (chosen == more) {
        if (caps.saveLoop) {
            caps.saveLoop();
        } else {
            UtilityAnimationExport::saveWithDialog(anchor->window(), {}, 0, caps.still, fileNameImpl(title, "").chopped(1));
        }
    } else if (chosen == copy) {
        QApplication::clipboard()->setImage(painted ? render(caps.paint, title) : QImage::fromData(caps.still));
    } else if (chosen == png) {
        const auto path = askSave(anchor->window(), "Save the picture", title, "png", "PNG image (*.png)");
        if (!path.isEmpty()) {
            if (painted) {
                savePng(caps.paint, title, path);
            } else {
                QImage::fromData(caps.still).save(path, "PNG");
            }
        }
    } else if (chosen == pdf) {
        const auto path = askSave(anchor->window(), "Save the picture", title, "pdf", "PDF document (*.pdf)");
        if (!path.isEmpty()) {
            if (painted) {
                savePdf(caps.paint, title, path);
            } else {
                saveImagePdf(QImage::fromData(caps.still), title, path);
            }
        }
    }
}

void ChartExport::install(QWidget * chart, const QString& title) {
    chart->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(chart, &QWidget::customContextMenuRequested, chart, [chart, title] (const QPoint& at) {
        Capabilities caps;
        caps.paint = chart;
        caps.title = title;
        popupMenu(chart, chart->mapToGlobal(at), caps);
    });
}
