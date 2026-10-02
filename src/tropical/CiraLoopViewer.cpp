// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "tropical/CiraLoopViewer.h"
#include <algorithm>
#include <memory>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "tropical/UtilityCira.h"
#include "util/To.h"

namespace {
    vector<string> loopLabels() {
        vector<string> labels;
        for (const auto& p : UtilityCira::products()) {
            if (p.loop) labels.push_back(p.label);
        }
        return labels;
    }
}

CiraLoopViewer::CiraLoopViewer(Window * parent, const string& stormId, const string& stormTitle, const string& product)
    : Window{parent}
    , stormId{stormId}
    , stormTitle{stormTitle}
    , image{this}
    , comboProduct{this, loopLabels()}
    , comboCount{this, {"12", "24", "48", "96"}}
    , objectAnimate{this, &image, [] (string productKey, string id, int count) { return UtilityCira::frameUrls(id, productKey, count); }}
{
    setAttribute(Qt::WA_DeleteOnClose);
    for (const auto& p : UtilityCira::products()) {
        if (p.loop) productKeys.push_back(p.key);
    }
    const auto found = std::find(productKeys.begin(), productKeys.end(), product);
    comboProduct.setIndex(found == productKeys.end() ? 0 : static_cast<size_t>(found - productKeys.begin()));
    comboCount.setIndex(1);
    objectAnimate.sector = stormId;   // the "sector" of the shared animation bar is the storm
    objectAnimate.product = productKeys[static_cast<size_t>(std::max(0, comboProduct.getIndex()))];
    objectAnimate.setFrameCount(24);
    comboProduct.connect([this] {
        objectAnimate.product = productKeys[static_cast<size_t>(std::max(0, comboProduct.getIndex()))];
        reload();
    });
    comboCount.connect([this] { objectAnimate.setFrameCount(To::Int(comboCount.getValue())); });
    row.addWidget(comboProduct);
    row.addWidget(comboCount);
    row.addStretch();
    box.addLayout(row);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    reload();
}

void CiraLoopViewer::reload() {
    setTitle("CIRA / RAMMB - " + stormTitle);
    objectAnimate.stopAnimateNoDownload();
    auto latest = std::make_shared<vector<string>>();
    const auto id = stormId;
    const auto key = objectAnimate.product;
    new FutureVoid{this,
        [latest, id, key] { *latest = UtilityCira::frameUrls(id, key, 1); },
        [this, latest] {
            if (closed || latest->empty()) {
                return;
            }
            new FutureBytes{this, latest->back(), [this] (const auto& bytes) { showLatest(bytes); }};
        }};
    objectAnimate.refresh();
}

void CiraLoopViewer::showLatest(const QByteArray& bytes) {
    if (bytes.isEmpty()) {
        return;
    }
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
}
