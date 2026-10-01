// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/HomeLayoutEditor.h"
#include <algorithm>
#include <numeric>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include "settings/HomeLayout.h"

namespace {
    constexpr int thumbWidth = 84;
    constexpr int thumbHeight = 46;
    constexpr int gap = 8;
    constexpr int diagramMaxWidth = 640;
    constexpr int chipHeight = 28;
    constexpr int chipGap = 5;
    constexpr int zoneInset = 6;
    // room in every zone row for the zone's number and all four section chips
    constexpr int zoneRowHeight = 22 + 4 * (chipHeight + chipGap) + 2 * zoneInset;

    int diagramHeight() {
        return HomeLayout::templates()[HomeLayout::templateIndex()].rows * zoneRowHeight;
    }
    const char * mimeType = "application/x-wxqt-home-section";

    // the rectangle of a zone inside `area`: columns share the width by their stretch, rows share the height equally
    QRect placeZone(const HomeLayout::Template& layoutTemplate, const HomeLayout::Zone& zone, const QRect& area, int inset) {
        const int total = std::accumulate(layoutTemplate.columnStretch.begin(), layoutTemplate.columnStretch.end(), 0);
        int before = 0;
        for (int c = 0; c < zone.col; c += 1) {
            before += layoutTemplate.columnStretch[c];
        }
        int span = 0;
        for (int c = zone.col; c < zone.col + zone.colSpan; c += 1) {
            span += layoutTemplate.columnStretch[c];
        }
        const double x0 = area.left() + area.width() * static_cast<double>(before) / total;
        const double x1 = area.left() + area.width() * static_cast<double>(before + span) / total;
        const double y0 = area.top() + area.height() * static_cast<double>(zone.row) / layoutTemplate.rows;
        const double y1 = area.top() + area.height() * static_cast<double>(zone.row + zone.rowSpan) / layoutTemplate.rows;
        return QRect{QPoint{static_cast<int>(x0), static_cast<int>(y0)}, QPoint{static_cast<int>(x1), static_cast<int>(y1)}}
            .adjusted(inset, inset, -inset, -inset);
    }
}

HomeLayoutEditor::HomeLayoutEditor(QWidget * parent, std::function<void()> onChange)
    : QWidget{parent}
    , onChange{std::move(onChange)}
{
    setAcceptDrops(true);
    QSizePolicy policy{QSizePolicy::Preferred, QSizePolicy::Preferred};
    policy.setHeightForWidth(true);
    setSizePolicy(policy);
}

int HomeLayoutEditor::stripRows(int w) const {
    const int perRow = std::max(1, (w + gap) / (thumbWidth + gap));
    const int count = static_cast<int>(HomeLayout::templates().size());
    return (count + perRow - 1) / perRow;
}

bool HomeLayoutEditor::hasHeightForWidth() const {
    return true;
}

int HomeLayoutEditor::heightForWidth(int w) const {
    return stripRows(w) * (thumbHeight + gap) + gap + diagramHeight();
}

QSize HomeLayoutEditor::sizeHint() const {
    return {diagramMaxWidth, heightForWidth(diagramMaxWidth)};
}

QSize HomeLayoutEditor::minimumSizeHint() const {
    return {thumbWidth * 2 + gap, thumbHeight + gap * 2 + diagramHeight()};
}

QRect HomeLayoutEditor::thumbRect(int templateIndex) const {
    const int perRow = std::max(1, (width() + gap) / (thumbWidth + gap));
    return QRect{(templateIndex % perRow) * (thumbWidth + gap), (templateIndex / perRow) * (thumbHeight + gap), thumbWidth, thumbHeight};
}

QRect HomeLayoutEditor::diagramRect() const {
    const int top = stripRows(width()) * (thumbHeight + gap) + gap;
    return QRect{0, top, std::min(width(), diagramMaxWidth), diagramHeight()};
}

QRect HomeLayoutEditor::zoneRect(int zone) const {
    const auto& layoutTemplate = HomeLayout::templates()[HomeLayout::templateIndex()];
    return placeZone(layoutTemplate, layoutTemplate.zones[zone], diagramRect(), zoneInset);
}

std::vector<HomeLayoutEditor::Chip> HomeLayoutEditor::chips() const {
    std::vector<Chip> out;
    const auto zoneCount = static_cast<int>(HomeLayout::templates()[HomeLayout::templateIndex()].zones.size());
    for (int zone = 0; zone < zoneCount; zone += 1) {
        const auto area = zoneRect(zone);
        int y = area.top() + 22;   // below the zone's number
        for (const auto& section : HomeLayout::sectionsIn(zone)) {
            out.push_back({section, QRect{area.left() + 6, y, area.width() - 12, chipHeight}});
            y += chipHeight + chipGap;
        }
    }
    return out;
}

int HomeLayoutEditor::zoneAt(const QPoint& point) const {
    const auto zoneCount = static_cast<int>(HomeLayout::templates()[HomeLayout::templateIndex()].zones.size());
    for (int zone = 0; zone < zoneCount; zone += 1) {
        if (zoneRect(zone).contains(point)) {
            return zone;
        }
    }
    return -1;
}

void HomeLayoutEditor::paintEvent(QPaintEvent *) {
    QPainter p{this};
    p.setRenderHint(QPainter::Antialiasing);
    const auto& pal = palette();
    const auto line = pal.color(QPalette::Mid);
    const auto fill = pal.color(QPalette::AlternateBase);
    const auto accent = pal.color(QPalette::Highlight);
    const auto& all = HomeLayout::templates();

    // the strip of layouts
    for (int index = 0; index < static_cast<int>(all.size()); index += 1) {
        const auto box = thumbRect(index);
        const bool chosen = index == HomeLayout::templateIndex();
        p.setPen(QPen{chosen ? accent : line, chosen ? 2.0 : 1.0});
        p.setBrush(pal.color(QPalette::Base));
        p.drawRoundedRect(box.adjusted(1, 1, -1, -1), 4, 4);
        p.setPen(Qt::NoPen);
        p.setBrush(chosen ? accent.lighter(150) : fill.darker(115));
        for (const auto& zone : all[index].zones) {
            p.drawRoundedRect(placeZone(all[index], zone, box, 4), 2, 2);
        }
    }

    // the chosen layout, large
    for (int zone = 0; zone < static_cast<int>(all[HomeLayout::templateIndex()].zones.size()); zone += 1) {
        const auto area = zoneRect(zone);
        p.setPen(QPen{zone == dropZone ? accent : line, zone == dropZone ? 2.5 : 1.0});
        p.setBrush(zone == dropZone ? accent.lighter(170) : fill);
        p.drawRoundedRect(area, 6, 6);
        p.setPen(pal.color(QPalette::PlaceholderText));
        p.drawText(area.adjusted(8, 4, -8, 0), Qt::AlignTop | Qt::AlignLeft, QString{"Zone %1"}.arg(zone + 1));
    }
    for (const auto& chip : chips()) {
        p.setPen(QPen{accent, 1.0});
        p.setBrush(pal.color(QPalette::Button));
        p.drawRoundedRect(chip.rect, 5, 5);
        p.setPen(pal.color(QPalette::ButtonText));
        p.drawText(chip.rect, Qt::AlignCenter, QString::fromStdString(HomeLayout::sectionLabel(chip.section)));
    }
}

void HomeLayoutEditor::mousePressEvent(QMouseEvent * event) {
    pressPos = event->pos();
    pressedSection.clear();
    pressedThumb = -1;
    for (int index = 0; index < static_cast<int>(HomeLayout::templates().size()); index += 1) {
        if (thumbRect(index).contains(pressPos)) {
            pressedThumb = index;
        }
    }
    for (const auto& chip : chips()) {
        if (chip.rect.contains(pressPos)) {
            pressedSection = chip.section;
        }
    }
}

void HomeLayoutEditor::mouseMoveEvent(QMouseEvent * event) {
    if (pressedSection.empty() || !(event->buttons() & Qt::LeftButton) ||
        (event->pos() - pressPos).manhattanLength() < QApplication::startDragDistance()) {
        return;
    }
    const auto section = pressedSection;
    pressedSection.clear();
    auto * mime = new QMimeData;
    mime->setData(mimeType, QByteArray::fromStdString(section));
    auto * drag = new QDrag{this};
    drag->setMimeData(mime);
    drag->exec(Qt::MoveAction);
    dropZone = -1;
    update();
}

void HomeLayoutEditor::mouseReleaseEvent(QMouseEvent * event) {
    const bool click = (event->pos() - pressPos).manhattanLength() < QApplication::startDragDistance();
    if (click && pressedThumb >= 0 && thumbRect(pressedThumb).contains(event->pos())) {
        HomeLayout::setTemplate(pressedThumb);
        updateGeometry();   // the diagram's height depends on the layout
        update();
        onChange();
    } else if (click && !pressedSection.empty()) {
        showZoneMenu(pressedSection, event->globalPosition().toPoint());
    }
    pressedSection.clear();
    pressedThumb = -1;
}

void HomeLayoutEditor::showZoneMenu(const std::string& section, const QPoint& globalPos) {
    QMenu menu;
    const auto zoneCount = static_cast<int>(HomeLayout::templates()[HomeLayout::templateIndex()].zones.size());
    std::vector<QAction *> actions;
    for (int zone = 0; zone < zoneCount; zone += 1) {
        auto * action = menu.addAction(QString{"Move %1 to zone %2"}.arg(QString::fromStdString(HomeLayout::sectionLabel(section))).arg(zone + 1));
        action->setEnabled(zone != HomeLayout::zoneOf(section));
        actions.push_back(action);
    }
    const auto * chosen = menu.exec(globalPos);
    for (int zone = 0; zone < zoneCount; zone += 1) {
        if (chosen == actions[zone]) {
            moveTo(section, zone, 1 << 20);   // the end of that zone
        }
    }
}

void HomeLayoutEditor::moveTo(const std::string& section, int zone, int position) {
    HomeLayout::move(section, zone, position);
    update();
    onChange();
}

void HomeLayoutEditor::dragEnterEvent(QDragEnterEvent * event) {
    if (event->mimeData()->hasFormat(mimeType)) {
        event->acceptProposedAction();
    }
}

void HomeLayoutEditor::dragMoveEvent(QDragMoveEvent * event) {
    const auto zone = zoneAt(event->position().toPoint());
    if (zone != dropZone) {
        dropZone = zone;
        update();
    }
    if (zone >= 0) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void HomeLayoutEditor::dragLeaveEvent(QDragLeaveEvent *) {
    dropZone = -1;
    update();
}

void HomeLayoutEditor::dropEvent(QDropEvent * event) {
    const auto point = event->position().toPoint();
    const auto zone = zoneAt(point);
    dropZone = -1;
    if (zone < 0 || !event->mimeData()->hasFormat(mimeType)) {
        update();
        return;
    }
    const auto section = event->mimeData()->data(mimeType).toStdString();
    // dropping a chip on its own zone: positions are counted without the chip itself
    int position = 0;
    for (const auto& chip : chips()) {
        if (HomeLayout::zoneOf(chip.section) != zone || chip.section == section) {
            continue;
        }
        if (point.y() < chip.rect.center().y()) {
            break;
        }
        position += 1;
    }
    event->acceptProposedAction();
    moveTo(section, zone, position);
}
