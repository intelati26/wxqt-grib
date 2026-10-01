// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef HOMELAYOUTEDITOR_H
#define HOMELAYOUTEDITOR_H

#include <functional>
#include <string>
#include <vector>
#include <QPoint>
#include <QRect>
#include <QWidget>

// The visual editor for HomeLayout: a strip of layout thumbnails (click one to choose it) and, below it, a diagram
// of the chosen layout with a chip for each large section. Drag a chip to another zone (or to another place in
// the same zone) to move it; clicking a chip instead opens a menu of the zones.
class HomeLayoutEditor : public QWidget {
public:
    HomeLayoutEditor(QWidget * parent, std::function<void()> onChange);
    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;
    bool hasHeightForWidth() const override;
    int heightForWidth(int) const override;   // the strip of layouts wraps, so the height depends on the width

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dragMoveEvent(QDragMoveEvent *) override;
    void dragLeaveEvent(QDragLeaveEvent *) override;
    void dropEvent(QDropEvent *) override;

private:
    struct Chip {
        std::string section;
        QRect rect;
    };
    QRect thumbRect(int templateIndex) const;
    QRect diagramRect() const;
    QRect zoneRect(int zone) const;
    std::vector<Chip> chips() const;
    int zoneAt(const QPoint&) const;                  // -1 outside every zone
    int stripRows(int width) const;
    void moveTo(const std::string& section, int zone, int position);
    void showZoneMenu(const std::string& section, const QPoint& globalPos);

    std::function<void()> onChange;
    QPoint pressPos;
    std::string pressedSection;   // chip under the press, if any
    int pressedThumb{-1};
    int dropZone{-1};             // zone highlighted while dragging over it
};

#endif  // HOMELAYOUTEDITOR_H
