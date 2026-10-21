// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/CiraStorm.h"
#include "ui/CaptionedTile.h"
#include "ui/UiStandards.h"
#include <QDateTime>
#include <QImage>
#include "misc/ImageViewer.h"
#include "misc/TextViewerStatic.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include <cstdlib>
#include <algorithm>
#include "tropical/CiraLoopViewer.h"
#include "hurricane/SarWindsViewer.h"
#include "hurricane/AtcfFeedViewer.h"
#include "tropical/UtilityJma.h"

CiraStorm::CiraStorm(Window * parent, const string& stormId, const string& title)
    : Window{parent}
    , stormId{stormId}
    , title{title}
    , sw{this, box}
    , textHeader{this, title}
    , textNote{this, "CIRA / RAMMB experimental tropical cyclone products (Colorado State University, NOAA). Click a picture to enlarge it."}
    , textSar{this, ""}
    , textForecast{this, ""}
    , textHistory{this, ""}
    , textRapid{this, ""}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tropical - " + title);
    textHeader.setBold();
    textHeader.setBlue();
    textNote.setWordWrap(true);
    textForecast.setFixedWidth();
    textHistory.setFixedWidth();
    textRapid.setFixedWidth();
    box.addWidget(textHeader);
    box.addWidget(textNote);
    box.addLayout(rowButtons);
    textSar.setFixedWidth();
    textSar.setVisible(false);
    box.addWidget(textSar);
    flowImages.setEqualRowHeights(true);
    box.addLayout(flowImages);
    box.addWidget(textForecast);
    box.addWidget(textHistory);
    box.addWidget(textRapid);
    box.addStretch();
    auto page = std::make_shared<UtilityCira::StormPage>();
    auto error = std::make_shared<string>();
    const auto id = stormId;
    new FutureVoid{this,
        [page, error, id] { UtilityCira::stormPage(id, *page, *error); },
        [this, page, error] {
            if (closed) {
                return;
            }
            if (!error->empty()) {
                textNote.setText(*error);
                return;
            }
            fill(*page);
        }};
}

void CiraStorm::fill(const UtilityCira::StormPage& page) {
    // loops of the imagery that is updated every few minutes to hours
    for (const auto& product : UtilityCira::products()) {
        if (!product.loop || !page.imageUrl.count(product.key)) {
            continue;
        }
        buttons.emplace_back(this, None, "Loop: " + product.label);
        const auto key = product.key;
        buttons.back().connect([this, key] { new CiraLoopViewer{this, stormId, title, key}; });
        rowButtons.addWidget(buttons.back());
    }
    {   // the SAR winds of NOAA STAR (any basin: the storm's ATCF id is theirs too)
        buttons.emplace_back(this, None, "SAR winds");
        buttons.back().connect([this] {
            string upper = stormId;
            std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
            const auto year = upper.size() >= 8 ? std::atoi(upper.substr(upper.size() - 4).c_str()) : 0;
            const auto dash = title.rfind(' ');
            new SarWindsViewer{this, upper, dash == string::npos ? string{} : title.substr(dash + 1), year};
        });
        rowButtons.addWidget(buttons.back());
    }
    {   // the raw ATCF feed of the storm
        buttons.emplace_back(this, None, "ATCF feed");
        buttons.back().connect([this] { new AtcfFeedViewer{this, stormId, title}; });
        rowButtons.addWidget(buttons.back());
    }
    addJtwc();
    addJma();
    addSar();
    rowButtons.addStretch();
    for (const auto& product : UtilityCira::products()) {
        const auto found = page.imageUrl.find(product.key);
        if (found == page.imageUrl.end()) {
            continue;
        }
        images.emplace_back(this);
        auto& image = images.back();
        image.imageSize = UiStandards::tileImage;
        image.getView()->setToolTip(QString::fromStdString(product.label));
        const auto url = found->second;
        const auto label = product.label;
        image.connect([this, url, label] { new ImageViewer{this, url, label}; });
        flowImages.addWidgetReal(CaptionedTile::make(this, image.getView(), QString::fromStdString(label), QString::fromStdString(label),
                                                     UiStandards::tileImage, true, UiStandards::tileWidth));
        const auto index = images.size() - 1;
        new FutureBytes{this, url, [this, index] (const auto& bytes) {
            if (!closed && index < images.size()) {
                images[index].setBytes(bytes);
            }
        }};
    }
    if (images.empty()) {
        textNote.setText(string{"CIRA / RAMMB has no pictures for " + stormId + " yet."});
    }
    if (!page.forecastTrack.empty()) {
        textForecast.setText("Forecast track" + (page.forecastTime.empty() ? string{} : " (issued " + page.forecastTime + " UTC)") + "\n" +
                             page.forecastTrack);
    }
    if (!page.trackHistory.empty()) {
        textHistory.setText("Track history\n" + page.trackHistory);
    }
    if (!page.rapidIntensificationTableUrl.empty()) {
        new FutureText{this, page.rapidIntensificationTableUrl, [this] (const auto& text) {
            if (!closed && !text.empty()) {
                textRapid.setText("Rapid intensification (RI) probabilities\n" + text);
            }
        }};
    }
}

namespace {
    // CIRA's "wp262026" is JTWC's "wp2626": basin, storm number, two-digit year. Numbers from 90 up are invests (no warning).
    string jtwcCode(const string& id) {
        if (id.size() < 8) {
            return {};
        }
        const auto basin = id.substr(0, 2);
        if (basin != "wp" && basin != "io" && basin != "sh") {
            return {};
        }
        const int number = std::atoi(id.substr(2, 2).c_str());
        return number >= 90 ? string{} : basin + id.substr(2, 2) + id.substr(6, 2);
    }
    const string jtwcBase{"https://www.metoc.navy.mil/jtwc/products/"};
}

// The Joint Typhoon Warning Center (US Navy / Air Force) covers the western Pacific, Indian Ocean and Southern Hemisphere: its warning
// graphic and text, its forecast reasoning, and for an invest its outlook of the basin.
void CiraStorm::addJtwc() {
    const auto basin = stormId.substr(0, 2);
    if (basin != "wp" && basin != "io" && basin != "sh") {
        return;
    }
    const auto code = jtwcCode(stormId);
    if (!code.empty()) {
        buttons.emplace_back(this, None, "JTWC warning");
        buttons.back().connect([this, code] { openText(jtwcBase + code + "web.txt", "JTWC warning " + code); });
        rowButtons.addWidget(buttons.back());
        buttons.emplace_back(this, None, "JTWC forecast reasoning");
        buttons.back().connect([this, code] { openText(jtwcBase + code + "prog.txt", "JTWC reasoning " + code); });
        rowButtons.addWidget(buttons.back());
        // JTWC's warning graphic, added once it has loaded (so a storm without one shows nothing)
        const auto url = jtwcBase + code + ".gif";
        new FutureBytes{this, url, [this, url] (const auto& bytes) {
            if (closed || QImage::fromData(bytes).isNull()) {
                return;
            }
            images.emplace_back(this);
            auto& image = images.back();
            image.imageSize = UiStandards::tileImage;
            image.getView()->setToolTip("JTWC warning graphic");
            image.connect([this, url] { new ImageViewer{this, url, "JTWC warning graphic"}; });
            image.setBytes(bytes);
            flowImages.addWidgetReal(CaptionedTile::make(this, image.getView(), "JTWC warning graphic", "JTWC warning graphic", UiStandards::tileImage, true, UiStandards::tileWidth));
        }};
    } else if (basin == "wp" || basin == "io") {
        const auto outlook = basin == "wp" ? "abpwweb.txt" : "abioweb.txt";
        buttons.emplace_back(this, None, basin == "wp" ? "JTWC western Pacific outlook" : "JTWC Indian Ocean outlook");
        buttons.back().connect([this, outlook] { openText(jtwcBase + outlook, "JTWC outlook"); });
        rowButtons.addWidget(buttons.back());
    }
}

void CiraStorm::openText(const string& url, const string& heading) {
    new FutureText{this, url, [this, heading] (const string& text) {
        if (closed) {
            return;
        }
        if (text.empty() || text.find("<Error>") != string::npos || text.find("<html") != string::npos) {
            new TextViewerStatic{this, "That product is not available from JTWC right now.", heading, 500, 150};
            return;
        }
        new TextViewerStatic{this, text, heading, 800, 700};
    }};
}

void CiraStorm::addJma() {
    if (stormId.substr(0, 2) != "wp") {
        return;
    }
    const auto name = UtilityJma::nameFromTitle(title);
    if (name.empty()) {
        return;
    }
    buttons.emplace_back(this, None, "JMA analysis and forecast (English)");
    buttons.back().connect([this, name] {
        auto text = std::make_shared<string>();
        auto error = std::make_shared<string>();
        new FutureVoid{this,
            [text, error, name] { UtilityJma::advisoryFor(name, *text, *error); },
            [this, text, error, name] {
                if (closed) {
                    return;
                }
                new TextViewerStatic{this, text->empty() ? "JMA: " + *error : *text, "JMA " + name, 820, 760};
            }};
    });
    rowButtons.addWidget(buttons.back());
}

// The newest SAR wind scene of this storm (any basin), shown on the page: the wind picture, when and by which satellite, and what was measured in it; a click opens all of them.
void CiraStorm::addSar() {
    string upper = stormId;
    std::transform(upper.begin(), upper.end(), upper.begin(), [] (unsigned char c) { return static_cast<char>(std::toupper(c)); });
    const auto year = upper.size() >= 8 ? std::atoi(upper.substr(upper.size() - 4).c_str()) : 0;
    const auto dash = title.rfind(' ');
    const string name = dash == string::npos ? string{} : title.substr(dash + 1);
    auto scenes = std::make_shared<vector<UtilitySarWinds::Scene>>();
    auto schedule = std::make_shared<UtilitySarWinds::Schedule>();
    auto problem = std::make_shared<string>();
    new FutureVoid{this,
        [scenes, schedule, problem, upper, name, year] {
            UtilitySarWinds::fetchSchedule(*schedule);
            *problem = UtilitySarWinds::storm(upper, name, year, *scenes);
        },
        [this, scenes, schedule, upper, name, year] {
            if (closed) {
                return;
            }
            // the next pass STAR lists for this storm (UTC)
            string next;
            const auto now = QDateTime::currentDateTimeUtc();
            const UtilitySarWinds::Pass * soonest = nullptr;
            QDateTime soonestTime;
            for (const auto& pass : schedule->passes) {
                if (pass.id != upper) {
                    continue;
                }
                auto when = QDateTime::fromString(QString::fromStdString(pass.time), "yyyy-MM-dd HH:mm");
                when.setTimeSpec(Qt::UTC);
                if (when.isValid() && when >= now.addSecs(-1800) && (soonest == nullptr || when < soonestTime)) {
                    soonest = &pass;
                    soonestTime = when;
                }
            }
            if (soonest != nullptr) {
                next = "Next SAR pass expected: " + soonest->time + "Z  " + soonest->sensor + "   (STAR's schedule, " + schedule->updated + ")";
            }
            if (scenes->empty()) {   // no scene of this storm yet: say when the next pass is, if STAR lists one
                textSar.setText(next.empty() ? string{"No SAR winds of this storm yet, and STAR lists no pass of it right now."} : "No SAR winds of this storm yet.\n" + next);
                textSar.setVisible(true);
                return;
            }
            const auto& scene = scenes->front();
            string text = "Latest SAR winds (NOAA STAR): " + scene.time + "  " + scene.sensor + "\n";
            for (const auto& line : scene.measurements) {
                text += "  " + line + "\n";
            }
            textSar.setText(text + (next.empty() ? string{} : next + "\n") + "  (the SAR winds tile, or the SAR winds button, opens every scene and the schedule)");
            textSar.setVisible(true);
            if (!scene.wind.empty()) {   // the scene's wind picture as a tile like the others (a click opens the SAR winds window)
                const auto caption = QString::fromStdString("SAR winds " + scene.sensor.substr(0, scene.sensor.find(" - ")));
                images.emplace_back(this);
                auto& image = images.back();
                image.imageSize = UiStandards::tileImage;
                image.getView()->setToolTip("The newest SAR wind scene (NOAA STAR): click for every scene");
                image.connect([this, upper, name, year] { new SarWindsViewer{this, upper, name, year}; });
                flowImages.addWidgetReal(CaptionedTile::make(this, image.getView(), caption, caption, UiStandards::tileImage, true, UiStandards::tileWidth));
                const auto at = images.size() - 1;
                new FutureBytes{this, scene.wind, [this, at] (const auto& bytes) {
                    if (!closed && at < images.size() && !bytes.isEmpty()) {
                        images[at].setBytes(bytes);
                    }
                }};
            }
        }};
}
