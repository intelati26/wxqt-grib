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
#include "util/Utility.h"

namespace {
    const int footer = 18;   // pixels of the saved picture's own line under the chart

    void drawFooter(QPainter& painter, int width, int top, const QString& title) {
        painter.fillRect(QRect{0, top, width, footer}, QColor{245, 245, 245});
        QFont font{painter.font()};
        font.setPixelSize(10);
        painter.setFont(font);
        painter.setPen(QColor{110, 110, 110});
        painter.drawText(QRect{6, top, width - 12, footer}, Qt::AlignVCenter | Qt::AlignLeft, title + "  -  saved " + QDateTime::currentDateTimeUtc().toString("d MMM yyyy HH:mm") + " UTC  -  wxqt");
    }

    QString fileName(const QString& title, const QString& extension) {
        QString base = title.toLower();
        for (auto& c : base) {
            if (!c.isLetterOrNumber()) {
                c = '_';
            }
        }
        return base + "_" + QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmm") + "." + extension;
    }

    QString startFolder() {
        const auto saved = QString::fromStdString(Utility::readPref("CHART_EXPORT_DIR", ""));
        return saved.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) : saved;
    }

    void remember(const QString& path) {
        Utility::writePref("CHART_EXPORT_DIR", QFileInfo{path}.absolutePath().toStdString());
    }
}

QImage ChartExport::render(QWidget * chart, const QString& title, int scale) {
    QImage image{(chart->width()) * scale, (chart->height() + footer) * scale, QImage::Format_ARGB32};
    image.fill(Qt::white);
    image.setDevicePixelRatio(scale);
    QPainter painter{&image};
    chart->render(&painter);
    drawFooter(painter, chart->width(), chart->height(), title);
    return image;
}

bool ChartExport::savePng(QWidget * chart, const QString& title, const QString& path) {
    return render(chart, title).save(path, "PNG");
}

bool ChartExport::savePdf(QWidget * chart, const QString& title, const QString& path) {
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
    chart->render(&painter);
    drawFooter(painter, chart->width(), chart->height(), title);
    return true;
}

void ChartExport::install(QWidget * chart, const QString& title) {
    chart->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(chart, &QWidget::customContextMenuRequested, chart, [chart, title] (const QPoint& at) {
        QMenu menu;
        auto * png = menu.addAction("Save as PNG...");
        auto * pdf = menu.addAction("Save as PDF...");
        menu.addSeparator();
        auto * copy = menu.addAction("Copy to the clipboard");
        const auto * chosen = menu.exec(chart->mapToGlobal(at));
        if (chosen == nullptr) {
            return;
        }
        if (chosen == copy) {
            QApplication::clipboard()->setImage(render(chart, title));
        } else if (chosen == png) {
            const auto path = QFileDialog::getSaveFileName(chart, "Save the chart", startFolder() + "/" + fileName(title, "png"), "PNG image (*.png)");
            if (!path.isEmpty()) {
                savePng(chart, title, path);
                remember(path);
            }
        } else if (chosen == pdf) {
            const auto path = QFileDialog::getSaveFileName(chart, "Save the chart", startFolder() + "/" + fileName(title, "pdf"), "PDF document (*.pdf)");
            if (path.isEmpty()) {
                return;
            }
            savePdf(chart, title, path);
            remember(path);
        }
    });
    chart->setToolTip(chart->toolTip().isEmpty() ? "Right-click to save the chart as PNG or PDF, or copy it" : chart->toolTip());
}
