// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "hurricane/HafsViewer.h"
#include <cstdlib>
#include <utility>
#include "gfs/GfsChart.h"
#include "objects/FutureVoid.h"
#include "ui/ActivityLabel.h"

namespace {
    // the basin letter of a model id: 09l the Atlantic, 18e the East Pacific
    string basinName(char letter) {
        switch (letter) {
        case 'l': return "Atlantic";
        case 'e': return "East Pacific";
        case 'c': return "Central Pacific";
        case 'w': return "West Pacific";
        case 's': return "South Hemisphere";
        case 'a': return "Arabian Sea";
        case 'b': return "Bay of Bengal";
        default: return "";
        }
    }

    std::vector<string> hours() {
        std::vector<string> out;
        for (int h = 0; h <= 126; h += 3) {
            out.push_back((h < 100 ? (h < 10 ? "00" : "0") : "") + std::to_string(h));
        }
        return out;
    }

    std::vector<string> productLabels(std::vector<string>& ids) {
        std::vector<string> labels;
        ids.clear();
        for (const auto& p : GfsChart::products()) {
            if (p.source == "HAFSA") {
                ids.push_back(p.id);
                labels.push_back(p.label);
            }
        }
        return labels;
    }
}

string HafsViewer::modelId(const string& nhcId) {
    if (nhcId.size() < 4) {
        return nhcId;
    }
    const auto basin = nhcId.substr(0, 2);
    return nhcId.substr(2, 2) + (basin == "al" ? "l" : basin == "ep" ? "e" : basin == "cp" ? "c" : basin == "wp" ? "w" : "x");
}

HafsViewer::HafsViewer(Window * parent, const string& storm, const string& name)
    : Window{parent}
    , photo{this, FullWithHeight, [this] { return getPhotoHeight(); }}
    , comboModel{this, {"HAFS-A", "HAFS-B"}}
    , comboStorm{this, {"Looking for storms..."}}
    , comboProduct{this, productLabels(productIds)}
    , comboTime{this, hours()}
    , backForward{this, [this] { step(-1); }, [this] { step(1); }}
    , textStatus{this, ""}
    , first{modelId(storm)}
    , firstName{name}
    , session{std::make_shared<GfsRender::Session>()}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("HAFS hurricane model" + (name.empty() ? string{} : " - " + name));
    comboTime.setIndex(8);   // 24 hours
    comboModel.connect([this] { loadStorms(); });
    comboStorm.connect([this] { draw(); });
    comboProduct.connect([this] { draw(); });
    comboTime.connect([this] { draw(); });
    row.addWidget(comboModel);
    row.addWidget(comboStorm);
    row.addWidget(comboProduct);
    row.addWidget(comboTime);
    row.addLayout(backForward);
    box.addLayout(row);
    box.addWidget(textStatus);
    box.addWidgetAndCenter(photo);
    box.addWidgetReal(new ActivityLabel{this});
    box.getAndShow(this);
    loadStorms();
}

void HafsViewer::loadStorms() {
    const int mine = ++loading;
    textStatus.setText(string{"Looking for the storms the model is running..."});
    const auto name = model();
    auto result = std::make_shared<std::pair<std::vector<string>, string>>();
    new FutureVoid{this, [name, result] { result->first = GfsRender::hafsStorms(name, result->second); },
                   [this, result, mine] {
                       if (mine == loading) {
                           fillStorms(result->first, result->second);
                       }
                   }};
}

void HafsViewer::fillStorms(const std::vector<string>& found, const string& cycle) {
    const string keep = !storms.empty() && comboStorm.getIndex() >= 0 && comboStorm.getIndex() < static_cast<int>(storms.size()) ? storms[static_cast<size_t>(comboStorm.getIndex())] : first;
    storms = found;
    std::vector<string> labels;
    for (const auto& s : storms) {
        labels.push_back(s + "  " + basinName(s.empty() ? ' ' : s.back()) + (s == first && !firstName.empty() ? "  " + firstName : ""));
    }
    comboStorm.block();
    if (storms.empty()) {
        comboStorm.setList({"No active storms in the model"});
        comboStorm.unblock();
        textStatus.setText(string{model() == "HAFSB" ? "HAFS-B has no storm in its newest cycle." : "HAFS-A has no storm in its newest cycle."});
        return;
    }
    comboStorm.setList(labels);
    size_t index = 0;
    for (size_t i = 0; i < storms.size(); i++) {
        if (storms[i] == keep) {
            index = i;
        }
    }
    comboStorm.setIndex(index);
    comboStorm.unblock();
    textStatus.setText("Newest cycle " + cycle + ", " + std::to_string(storms.size()) + (storms.size() == 1 ? " storm." : " storms."));
    draw();
}

void HafsViewer::step(int by) {
    const int next = comboTime.getIndex() + by;
    if (next < 0 || next >= 43) {
        return;
    }
    comboTime.block();
    comboTime.setIndex(static_cast<size_t>(next));
    comboTime.unblock();
    draw();
}

void HafsViewer::draw() {
    const int s = comboStorm.getIndex(), p = comboProduct.getIndex();
    if (storms.empty() || s < 0 || s >= static_cast<int>(storms.size()) || p < 0 || p >= static_cast<int>(productIds.size())) {
        return;
    }
    const int mine = ++drawing;
    const auto name = model(), storm = storms[static_cast<size_t>(s)], param = productIds[static_cast<size_t>(p)];
    const int hour = comboTime.getIndex() * 3;
    setTitle(name + " " + storm + " +" + std::to_string(hour) + " h");
    auto shared = session;
    auto result = std::make_shared<std::pair<QByteArray, string>>();
    new FutureVoid{this, [=] { result->first = GfsRender::png(*shared, name, param, storm, "", hour, {}, result->second); },
                   [this, result, mine] {
                       if (mine != drawing) {
                           return;
                       }
                       if (!result->first.isEmpty()) {
                           photo.setBytes(result->first);
                       } else {
                           textStatus.setText("HAFS: " + (result->second.empty() ? string{"nothing could be drawn"} : result->second));
                       }
                   }};
}
