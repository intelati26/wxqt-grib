// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "climate/ClimateHistoryViewer.h"
#include <algorithm>
#include <memory>
#include <QDate>
#include "objects/FutureBytes.h"
#include "objects/FutureVoid.h"
#include "util/To.h"

namespace {
    vector<string> labels() {
        vector<string> out;
        for (const auto& p : UtilityClimate::historyProducts()) {
            out.push_back(p.label);
        }
        return out;
    }

    const vector<int> steps{1, 7, 30};

    // "2026-09-30" for a dated picture's address, else the frame's place in its fixed list
    string frameLabel(const string& url, size_t index) {
        const auto at = url.rfind('_');
        const auto stamp = at == string::npos ? string{} : url.substr(at + 1, 8);
        if (stamp.size() == 8 && std::all_of(stamp.begin(), stamp.end(), [] (char c) { return c >= '0' && c <= '9'; })) {
            return stamp.substr(0, 4) + "-" + stamp.substr(4, 2) + "-" + stamp.substr(6, 2);
        }
        return "frame " + std::to_string(index + 1) + " (oldest first)";
    }
}

ClimateHistoryViewer::ClimateHistoryViewer(Window * parent, const string& productKey)
    : Window{parent}
    , image{this}
    , comboProduct{this, labels()}
    , comboCount{this, {"7", "14", "30", "60", "90"}}
    , comboStep{this, {"every day", "every 7 days", "every 30 days"}}
    , dateEdit{new QDateEdit{this}}
    , buttonLatest{this, None, "Latest"}
    , objectAnimate{this, &image, [] (string key, string sector, int count) {
          // the sector carries the last day and the spacing: "20260930:7"
          const auto colon = sector.find(':');
          const auto end = sector.substr(0, colon);
          const auto step = colon == string::npos ? 1 : std::atoi(sector.substr(colon + 1).c_str());
          return UtilityClimate::historyFrames(key, end, step, count);
      }}
{
    setAttribute(Qt::WA_DeleteOnClose);
    const auto& products = UtilityClimate::historyProducts();
    size_t index = 0;
    for (size_t i = 0; i < products.size(); i += 1) {
        if (products[i].key == productKey) {
            index = i;
        }
    }
    comboProduct.setIndex(index);
    comboCount.setIndex(2);
    comboStep.setIndex(0);
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat("yyyy-MM-dd");
    const auto first = QDate::fromString(QString::fromStdString(UtilityClimate::earliestHistoryDate()), "yyyyMMdd");
    dateEdit->setDateRange(first, QDate::currentDate());
    dateEdit->setDate(QDate::currentDate().addDays(-1));   // until the newest day is looked up
    objectAnimate.labeler = frameLabel;
    objectAnimate.setFrameCount(30);
    comboProduct.connect([this] { changed(); });
    comboCount.connect([this] { changed(); });
    comboStep.connect([this] { changed(); });
    QObject::connect(dateEdit, &QDateEdit::dateChanged, this, [this] { changed(); });
    buttonLatest.connect([this] {
        auto latest = std::make_shared<string>();
        new FutureVoid{this, [latest] { *latest = UtilityClimate::latestHistoryDate(); }, [this, latest] {
            if (!closed) {
                dateEdit->setDate(QDate::fromString(QString::fromStdString(*latest), "yyyyMMdd"));
            }
        }};
    });
    row.addWidget(comboProduct);
    row.addWidgetReal(dateEdit);
    row.addWidget(buttonLatest);
    row.addWidget(comboStep);
    row.addWidget(comboCount);
    row.addStretch();
    box.addLayout(row);
    objectAnimate.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    // the newest day with a picture
    auto latest = std::make_shared<string>();
    new FutureVoid{this, [latest] { *latest = UtilityClimate::latestHistoryDate(); }, [this, latest] {
        if (closed) {
            return;
        }
        dateEdit->blockSignals(true);
        dateEdit->setDate(QDate::fromString(QString::fromStdString(*latest), "yyyyMMdd"));
        dateEdit->blockSignals(false);
        changed();
    }};
}

string ClimateHistoryViewer::endDate() const {
    return dateEdit->date().toString("yyyyMMdd").toStdString();
}

void ClimateHistoryViewer::changed() {
    const auto& products = UtilityClimate::historyProducts();
    const auto& product = products[static_cast<size_t>(std::max(0, comboProduct.getIndex()))];
    dated = product.dated;
    dateEdit->setEnabled(dated);
    comboStep.getView()->setEnabled(dated);
    comboCount.getView()->setEnabled(dated);
    const auto step = steps[static_cast<size_t>(std::max(0, comboStep.getIndex()))];
    objectAnimate.product = product.key;
    objectAnimate.sector = endDate() + ":" + std::to_string(step);
    objectAnimate.setFrameCount(dated ? To::Int(comboCount.getValue()) : 14);
    reload();
}

void ClimateHistoryViewer::reload() {
    const auto& products = UtilityClimate::historyProducts();
    setTitle("Climate history - " + products[static_cast<size_t>(std::max(0, comboProduct.getIndex()))].label);
    objectAnimate.stopAnimateNoDownload();
    // the still picture: the newest frame of the loop
    const auto frames = UtilityClimate::historyFrames(objectAnimate.product, endDate(), 1, 1);
    if (!frames.empty()) {
        new FutureBytes{this, frames.back(), [this] (const auto& bytes) { showLatest(bytes); }};
    }
    objectAnimate.refresh();
}

void ClimateHistoryViewer::showLatest(const QByteArray& bytes) {
    if (closed || bytes.isEmpty()) {
        return;
    }
    image.setBytesKeepView(bytes);
    objectAnimate.setCurrentBytes(bytes);
}
