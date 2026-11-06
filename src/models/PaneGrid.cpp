// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/PaneGrid.h"
#include <algorithm>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include "misc/ImageViewer.h"
#include "ui/Window.h"

PaneGrid::PaneGrid(QWidget * parent)
    : QWidget{parent}
    , grid{new QGridLayout{this}}
{
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(4);
    for (size_t i = 0; i < tiles.size(); i++) {
        auto * tile = new CompareTile{this};
        tiles[i] = tile;
        QObject::connect(tile->image, &ZoomImage::clicked, this, [this] (double fx, double fy) {   // a point marked in every pane
            for (auto * t : tiles) {
                t->image->setMarker(fx, fy);
            }
        });
        QObject::connect(tile->image, &ZoomImage::doubleClicked, this, [this, tile] {
            if (!tile->shownBytes.isEmpty()) {
                new ImageViewer{dynamic_cast<Window *>(window()), tile->shownBytes, tile->caption->text().toStdString()};
            }
        });
    }
    setCount(1);
}

void PaneGrid::onChange(const std::function<void(int)>& handler) {
    for (size_t i = 0; i < tiles.size(); i++) {
        QObject::connect(tiles[i]->change, &QPushButton::clicked, this, [handler, i] { handler(static_cast<int>(i)); });
    }
}

void PaneGrid::setCount(int panes) {
    shownCount = std::clamp(panes, 1, maxPanes);
    for (auto * tile : tiles) {
        grid->removeWidget(tile);
    }
    const int columns = shownCount == 4 ? 2 : shownCount;
    for (int i = 0; i < maxPanes; i++) {
        const bool used = i < shownCount;
        tiles[static_cast<size_t>(i)]->setVisible(used);
        if (used) {
            grid->addWidget(tiles[static_cast<size_t>(i)], i / columns, i % columns);
        }
    }
    for (int c = 0; c < 2; c++) {
        grid->setColumnStretch(c, c < columns ? 1 : 0);
    }
    for (int r = 0; r < 2; r++) {
        grid->setRowStretch(r, r * columns < shownCount ? 1 : 0);
    }
}

void PaneGrid::setMessage(const QString& text) {
    for (auto * tile : tiles) {
        tile->setMessage(text);
    }
}

void PaneGrid::hoverAt(double fx, double fy) {
    for (auto * tile : tiles) {
        if (tile->isVisible() && tile->hover) {
            tile->hover->show(fx, fy, 12, 12);
        }
    }
}

QByteArray PaneGrid::composite(const std::vector<QByteArray>& pictures) {
    if (pictures.size() == 1) {
        return pictures.front();
    }
    const int columns = pictures.size() == 4 ? 2 : static_cast<int>(pictures.size());
    const int rows = (static_cast<int>(pictures.size()) + columns - 1) / columns;
    std::vector<QImage> images;
    constexpr int cellWidth = 900;
    int cellHeight = 100;
    for (const auto& bytes : pictures) {
        images.push_back(QImage::fromData(bytes).scaledToWidth(cellWidth, Qt::SmoothTransformation));
        cellHeight = std::max(cellHeight, images.back().height());
    }
    QImage out(columns * cellWidth + (columns - 1) * 6, rows * cellHeight + (rows - 1) * 6, QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter{&out};
    for (size_t i = 0; i < images.size(); i++) {
        painter.drawImage(static_cast<int>(i % static_cast<size_t>(columns)) * (cellWidth + 6), static_cast<int>(i / static_cast<size_t>(columns)) * (cellHeight + 6), images[i]);
    }
    painter.end();
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    out.save(&buffer, "PNG");
    return bytes;
}
