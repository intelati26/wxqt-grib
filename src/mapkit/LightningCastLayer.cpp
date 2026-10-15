// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/LightningCastLayer.h"
#include <algorithm>
#include <memory>
#include <QDateTime>
#include <QRegularExpression>
#include <QTimeZone>
#include "radar/MapView.h"
#include "util/UtilityIO.h"

namespace {
    const char * feed = "https://cimss.ssec.wisc.edu/severe_conv/NOAACIMSS_PLTG_GOES-East_CONUS_LOOP";

    // the GRLevelX placefile: "TimeRange: start end" opens a scan; "Color: r g b" and "Line: width, flags, "text"" open a contour of the percent in the text, its points
    // ("lat, lon") follow, "End:" closes it
    vector<LightningCastLayer::Frame> parse(const QByteArray& bytes) {
        vector<LightningCastLayer::Frame> frames;
        static const QRegularExpression percentRe{">=\\s*(\\d+)%"};
        QColor color;
        int percent = 0;
        bool inLine = false;
        QPolygonF points;
        for (const auto& raw : bytes.split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            if (line.startsWith("TimeRange:")) {
                const auto parts = line.mid(10).trimmed().split(' ', Qt::SkipEmptyParts);
                auto start = QDateTime::fromString(parts.value(0), Qt::ISODate);
                start.setTimeSpec(Qt::UTC);
                if (start.isValid()) {
                    frames.push_back({static_cast<long>(start.toSecsSinceEpoch()), {}});
                }
            } else if (line.startsWith("Color:")) {
                const auto parts = line.mid(6).trimmed().split(' ', Qt::SkipEmptyParts);
                color = QColor{parts.value(0).toInt(), parts.value(1).toInt(), parts.value(2).toInt()};
            } else if (line.startsWith("Line:")) {
                const auto match = percentRe.match(line);
                percent = match.hasMatch() ? match.captured(1).toInt() : 0;
                inLine = true;
                points.clear();
            } else if (line.startsWith("End:")) {
                if (inLine && percent > 0 && points.size() > 1 && !frames.empty()) {
                    frames.back().contours.push_back({percent, color, points});
                }
                inLine = false;
            } else if (inLine && !line.isEmpty() && (line[0].isDigit() || line[0] == '-')) {
                const auto parts = line.split(',');
                if (parts.size() == 2) {
                    points.append(QPointF{parts[1].trimmed().toDouble(), parts[0].trimmed().toDouble()});
                }
            }
        }
        return frames;
    }
}

void LightningCastLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    loading = true;
    auto result = std::make_shared<vector<Frame>>();
    auto empty = std::make_shared<bool>(false);
    host.background(
        [result, empty] {
            const auto bytes = UtilityIO::downloadAsByteArray(feed);
            *empty = bytes.isEmpty();
            if (!bytes.isEmpty()) {
                *result = parse(bytes);
            }
        },
        [this, &host, result, empty] {
            loading = false;
            if (*empty || result->empty()) {
                error = "the LightningCast feed could not be read";
            } else {
                error.clear();
                frames = std::move(*result);
                std::sort(frames.begin(), frames.end(), [] (const Frame& a, const Frame& b) { return a.seconds < b.seconds; });
            }
            host.timesChanged();
            host.redraw();
        });
}

vector<long> LightningCastLayer::times() const {
    vector<long> all;
    for (const auto& frame : frames) {
        all.push_back(frame.seconds);
    }
    return all;
}

void LightningCastLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

const LightningCastLayer::Frame * LightningCastLayer::frameFor(long seconds) const {
    if (frames.empty()) {
        return nullptr;
    }
    if (seconds == 0) {
        return &frames.back();
    }
    const Frame * best = &frames.front();
    for (const auto& frame : frames) {
        if (frame.seconds <= seconds) {
            best = &frame;
        }
    }
    return best;
}

string LightningCastLayer::timeText() const {
    const auto * frame = frameFor(shown);
    return frame == nullptr ? string{} : "LightningCast " + QDateTime::fromSecsSinceEpoch(frame->seconds, QTimeZone::utc()).toString("HH:mm").toStdString() + "Z";
}

string LightningCastLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    const auto * frame = frameFor(shown);
    if (frame == nullptr) {
        return "LightningCast: reading the feed...";
    }
    return "LightningCast (next 60 min), scan " + QDateTime::fromSecsSinceEpoch(frame->seconds, QTimeZone::utc()).toString("HH:mm").toStdString() + "Z";
}

vector<MapLegendRow> LightningCastLayer::legend() const {
    if (frames.empty()) {
        return {};
    }
    MapLegendRow row;
    row.title = "Lightning, next 60 min (LightningCast):";
    const struct { int percent; QColor color; } levels[] = {{10, QColor{80, 201, 134}}, {30, QColor{255, 255, 81}}, {50, QColor{255, 192, 108}}, {70, QColor{255, 80, 80}}, {90, QColor{255, 80, 255}}};
    for (const auto& level : levels) {
        row.entries.push_back({MapLegendEntry::Line, level.color, QString::number(level.percent) + "%"});
    }
    return {row};
}

void LightningCastLayer::paint(QPainter& painter, MapHost& host) {
    const auto * frame = frameFor(shown);
    if (frame == nullptr) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    const double bottom = -250.0 + 1000.0 * view.map()->height() / std::max(1, view.map()->width());
    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const bool edge : {true, false}) {
        for (const int percent : {10, 30, 50, 70, 90}) {   // the higher chances on top
            for (const auto& contour : frame->contours) {
                if (contour.percent != percent) {
                    continue;
                }
                QPolygonF mapped;
                mapped.reserve(contour.points.size());
                bool near = false;
                for (const auto& point : contour.points) {
                    const QPointF at = t(point.y(), point.x());
                    mapped.append(at);
                    near = near || (at.x() > -520.0 && at.x() < 520.0 && at.y() > -270.0 && at.y() < bottom + 20.0);
                }
                if (!near) {
                    continue;
                }
                painter.setPen(edge ? QPen{QColor{0, 0, 0, 190}, 3.6 * perPixel, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin}
                                    : QPen{contour.color, 2.0 * perPixel, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin});
                painter.drawPolyline(mapped);
            }
        }
    }
    painter.restore();
}
