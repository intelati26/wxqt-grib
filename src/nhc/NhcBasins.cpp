// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "nhc/NhcBasins.h"
#include <algorithm>
#include <cmath>
#include <QHBoxLayout>
#include <QPointer>
#include <QPushButton>
#include "hurricane/HurricaneViewer.h"
#include "objects/FutureVoid.h"
#include "objects/URL.h"
#include "tropical/PhaseSpace.h"
#include "tropical/PhaseViewer.h"
#include "ui/UiStandards.h"

namespace {
    bool inside(const UtilityNhcGis::Ring& ring, double lat, double lon) {
        bool in = false;
        for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
            const double xi = ring[i].first, yi = ring[i].second, xj = ring[j].first, yj = ring[j].second;
            if (((yi > lat) != (yj > lat)) && (lon < (xj - xi) * (lat - yi) / (yj - yi) + xi)) {
                in = !in;
            }
        }
        return in;
    }

    bool inArea(const UtilityNhcGis::OutlookArea& area, double lat, double lon) {
        return std::any_of(area.rings.begin(), area.rings.end(), [lat, lon] (const auto& ring) { return ring.size() >= 3 && inside(ring, lat, lon); });
    }

    // the outlook's "Pacific" areas belong to the Central Pacific west of 140 W
    string basinOfArea(const UtilityNhcGis::OutlookArea& area) {
        return area.basin == "Atlantic" ? "al" : (area.centerLon < -140.0 || area.centerLon > 0.0 ? "cp" : "ep");
    }

    const char * basinName(const string& basin) {
        return basin == "al" ? "Atlantic" : basin == "ep" ? "East Pacific" : "Central Pacific";
    }

    QString where(double lat, double lon) {
        lon = lon > 180.0 ? lon - 360.0 : lon;   // (the outlook's shapes run on to 190 E)
        return QString::number(std::fabs(lat), 'f', 1) + (lat >= 0 ? "N " : "S ") + QString::number(std::fabs(lon), 'f', 1) + (lon >= 0 ? "E" : "W");
    }

    QLabel * text(const QString& html, QWidget * parent) {
        auto * label = new QLabel{html, parent};
        label->setWordWrap(true);
        label->setTextFormat(Qt::RichText);
        return label;
    }
}

NhcBasins::NhcBasins(Window * owner, QWidget * parent)
    : QWidget{parent}
    , owner{owner}
    , layout{new QVBoxLayout{this}}
    , outlook{std::make_shared<HurricaneData::OutlookData>()}
    , systems{std::make_shared<vector<System>>()}
{
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(text("Reading the outlook and the storm lists...", this));
    auto outlookData = outlook;
    auto list = systems;
    QPointer<NhcBasins> guard{this};
    new FutureVoid{owner, [outlookData, list] {
        HurricaneData::loadOutlook(*outlookData);
        for (const char * basin : {"al", "ep", "cp"}) {
            vector<HurricaneData::StormEntry> entries;
            string error;
            HurricaneData::loadStormList(entries, error, basin);
            for (const auto& entry : entries) {
                if (!entry.active) {
                    continue;
                }
                System system;
                system.entry = entry;
                system.basin = basin;
                system.lat = entry.lat;
                system.lon = entry.lon;
                system.placed = entry.lat != 0.0 || entry.lon != 0.0;
                if (!system.placed) {   // an invest: the newest fix of its best track file
                    const auto text = URL::getBytes("https://ftp.nhc.noaa.gov/atcf/btk/b" + entry.id + ".dat").toStdString();
                    const auto track = PhaseSpace::parseBest(text);
                    if (!track.empty()) {
                        system.lat = track.back().lat;
                        system.lon = track.back().lon;
                        system.placed = true;
                    }
                }
                list->push_back(system);
            }
        }
    }, [guard] {
        if (!guard.isNull()) {
            guard->fill();
        }
    }};
}

void NhcBasins::fill() {
    while (auto * item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    std::vector<bool> linked(systems->size(), false);
    for (const string basin : {"al", "ep", "cp"}) {
        auto * heading = text(QString{"<b style='font-size:14px'>%1</b>"}.arg(basinName(basin)), this);
        layout->addWidget(heading);
        const auto button = [this] (const QString& label, const QString& tip, std::function<void()> action) {
            auto * b = UiStandards::button(this, label, tip);
            QObject::connect(b, &QPushButton::clicked, action);
            return b;
        };
        // a system's row: its name, class and place, then the buttons
        const auto systemRow = [&] (const System& system, const QString& lead) {
            auto * row = new QWidget{this};
            auto * rowLayout = new QHBoxLayout{row};
            rowLayout->setContentsMargins(24, 0, 0, 0);
            const auto& e = system.entry;
            const bool invest = e.id.size() >= 4 && e.id.substr(2, 2)[0] == '9';
            QString line = lead + "<b>" + QString::fromStdString(HurricaneData::idLabel(e.id)) + (e.name.empty() ? QString{} : " " + QString::fromStdString(e.name)) + "</b>  " + (invest ? QString{"invest"} : QString::fromStdString(e.classification));
            if (e.wind >= 0) {
                line += QString{", %1 kt"}.arg(e.wind);
            }
            if (system.placed) {
                line += "  at " + where(system.lat, system.lon);
            }
            rowLayout->addWidget(text(line, row), 1);
            const string basinCopy = system.basin, id = e.id;
            rowLayout->addWidget(button("Track map...", "The track, model guidance and recon of this system", [this, basinCopy, id] { new HurricaneViewer{owner, basinCopy, id}; }));
            const double lat = system.lat, lon = system.lon;
            const string title = HurricaneData::idLabel(id) + (e.name.empty() ? "" : " " + e.name);
            rowLayout->addWidget(button("Phase space...", "Hart's cyclone phase space of this system in a model", [this, title, lat, lon, id] { PhaseViewer::pick(owner, title, lat, lon, "GFS", id); }));
            layout->addWidget(row);
        };
        bool any = false;
        for (const auto& area : outlook->areas) {
            if (basinOfArea(area) != basin) {
                continue;
            }
            any = true;
            const auto& risk = area.risk7.empty() ? area.risk2 : area.risk7;
            const QString color = risk == "High" ? "#d33" : risk == "Medium" ? "#e80" : "#cb0";
            layout->addWidget(text("<span style='color:" + color + "'>&#9632;</span> <b>Outlook area " + QString::fromStdString(area.area) + "</b> near " + where(area.centerLat, area.centerLon) + ":  " +
                                   QString::number(area.prob2) + "% in 2 days (" + QString::fromStdString(area.risk2) + "),  " + QString::number(area.prob7) + "% in 7 days (" + QString::fromStdString(area.risk7) + ")", this));
            bool has = false;
            for (size_t i = 0; i < systems->size(); i++) {
                const auto& system = (*systems)[i];
                if (system.placed && inArea(area, system.lat, system.lon)) {
                    systemRow(system, "inside the area:  ");
                    linked[i] = true;
                    has = true;
                }
            }
            if (!has) {
                auto * none = text("no storm or invest designated in it yet", this);
                UiStandards::muted(none);
                none->setContentsMargins(24, 0, 0, 0);
                layout->addWidget(none);
            }
        }
        if (!any) {
            auto * none = text("The outlook has no area of possible development in this basin.", this);
            UiStandards::muted(none);
            layout->addWidget(none);
        }
        bool header = false;
        for (size_t i = 0; i < systems->size(); i++) {
            const auto& system = (*systems)[i];
            if (system.basin != basin || linked[i]) {
                continue;
            }
            if (!header) {
                layout->addWidget(text("<b>Storms and invests outside the outlook areas</b>", this));
                header = true;
            }
            systemRow(system, "");
        }
    }
    layout->addStretch();
}
