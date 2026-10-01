// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spc/SvrComparison.h"
#include <algorithm>
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "spc/UtilitySpcSwo.h"
#include "util/To.h"

namespace {
    const string csuBase{"https://schumacher.atmos.colostate.edu/hilla/csu_mlp/latest/"};
    const string cipsBase{"https://www.eas.slu.edu/CIPS/SVRprob/current/"};
    const vector<string> hazardCodes{"HAIL", "TORN", "WIND"};

    vector<string> dayLabels() {
        vector<string> days;
        for (int day = 1; day <= 8; day += 1) {
            days.push_back("Day " + To::string(day));
        }
        return days;
    }
}

SvrComparison::SvrComparison(Window * parent)
    : Window{parent}
    , comboDay{this, dayLabels()}
    , comboHazard{this, {"CIPS hazard: hail", "CIPS hazard: tornado", "CIPS hazard: wind"}}
    , textNote{this, ""}
{
    setTitle("Severe weather outlook comparison");
    comboDay.connect([this] { reload(); });
    comboHazard.connect([this] { reload(); });
    comboHazard.getView()->setToolTip("CIPS has one picture per hazard on days 1 and 2; CSU-MLP and SPC pictures show all hazards (or the categorical risk)");
    rowTop.addWidget(comboDay);
    rowTop.addWidget(comboHazard);
    rowTop.addWidget(textNote);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addLayout(rowTitles);
    box.addLayout(rowImages);
    box.addStretch();
    box.getAndShow(this);
    reload();
}

vector<SvrComparison::Panel> SvrComparison::panels(int day, int hazard) const {
    vector<Panel> out;
    // SPC: days 1-3 the categorical outlook, days 4-8 the probability graphic for that day
    out.push_back({"SPC convective outlook", ""});
    out.push_back({"CSU-MLP (Colorado State)", day <= 2 ? csuBase + "day" + To::string(day) + "_3panel_latest.png"
                                                         : csuBase + "day" + To::string(day) + "_nospc_latest.png"});
    string cips;
    string cipsTitle = "CIPS analogs (Saint Louis Univ.)";
    if (day == 1 || day == 2) {
        cips = cipsBase + (day == 1 ? "F024/" : "F048/") + "Blend_fill_" + hazardCodes[static_cast<size_t>(std::clamp(hazard, 0, 2))] + ".png";
        cipsTitle += " - " + string{hazard == 0 ? "hail" : hazard == 1 ? "tornado" : "wind"};
    } else if (day <= 6) {
        const char * hours[] = {"F072", "F096", "F120", "F144"};
        cips = cipsBase + hours[day - 3] + "/Blend_fill_ALL.png";
        cipsTitle += " - all severe";
    }
    out.push_back({cipsTitle, cips});
    return out;
}

void SvrComparison::reload() {
    const auto day = comboDay.getIndex() + 1;
    const auto hazard = comboHazard.getIndex();
    comboHazard.getView()->setEnabled(day <= 2);
    const auto generationNow = ++generation;
    rowTitles.removeChildren();
    rowImages.removeChildren();
    titles.clear();
    images.clear();
    auto list = panels(day, hazard);
    textNote.setText(day >= 7 ? string{"No CIPS guidance this far out"} : string{});
    for (size_t index = 0; index < list.size(); index += 1) {
        titles.emplace_back(this, list[index].title);
        titles.back().setBold();
        rowTitles.addWidget(titles.back());
        images.emplace_back(this);
        images.back().setNumberAcross(static_cast<int>(list.size()), getWindowWidth());
        images.back().connect([this, index] {
            if (index < images.size() && !images[index].bytes.isEmpty()) {
                new ImageViewer{this, images[index].bytes};
            }
        });
        rowImages.addWidget(images.back());
    }
    resizeEventCustom();
    for (size_t index = 0; index < list.size(); index += 1) {
        if (index == 0) {
            // the SPC graphic's address needs a page read (it carries the issue time): off the UI thread
            auto url = std::make_shared<string>();
            new FutureVoid{this,
                [day, url] {
                    const auto urls = UtilitySpcSwo::getImageUrls(day <= 3 ? day : 48);
                    const auto position = day <= 3 ? 0 : day - 4;
                    if (position >= 0 && position < static_cast<int>(urls.size())) {
                        *url = urls[static_cast<size_t>(position)];
                    }
                },
                [this, url, generationNow] {
                    if (generationNow != generation || url->empty()) {
                        return;
                    }
                    new FutureBytes{this, *url, [this, generationNow] (const auto& ba) {
                        if (generationNow == generation && !images.empty()) {
                            images[0].setBytes(ba);
                        }
                    }};
                }};
        } else if (!list[index].url.empty()) {
            new FutureBytes{this, list[index].url, [this, index, generationNow] (const auto& ba) {
                if (generationNow == generation && index < images.size()) {
                    images[index].setBytes(ba);
                }
            }};
        }
    }
}

void SvrComparison::resizeEventCustom() {
    if (images.empty()) {
        return;
    }
    for (auto& image : images) {
        image.resizeToWidth(static_cast<float>(width() / static_cast<int>(images.size()) - 10));
    }
}
