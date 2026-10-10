// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/AdvisoryViewer.h"
#include <QFont>
#include <QFontDatabase>
#include "hurricane/UtilityNhcText.h"
#include "objects/FutureVoid.h"

AdvisoryViewer::AdvisoryViewer(Window * parent, const HurricaneData::StormEntry& entry)
    : Window{parent}
    , textHeadline{this, "Loading NHC's text products..."}
{
    build("NHC advisory text - " + HurricaneData::idLabel(entry.id) + " " + entry.name,
          {{"Public advisory", entry.advisoryUrl}, {"Discussion", entry.discussionUrl}, {"Forecast / advisory", entry.forecastAdvisoryUrl},
           {"Wind speed probabilities", entry.probabilitiesUrl}});
}

AdvisoryViewer::AdvisoryViewer(Window * parent, const std::string& title, const std::vector<std::pair<std::string, std::string>>& products)
    : Window{parent}
    , textHeadline{this, "Loading NHC's text products..."}
{
    build(title, products);
}

void AdvisoryViewer::build(const std::string& title, const std::vector<std::pair<std::string, std::string>>& products) {
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(title);
    textHeadline.setWordWrap(true);
    tabs = new QTabWidget{this};
    for (const auto& [name, url] : products) {
        auto * page = new QPlainTextEdit{this};
        page->setReadOnly(true);
        page->setLineWrapMode(QPlainTextEdit::NoWrap);
        page->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        page->setPlainText(url.empty() ? "NHC lists no such product for this storm." : "Loading...");
        tabs->addTab(page, QString::fromStdString(name));
        pages.push_back(page);
    }
    box.addWidget(textHeadline);
    box.addWidgetReal(tabs, 1, Qt::Alignment{});
    box.getAndShow(this);
    resize(900, 720);
    auto texts = std::make_shared<std::vector<std::string>>(products.size());
    new FutureVoid{this,
        [texts, products] {
            for (size_t i = 0; i < products.size(); i++) {
                if (!products[i].second.empty()) {
                    (*texts)[i] = HurricaneData::loadBulletin(products[i].second);
                }
            }
        },
        [this, texts, products] {
            if (closed) {
                return;
            }
            for (size_t i = 0; i < pages.size(); i++) {
                if (!products[i].second.empty()) {
                    pages[i]->setPlainText((*texts)[i].empty() ? "Could not read this product from NHC." : QString::fromStdString((*texts)[i]));
                }
            }
            const auto headline = UtilityNhcText::headline((*texts)[0]);
            textHeadline.setText(headline.empty() ? std::string{"NHC text products"} : headline);
        }};
}
