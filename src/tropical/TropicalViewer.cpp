// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/TropicalViewer.h"
#include <memory>
#include <QWidget>
#include "nhc/Nhc.h"
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "tropical/CiraStorm.h"

TropicalViewer::TropicalViewer(Window * parent)
    : Window{parent}
    , sw{this, box}
    , buttonNhc{this, None, "NHC: Atlantic, East and Central Pacific (outlooks, advisories, sea surface temperatures)"}
    , buttonRefresh{this, None, "Refresh"}
    , textNote{this, "Active tropical cyclones worldwide - CIRA / RAMMB (Colorado State University, NOAA) experimental products. Click a storm."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("Tropical");
    textNote.setWordWrap(true);
    buttonNhc.connect([this] { new Nhc{this}; });
    buttonRefresh.connect([this] { reload(); });
    rowTop.addWidget(buttonNhc);
    rowTop.addWidget(buttonRefresh);
    rowTop.addStretch();
    box.addLayout(rowTop);
    box.addWidget(textNote);
    box.addLayout(boxStorms);
    box.addStretch();
    reload();
}

void TropicalViewer::reload() {
    textNote.setText(string{"Loading the active tropical cyclones..."});
    auto storms = std::make_shared<vector<UtilityCira::Storm>>();
    auto infrared = std::make_shared<vector<string>>();
    auto error = std::make_shared<string>();
    new FutureVoid{this,
        [storms, infrared, error] {
            if (!UtilityCira::activeStorms(*storms, *error)) {
                return;
            }
            for (const auto& storm : *storms) {   // a current infrared picture of each (the storm's own page names it)
                UtilityCira::StormPage page;
                string ignored;
                UtilityCira::stormPage(storm.id, page, ignored);
                const auto found = page.imageUrl.find("4kmirimg");
                infrared->push_back(found == page.imageUrl.end() ? string{} : found->second);
            }
        },
        [this, storms, infrared, error] {
            if (closed) {
                return;
            }
            if (!error->empty()) {
                textNote.setText(*error);
                return;
            }
            build(*storms, *infrared);
        }};
}

void TropicalViewer::build(const vector<UtilityCira::Storm>& storms, const vector<string>& infraredUrls) {
    // start again (a refresh)
    boxStorms.removeChildren();
    headings.clear();
    flows.clear();
    images.clear();
    captions.clear();
    tileBoxes.clear();
    if (storms.empty()) {
        textNote.setText(string{"No active tropical cyclones anywhere right now (CIRA / RAMMB)."});
        return;
    }
    textNote.setText(string{"Active tropical cyclones worldwide - CIRA / RAMMB (Colorado State University, NOAA) experimental products. Click a storm."});
    string currentBasin;
    for (size_t i = 0; i < storms.size(); i += 1) {
        const auto& storm = storms[i];
        if (storm.basin != currentBasin) {
            currentBasin = storm.basin;
            headings.emplace_back(this, currentBasin);
            headings.back().setBold();
            headings.back().setBlue();
            boxStorms.addWidget(headings.back());
            flows.emplace_back();
            boxStorms.addLayout(flows.back());
        }
        // a tile: the infrared picture over the storm's title
        auto * tile = new QWidget{this};
        tileBoxes.emplace_back();
        images.emplace_back(this);
        images.back().imageSize = 330;
        captions.emplace_back(this, storm.title);
        captions.back().setWordWrap(true);
        tileBoxes.back().addWidget(images.back());
        tileBoxes.back().addWidget(captions.back());
        tile->setLayout(tileBoxes.back().getView());
        flows.back().addWidgetReal(tile);
        const auto id = storm.id;
        const auto title = storm.title;
        images.back().connect([this, id, title] { new CiraStorm{this, id, title}; });
        const auto imageIndex = images.size() - 1;
        if (!infraredUrls[i].empty()) {
            new FutureBytes{this, infraredUrls[i], [this, imageIndex] (const auto& bytes) {
                if (!closed && imageIndex < images.size()) {
                    images[imageIndex].setBytes(bytes);
                }
            }};
        } else {
            images.back().getView()->setText("(no picture yet)");
        }
    }
}
