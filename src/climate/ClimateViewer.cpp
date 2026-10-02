// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/ClimateViewer.h"
#include <algorithm>
#include <memory>
#include <QWidget>
#include "climate/ClimateChart.h"
#include "climate/ClimateHistoryViewer.h"
#include "misc/ImageViewer.h"
#include "misc/TextViewerStatic.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"

namespace {
    const vector<int> yearChoices{5, 10, 20, 40, 80};
}

ClimateViewer::ClimateViewer(Window * parent)
    : Window{parent}
    , sw{this, box}
    , buttonRefresh{this, None, "Refresh"}
    , buttonDiscussion{this, None, "ENSO discussion (full text)"}
    , textYears{this, "Charts show:"}
    , comboYears{this, {"5 years", "10 years", "20 years", "40 years", "All"}}
    , textStatus{this, "Loading the climate and ocean products..."}
    , textSynopsis{this, ""}
    , textNext{this, ""}
    , headingCharts{this, "Indices and cycles"}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Climate and ocean");
    textStatus.setBold();
    textSynopsis.setWordWrap(true);
    textNext.setGray();
    headingCharts.setBold();
    headingCharts.setBlue();
    comboYears.setIndex(1);
    comboYears.connect([this] { rebuildCharts(); });
    buttonRefresh.connect([this] { reload(); });
    buttonDiscussion.connect([this] {
        if (data && data->ensoOk) {
            new TextViewerStatic{this, data->enso.discussion, "ENSO diagnostic discussion - " + data->enso.issued, 800, 700};
        }
    });
    rowTop.addWidget(buttonRefresh);
    rowTop.addWidget(buttonDiscussion);
    rowTop.addWidget(textYears);
    rowTop.addWidget(comboYears);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidget(textStatus);
    box.addWidget(textSynopsis);
    box.addWidget(textNext);
    // the source tables, as text
    for (const auto& product : UtilityClimate::textProducts()) {
        textButtons.emplace_back(this, None, product.label);
        textButtons.back().connect([this, product] { openText(product); });
        rowText.addWidget(textButtons.back());
    }
    box.addLayout(rowText);
    box.addWidget(headingCharts);
    box.addLayout(flowCharts);
    box.addLayout(boxSections);
    box.addStretch();
    reload();
}

void ClimateViewer::reload() {
    textStatus.setText(string{"Loading the climate and ocean products..."});
    auto fresh = std::make_shared<Data>();
    new FutureVoid{this,
        [fresh] {
            fresh->ensoOk = UtilityClimate::ensoStatus(fresh->enso, fresh->ensoError);
            for (const auto& info : UtilityClimate::indices()) {
                UtilityClimate::Series one;
                string ignored;
                UtilityClimate::loadSeries(info, one, ignored);
                fresh->series.push_back(std::move(one));
            }
        },
        [this, fresh] {
            if (closed) {
                return;
            }
            data = fresh;
            build();
        }};
}

void ClimateViewer::build() {
    if (data->ensoOk) {
        textStatus.setText("ENSO Alert System status: " + data->enso.status + "   (CPC, issued " + data->enso.issued + ")");
        textSynopsis.setText(data->enso.synopsis);
        textNext.setText(data->enso.next.empty() ? string{} : "Next ENSO diagnostic discussion: " + data->enso.next);
    } else {
        textStatus.setText(data->ensoError);
        textSynopsis.setText(string{});
        textNext.setText(string{});
    }
    rebuildCharts();

    // the pictures: start again (a refresh)
    boxSections.removeChildren();
    headings.clear();
    flows.clear();
    images.clear();
    captions.clear();
    tileBoxes.clear();
    string currentSection;
    for (const auto& tile : UtilityClimate::tiles()) {
        if (tile.section != currentSection) {
            currentSection = tile.section;
            headings.emplace_back(this, currentSection);
            headings.back().setBold();
            headings.back().setBlue();
            boxSections.addWidget(headings.back());
            flows.emplace_back();
            boxSections.addLayout(flows.back());
        }
        // a tile: the picture over its caption
        auto * holder = new QWidget{this};
        tileBoxes.emplace_back();
        images.emplace_back(this);
        images.back().imageSize = 400;
        captions.emplace_back(this, tile.history.empty() ? tile.label : tile.label + " - click for the history and a loop");
        captions.back().setWordWrap(true);
        tileBoxes.back().addWidget(images.back());
        tileBoxes.back().addWidget(captions.back());
        holder->setLayout(tileBoxes.back().getView());
        holder->setFixedWidth(410);
        flows.back().addWidgetReal(holder);
        const auto big = tile.full.empty() ? tile.url : tile.full;
        const auto label = tile.label;
        const auto history = tile.history;
        images.back().connect([this, big, label, history] {
            if (history.empty()) {
                new ImageViewer{this, big, label};
            } else {
                new ClimateHistoryViewer{this, history};
            }
        });
        const auto index = images.size() - 1;
        new FutureBytes{this, tile.url, [this, index] (const auto& bytes) {
            if (closed || index >= images.size()) {
                return;
            }
            if (bytes.isEmpty()) {
                images[index].getView()->setText("(not available right now)");
                return;
            }
            images[index].setBytes(bytes);
        }};
    }
    built = true;
}

void ClimateViewer::rebuildCharts() {
    flowCharts.removeChildren();
    if (!data) {
        return;
    }
    const auto years = yearChoices[static_cast<size_t>(std::max(0, comboYears.getIndex()))];
    const auto& infos = UtilityClimate::indices();
    for (size_t i = 0; i < infos.size() && i < data->series.size(); i += 1) {
        if (data->series[i].empty()) {
            continue;
        }
        flowCharts.addWidgetReal(new ClimateChart{infos[i], data->series[i], years, this});
    }
}

void ClimateViewer::openText(const UtilityClimate::TextProduct& product) {
    new FutureText{this, product.url, [this, product] (const string& text) {
        if (closed) {
            return;
        }
        if (text.empty() || text.find("<html") != string::npos || text.find("<HTML") != string::npos) {
            new TextViewerStatic{this, product.label + " is not available right now.", product.label, 500, 150};
            return;
        }
        new TextViewerStatic{this, UtilityClimate::tailOf(text, product.tailLines), product.label, 800, 700};
    }};
}
