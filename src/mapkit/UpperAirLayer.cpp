// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/UpperAirLayer.h"
#include <algorithm>
#include <cmath>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimeZone>
#include "models/SoundingViewer.h"
#include "objects/FutureVoid.h"
#include "radar/MapView.h"
#include "sounding/SoundingProfile.h"
#include "util/SoundingSites.h"
#include "util/UtilityIO.h"

namespace {
    constexpr long half = 12 * 3600;

    // the balloon sounding of a station and launch from the IEM archive: false with the reason
    bool fetch(const string& code, long launch, SoundingProfile& out, string& why) {   // (UpperAirLayer::fetchProfile)
        const auto when = QDateTime::fromSecsSinceEpoch(launch, QTimeZone::utc());
        for (const string& id : {"K" + code, code}) {   // (the United States' stations have a K in front; others stand as they are)
            const auto text = UtilityIO::getHtml("https://mesonet.agron.iastate.edu/json/raob.py?ts=" + when.toString("yyyyMMddHHmm").toStdString() + "&station=" + id);
            const auto doc = QJsonDocument::fromJson(QByteArray::fromStdString(text));
            const auto profiles = doc.object().value("profiles").toArray();
            if (profiles.isEmpty()) {
                continue;
            }
            SoundingProfile p;
            for (const auto& row : profiles.first().toObject().value("profile").toArray()) {
                const auto o = row.toObject();
                const auto number = [&o] (const char * key) { return o.value(key).isNull() || !o.contains(key) ? -9999.0 : o.value(key).toDouble(-9999.0); };
                const double pres = number("pres");
                if (pres < 0 || number("tmpc") < -9000) {
                    continue;   // the levels of wind alone are left out (the viewer wants the temperature)
                }
                p.pres.push_back(pres);
                p.hght.push_back(number("hght"));
                p.tmpc.push_back(number("tmpc"));
                p.dwpc.push_back(number("dwpc"));
                p.wdir.push_back(number("drct"));
                p.wspd.push_back(number("sknt"));
            }
            if (p.size() < 5) {
                continue;
            }
            p.station = code;
            p.validTime = when.toString("yyMMdd/HHmm").toStdString();
            if (SoundingSites::sites != nullptr && SoundingSites::sites->byCode.count(code) > 0) {
                p.latitude = SoundingSites::sites->byCode[code]->latLon.lat();
            }
            p.finalize();
            out = std::move(p);
            return true;
        }
        why = "no sounding for " + code + " at " + when.toString("yyyy-MM-dd HH").toStdString() + "Z in the archive (the station may not have launched)";
        return false;
    }
}

bool UpperAirLayer::fetchProfile(const string& code, long launch, SoundingProfile& out, string& why) {
    return fetch(code, launch, out, why);
}

// the launch the bar is at: the one at or before the time, or the newest that is likely in (three hours after the launch time)
long UpperAirLayer::launch() const {
    const long now = shown != 0 ? shown : (asOf.isValid() ? asOf.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch() - 3 * 3600);
    return now / half * half;
}

vector<long> UpperAirLayer::times() const {
    vector<long> out;
    const long middle = (asOf.isValid() ? asOf.toSecsSinceEpoch() : QDateTime::currentSecsSinceEpoch() - 3 * 3600) / half * half;
    const int after = asOf.isValid() ? 8 : 0;
    for (int i = -8; i <= after; i++) {
        out.push_back(middle + i * half);
    }
    return out;
}

void UpperAirLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

void UpperAirLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    shown = 0;
    host.timesChanged();
    host.redraw();
}

string UpperAirLayer::timeText() const {
    return QDateTime::fromSecsSinceEpoch(launch(), QTimeZone::utc()).toString("ddd yyyy-MM-dd HH").toStdString() + "Z balloon launches";
}

string UpperAirLayer::summary() const {
    return timeText();
}

void UpperAirLayer::paint(QPainter& painter, MapHost& host) {
    if (SoundingSites::sites == nullptr) {
        return;
    }
    auto& view = host.view();
    const auto t = view.transform();
    const double perPixel = view.unitsPerPixel();
    QFont font{painter.font()};
    font.setPixelSize(std::max(6, static_cast<int>(std::lround(10.0 * perPixel))));
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics{font};
    for (const auto& code : SoundingSites::sites->codeList) {
        const auto& site = *SoundingSites::sites->byCode[code];
        const auto pixels = view.toPixels(site.latLon.lat(), site.latLon.lon());
        if (pixels.x() < 0 || pixels.y() < 0 || pixels.x() > view.pixelWidth() || pixels.y() > view.pixelHeight()) {
            continue;
        }
        const QPointF at = t(site.latLon.lat(), site.latLon.lon());
        const double r = 4.5 * perPixel;
        painter.setPen(QPen{QColor{255, 255, 255}, 1.4 * perPixel});
        painter.setBrush(QColor{60, 140, 70});
        painter.drawEllipse(at, r, r);
        const QString text = QString::fromStdString(code);
        const QRectF box{at.x() + r + 1.5 * perPixel, at.y() - metrics.height() / 2.0, metrics.horizontalAdvance(text) + 4.0 * perPixel, metrics.height()};
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor{20, 20, 28, 170});
        painter.drawRoundedRect(box, 2.0 * perPixel, 2.0 * perPixel);
        painter.setPen(QColor{240, 240, 240});
        painter.drawText(box.adjusted(2.0 * perPixel, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, text);
    }
}

MapHit UpperAirLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 12.0;
    hit.priority = 25;
    if (SoundingSites::sites == nullptr) {
        return hit;
    }
    const long at = launch();
    for (const auto& code : SoundingSites::sites->codeList) {
        const auto& site = *SoundingSites::sites->byCode[code];
        const auto p = host.view().toPixels(site.latLon.lat(), site.latLon.lon());
        const double distance = std::hypot(p.x() - pixels.x(), p.y() - pixels.y());
        if (distance < hit.distance) {
            hit.distance = distance;
            hit.text = QString::fromStdString(code + "  " + site.fullName + "\n" + timeText() + "\nClick for the sounding");
            const string name = code, title = "Observed sounding  " + code + " " + site.fullName + "  " + QDateTime::fromSecsSinceEpoch(at, QTimeZone::utc()).toString("yyyy-MM-dd HH").toStdString() + "Z";
            hit.open = [name, title, at] (Window * parent) {
                auto profile = std::make_shared<SoundingProfile>();
                auto why = std::make_shared<string>();
                auto ok = std::make_shared<bool>(false);
                parent->setTitle("Master map - reading the sounding of " + name + " ...");
                new FutureVoid{parent, [=] { *ok = fetch(name, at, *profile, *why); }, [=] {
                    if (!*ok) {
                        parent->setTitle("Master map - " + *why);
                        return;
                    }
                    parent->setTitle("Master map");
                    new SoundingViewer{parent, *profile, title};
                }};
            };
        }
    }
    return hit;
}
