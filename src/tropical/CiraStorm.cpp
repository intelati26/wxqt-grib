// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/CiraStorm.h"
#include "misc/ImageViewer.h"
#include "objects/FutureBytes.h"
#include "objects/FutureText.h"
#include "objects/FutureVoid.h"
#include "tropical/CiraLoopViewer.h"

CiraStorm::CiraStorm(Window * parent, const string& stormId, const string& title)
    : Window{parent}
    , stormId{stormId}
    , title{title}
    , sw{this, box}
    , textHeader{this, title}
    , textNote{this, "CIRA / RAMMB experimental tropical cyclone products (Colorado State University, NOAA). Click a picture to enlarge it."}
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
    rowButtons.addStretch();
    for (const auto& product : UtilityCira::products()) {
        const auto found = page.imageUrl.find(product.key);
        if (found == page.imageUrl.end()) {
            continue;
        }
        images.emplace_back(this);
        auto& image = images.back();
        image.imageSize = 380;
        image.getView()->setToolTip(QString::fromStdString(product.label));
        const auto url = found->second;
        const auto label = product.label;
        image.connect([this, url, label] { new ImageViewer{this, url, label}; });
        flowImages.addWidget(image);
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
