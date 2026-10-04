// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "dashboard/PlaceholderDashboard.h"
#include "ui/UiStandards.h"
#include <QLayout>
#include <QWidget>

PlaceholderDashboard::PlaceholderDashboard(Window * parent, const string& title, const string& intro, const vector<Panel>& panels, const vector<Link>& links)
    : Window{parent}
    , sw{this, box}
    , textIntro{this, intro}
    , textNote{this, "Planned - not built yet. Each tile below is a panel this screen will have; nothing in it is live."}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle(title);
    textIntro.setWordWrap(true);
    textNote.setGray();
    textNote.setWordWrap(true);
    box.addWidget(textIntro);
    box.addWidget(textNote);
    for (const auto& link : links) {
        linkButtons.emplace_back(this, None, link.label);
        const auto open = link.open;
        linkButtons.back().connect([this, open] { open(this); });
        rowLinks.addWidget(linkButtons.back());
    }
    if (!links.empty()) {
        rowLinks.addStretch();
        box.addLayout(rowLinks);
    }
    box.addLayout(flow);
    box.addStretch();
    for (const auto& panel : panels) {
        auto * holder = new QWidget{this};
        holder->setObjectName("placeholderTile");
        holder->setStyleSheet("QWidget#placeholderTile { border: 1px dashed gray; }");
        tileBoxes.emplace_back();
        texts.emplace_back(this, panel.title);
        texts.back().setBold();
        texts.back().setBlue();
        tileBoxes.back().addWidget(texts.back());
        texts.emplace_back(this, panel.step.empty() ? string{"Planned"} : "Planned - " + panel.step);
        texts.back().setGray();
        tileBoxes.back().addWidget(texts.back());
        texts.emplace_back(this, panel.what);
        texts.back().setWordWrap(true);
        tileBoxes.back().addWidget(texts.back());
        if (!panel.source.empty()) {
            texts.emplace_back(this, "Data: " + panel.source);
            texts.back().setGray();
            texts.back().setWordWrap(true);
            tileBoxes.back().addWidget(texts.back());
        }
        tileBoxes.back().addStretch();
        holder->setLayout(tileBoxes.back().getView());
        holder->layout()->setContentsMargins(8, 6, 8, 6);
        holder->layout()->setSpacing(4);
        holder->setFixedWidth(UiStandards::tileWidth);
        flow.addWidgetReal(holder);
    }
}
