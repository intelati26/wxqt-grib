// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/CfsOutlookLayer.h"
#include <algorithm>
#include <cmath>
#include <future>
#include <QComboBox>
#include <QLabel>
#include <QPainter>
#include <QSlider>
#include <QTimeZone>
#include <QVBoxLayout>
#include "mapkit/PlateCarreeImage.h"
#include "radar/MapLegend.h"
#include "radar/MapView.h"
#include "util/Utility.h"

namespace {
    struct Stop {
        double value;
        QColor color;
    };
    // the colours of a field: value stops, blended between
    const std::vector<Stop>& ramp(const string& key) {
        static const std::vector<Stop> temperature{{-40, QColor{90, 40, 150}}, {-25, QColor{50, 90, 200}}, {-10, QColor{80, 170, 230}}, {0, QColor{200, 235, 245}}, {10, QColor{170, 215, 120}},
                                                   {20, QColor{250, 230, 90}}, {30, QColor{240, 140, 50}}, {40, QColor{200, 40, 40}}};
        static const std::vector<Stop> rain{{0, QColor{250, 250, 245}}, {1, QColor{225, 215, 170}}, {2, QColor{170, 215, 150}}, {4, QColor{90, 190, 120}}, {7, QColor{50, 150, 200}}, {11, QColor{40, 80, 190}}, {16, QColor{110, 50, 160}}};
        static const std::vector<Stop> height{{5000, QColor{110, 50, 160}}, {5300, QColor{50, 90, 200}}, {5550, QColor{90, 190, 220}}, {5700, QColor{170, 215, 120}}, {5800, QColor{250, 230, 90}}, {5900, QColor{240, 140, 50}}, {6000, QColor{200, 40, 40}}};
        static const std::vector<Stop> pressure{{980, QColor{110, 50, 160}}, {995, QColor{50, 90, 200}}, {1005, QColor{120, 190, 230}}, {1013, QColor{235, 235, 225}}, {1020, QColor{250, 210, 90}}, {1030, QColor{240, 130, 50}}, {1040, QColor{200, 40, 40}}};
        static const std::vector<Stop> water{{0, QColor{250, 250, 245}}, {10, QColor{225, 215, 170}}, {20, QColor{170, 215, 150}}, {35, QColor{90, 190, 120}}, {50, QColor{50, 150, 200}}, {65, QColor{110, 50, 160}}};
        if (key == "precip") return rain;
        if (key == "z500") return height;
        if (key == "mslp") return pressure;
        if (key == "pwat") return water;
        return temperature;
    }

    QRgb colorOf(const std::vector<Stop>& stops, double v) {
        if (v <= stops.front().value) {
            return stops.front().color.rgb();
        }
        for (size_t i = 1; i < stops.size(); i++) {
            if (v <= stops[i].value) {
                const double f = (v - stops[i - 1].value) / (stops[i].value - stops[i - 1].value);
                const auto a = stops[i - 1].color, b = stops[i].color;
                return qRgb(static_cast<int>(a.red() + (b.red() - a.red()) * f), static_cast<int>(a.green() + (b.green() - a.green()) * f), static_cast<int>(a.blue() + (b.blue() - a.blue()) * f));
            }
        }
        return stops.back().color.rgb();
    }
}

CfsOutlookLayer::CfsOutlookLayer() {
    variableKey = Utility::readPref("MM_CFS_VARIABLE", "t2m");
    if (UtilityCfs::find(variableKey) == nullptr) {
        variableKey = "t2m";
    }
    memberChoice = std::clamp(Utility::readPrefInt("MM_CFS_MEMBER", 0), 0, 4);
}

long CfsOutlookLayer::middleOf(const string& yyyymm) const {
    return QDateTime{QDate{std::stoi(yyyymm.substr(0, 4)), std::stoi(yyyymm.substr(4, 2)), 15}, QTime{0, 0}, QTimeZone::utc()}.toSecsSinceEpoch();
}

vector<long> CfsOutlookLayer::times() const {
    vector<long> out;
    if (!haveRun) {
        return out;
    }
    for (const auto& month : UtilityCfs::months(run)) {
        if (frames.count(month) > 0) {
            out.push_back(middleOf(month));
        }
    }
    return out;
}

string CfsOutlookLayer::monthShown() const {
    const auto t = times();
    if (t.empty()) {
        return {};
    }
    long target = shown;
    if (target == 0 || target < t.front()) {
        target = shown == 0 ? t.front() : t.front();
    }
    auto it = std::upper_bound(t.begin(), t.end(), target);
    it = it == t.begin() ? it : std::prev(it);
    return QDateTime::fromSecsSinceEpoch(*it, QTimeZone::utc()).toString("yyyyMM").toStdString();
}

string CfsOutlookLayer::timeText() const {
    const auto month = monthShown();
    if (month.empty() || !haveRun) {
        return {};
    }
    return QDate{std::stoi(month.substr(0, 4)), std::stoi(month.substr(4, 2)), 1}.toString("MMMM yyyy").toStdString() + " mean  CFSv2 run " + UtilityCfs::runText(run);
}

string CfsOutlookLayer::summary() const {
    if (!error.empty()) {
        return error;
    }
    if (loading && frames.empty()) {
        return "reading the CFSv2 run...";
    }
    const auto * variable = UtilityCfs::find(variableKey);
    if (!haveRun || variable == nullptr) {
        return {};
    }
    return "CFSv2 " + UtilityCfs::runText(run) + ", " + variable->label + ", " + (memberChoice == 0 ? string{"mean of 4 members"} : "member " + std::to_string(memberChoice)) + (loading ? "  (more months coming)" : "");
}

void CfsOutlookLayer::showTime(long seconds, MapHost& host) {
    shown = seconds;
    host.redraw();
}

void CfsOutlookLayer::setAsOf(const QDateTime& utc, MapHost& host) {
    asOf = utc.toUTC();
    frames.clear();
    haveRun = false;
    error.clear();
    shown = 0;
    refresh(host);
}

void CfsOutlookLayer::optionChanged(MapHost& host) {
    if (changeRequested) {
        changeRequested = false;
        frames.clear();
        shown = shown;
        refresh(host);
    }
}

void CfsOutlookLayer::refresh(MapHost& host) {
    if (loading) {
        return;
    }
    if (!frames.empty() && asOf.isValid()) {
        return;
    }
    loading = true;
    struct Found {
        UtilityCfs::Run run;
        bool ok{false};
        string error;
    };
    auto found = std::make_shared<Found>();
    const QDateTime when = asOf;
    host.background(
        [found, when] {
            if (when.isValid()) {   // the run of that day at 00 UTC (the bucket begins 2018-10-31)
                if (when < QDateTime{QDate{2018, 10, 31}, QTime{0, 0}, QTimeZone::utc()}) {
                    found->error = "the open CFSv2 bucket begins on 31 October 2018";
                    return;
                }
                found->run = {when.toString("yyyyMMdd").toStdString(), "00"};
                found->ok = true;
                return;
            }
            found->ok = UtilityCfs::latestRun(found->run);
            if (!found->ok) {
                found->error = "no CFSv2 run found";
            }
        },
        [this, &host, found, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            if (!found->ok) {
                error = found->error;
                host.timesChanged();
                host.redraw();
                return;
            }
            error.clear();
            if (haveRun && run.date == found->run.date && run.cycle == found->run.cycle && !frames.empty()) {
                host.timesChanged();
                return;
            }
            run = found->run;
            haveRun = true;
            load(host);
        });
}

// every month of the run, the second (the first whole month) first so that the map has something soon; the members are read side by side
void CfsOutlookLayer::load(MapHost& host) {
    if (!haveRun || loading) {
        return;
    }
    const auto months = UtilityCfs::months(run);
    std::vector<string> wanted;
    for (const auto& m : months) {
        if (frames.count(m) == 0) {
            wanted.push_back(m);
        }
    }
    if (wanted.empty()) {
        return;
    }
    std::rotate(wanted.begin(), wanted.size() > 1 ? wanted.begin() + 1 : wanted.begin(), wanted.end());   // the month after the run's own first
    loading = true;
    struct Loaded {
        std::vector<std::pair<string, GfsGrid::Grid>> grids;
        string error;
    };
    auto loaded = std::make_shared<Loaded>();
    const auto chosen = *UtilityCfs::find(variableKey);
    const auto theRun = run;
    const int memberWanted = memberChoice;
    host.background(
        [loaded, wanted, chosen, theRun, memberWanted] {
            for (const auto& month : wanted) {
                std::vector<std::future<std::pair<bool, GfsGrid::Grid>>> jobs;
                for (int member = (memberWanted == 0 ? 1 : memberWanted); member <= (memberWanted == 0 ? 4 : memberWanted); member++) {
                    jobs.push_back(std::async(std::launch::async, [chosen, theRun, member, month] {
                        GfsGrid::Grid g;
                        string message;
                        const bool ok = UtilityCfs::monthly(theRun, member, chosen, month, g, message);
                        return std::make_pair(ok, g);
                    }));
                }
                GfsGrid::Grid mean;
                int count = 0;
                for (auto& job : jobs) {
                    auto [ok, g] = job.get();
                    if (!ok) {
                        continue;
                    }
                    if (mean.empty()) {
                        mean = g;
                    } else if (g.values.size() == mean.values.size()) {
                        for (size_t i = 0; i < g.values.size(); i++) {
                            mean.values[i] += g.values[i];
                        }
                    }
                    count++;
                }
                if (count == 0) {
                    if (loaded->grids.empty()) {
                        loaded->error = "the CFSv2 run has no " + chosen.label + " for " + month;
                    }
                    continue;
                }
                for (auto& v : mean.values) {
                    v /= static_cast<float>(count);
                }
                loaded->grids.emplace_back(month, std::move(mean));
            }
        },
        [this, &host, loaded, alive = alive] {
            if (!*alive) {
                return;
            }
            loading = false;
            for (auto& [month, grid] : loaded->grids) {
                Frame f;
                f.image = colored(grid);
                f.grid = std::move(grid);
                frames[month] = std::move(f);
            }
            if (frames.empty()) {
                error = loaded->error.empty() ? "no CFSv2 data could be read" : loaded->error;
            } else {
                error.clear();
            }
            host.timesChanged();
            host.redraw();
        });
}

// the rows within 85 degrees of the equator (the map is Mercator: the poles are off it)
QImage CfsOutlookLayer::colored(const GfsGrid::Grid& grid) const {
    int first = 0, last = grid.rows - 1;
    while (first < last && grid.lat0 - first * grid.step > 85.0) {
        first++;
    }
    while (last > first && grid.lat0 - last * grid.step < -85.0) {
        last--;
    }
    // the 1 degree cells smoothed (bicubic) onto a grid three times as fine, so that the colours run on and no cell shows
    const int up = 3;
    QImage image{grid.columns * up, (last - first + 1) * up, QImage::Format_ARGB32_Premultiplied};
    const auto& stops = ramp(variableKey);
    for (int y = 0; y < image.height(); y++) {
        auto * out = reinterpret_cast<QRgb *>(image.scanLine(y));
        const double lat = grid.lat0 - first * grid.step + grid.step / 2.0 - (y + 0.5) * grid.step / up;
        for (int x = 0; x < image.width(); x++) {
            const double lon = grid.lon0 - grid.step / 2.0 + (x + 0.5) * grid.step / up;
            const float v = grid.sampleCubic(lon, lat);
            out[x] = std::isnan(v) ? qRgba(0, 0, 0, 0) : (colorOf(stops, v) | 0xff000000u);
        }
    }
    return image;
}

void CfsOutlookLayer::paint(QPainter& painter, MapHost& host) {
    const auto month = monthShown();
    const auto found = frames.find(month);
    if (found == frames.end()) {
        return;
    }
    const auto& g = found->second.grid;
    int first = 0;
    while (first < g.rows - 1 && g.lat0 - first * g.step > 85.0) {
        first++;
    }
    const double north = g.lat0 - first * g.step + g.step / 2.0, south = north - found->second.image.height() * g.step / 3.0;
    PlateCarreeImage::paint(painter, host, found->second.image, g.lon0 - g.step / 2.0, south, g.lon0 + g.columns * g.step - g.step / 2.0, north, opacity / 100.0, true);
}

MapHit CfsOutlookLayer::pick(const QPointF& pixels, MapHost& host) const {
    MapHit hit;
    hit.reach = 1.0;
    hit.priority = -1;
    const auto found = frames.find(monthShown());
    const auto * variable = UtilityCfs::find(variableKey);
    if (found == frames.end() || variable == nullptr) {
        return hit;
    }
    const auto [lat, lon] = host.view().toLatLon(pixels);
    const float v = found->second.grid.sample(lon > 180.0 ? lon - 360.0 : lon, lat);
    if (std::isnan(v)) {
        return hit;
    }
    hit.distance = 0.0;
    hit.text = QString::fromStdString("CFSv2 " + variable->label + ", " + timeText()) + "\n" + QString::number(v, 'f', variable->key == "precip" ? 1 : 1) + " " + QString::fromStdString(variable->units);
    return hit;
}

vector<MapLegendRow> CfsOutlookLayer::legend() const {
    const auto * variable = UtilityCfs::find(variableKey);
    if (variable == nullptr || frames.empty()) {
        return {};
    }
    MapLegendRow row;
    row.title = QString::fromStdString(variable->label + " (" + variable->units + "):");
    for (const auto& stop : ramp(variableKey)) {
        row.entries.push_back({MapLegendEntry::Square, stop.color, QString::number(stop.value, 'f', 0)});
    }
    return {row};
}

QWidget * CfsOutlookLayer::options(QWidget * parent, const std::function<void()>& changed) {
    auto * widget = new QWidget{parent};
    auto * layout = new QVBoxLayout{widget};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(new QLabel{"Field:", widget});
    auto * field = new QComboBox{widget};
    int index = 0, at = 0;
    for (const auto& v : UtilityCfs::variables()) {
        field->addItem(QString::fromStdString(v.label), QString::fromStdString(v.key));
        if (v.key == variableKey) {
            index = at;
        }
        at++;
    }
    field->setCurrentIndex(index);
    layout->addWidget(field);
    layout->addWidget(new QLabel{"Members:", widget});
    auto * members = new QComboBox{widget};
    members->addItems({"Mean of the 4 members", "Member 1", "Member 2", "Member 3", "Member 4"});
    members->setCurrentIndex(memberChoice);
    layout->addWidget(members);
    layout->addWidget(new QLabel{"Opacity:", widget});
    auto * slider = new QSlider{Qt::Horizontal, widget};
    slider->setRange(20, 100);
    slider->setValue(opacity);
    layout->addWidget(slider);
    QObject::connect(field, &QComboBox::currentIndexChanged, [this, field, changed] (int) {
        variableKey = field->currentData().toString().toStdString();
        Utility::writePref("MM_CFS_VARIABLE", variableKey);
        changeRequested = true;
        changed();
    });
    QObject::connect(members, &QComboBox::currentIndexChanged, [this, changed] (int value) {
        memberChoice = value;
        Utility::writePrefInt("MM_CFS_MEMBER", value);
        changeRequested = true;
        changed();
    });
    QObject::connect(slider, &QSlider::valueChanged, [this, changed] (int value) { opacity = value; changed(); });
    return widget;
}
