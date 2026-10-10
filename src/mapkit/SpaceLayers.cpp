// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/SpaceLayers.h"
#include <algorithm>
#include <cmath>
#include "space/SpaceData.h"

string AuroraLayer::summary() const {
    if (loading && !data) return "reading the aurora forecast...";
    if (!data || !data->ok) return {};
    return "aurora forecast for " + data->forecast.substr(11, 5) + "Z";
}

void AuroraLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto fresh = std::make_shared<UtilitySpace::Ovation>();
    host.background([fresh] { *fresh = SpaceData::loadOvation(); }, [this, &host, fresh] {
        loading = false;
        if (fresh->ok) {
            data = fresh;
        }
        host.redraw();
    });
}

QColor AuroraLayer::colorOf(float chance) {
    // faint green for a few percent, bright green, yellow, orange, red and magenta as the chance rises
    const int alpha = std::clamp(static_cast<int>(35 + chance * 3.2), 0, 215);
    if (chance < 15.0f) return QColor{60, 220, 110, alpha};
    if (chance < 30.0f) return QColor{110, 235, 60, alpha};
    if (chance < 50.0f) return QColor{235, 230, 50, alpha};
    if (chance < 70.0f) return QColor{250, 150, 40, alpha};
    if (chance < 85.0f) return QColor{240, 60, 50, alpha};
    return QColor{230, 50, 200, alpha};
}

void AuroraLayer::paint(QPainter& painter, MapHost& host) {
    if (!data || !data->ok) {
        return;
    }
    const auto t = host.view().transform();
    painter.setPen(Qt::NoPen);
    for (int row = 0; row <= 180; row++) {
        const double lat = row - 90.0;
        for (int column = 0; column < 360; column++) {
            const float chance = data->grid[static_cast<size_t>(row) * 360 + static_cast<size_t>(column)];
            if (chance < 3.0f) {
                continue;
            }
            const double lon = column > 180 ? column - 360.0 : column;
            const QPointF a = t(lat - 0.5, lon - 0.5);
            const QPointF b = t(lat + 0.5, lon + 0.5);
            if (b.x() < -520.0 || a.x() > 520.0 || a.y() < -270.0 || b.y() > 770.0) {
                continue;
            }
            painter.setBrush(colorOf(chance));
            painter.drawRect(QRectF{std::min(a.x(), b.x()), std::min(a.y(), b.y()), std::abs(b.x() - a.x()) + 0.6, std::abs(b.y() - a.y()) + 0.6});
        }
    }
}

MapHit AuroraLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -2;
    if (!data || !data->ok) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const float chance = data->at(lat, lon);
    if (chance >= 3.0f) {
        hit.distance = 0.0;
        hit.text = "Aurora: " + QString::number(static_cast<int>(chance)) + "% chance\n(OVATION, forecast for " + QString::fromStdString(data->forecast.substr(11, 5)) + "Z)";
    }
    return hit;
}

vector<MapLegendRow> AuroraLayer::legend() const {
    MapLegendRow row;
    row.title = "Aurora chance:";
    for (const auto& [label, chance] : {std::pair<const char *, float>{"<15%", 10.0f}, {"15-30", 20.0f}, {"30-50", 40.0f}, {"50-70", 60.0f}, {"70-85", 75.0f}, {"85%+", 90.0f}}) {
        auto c = colorOf(chance);
        c.setAlpha(255);
        row.entries.push_back({MapLegendEntry::Square, c, label});
    }
    return {row};
}
