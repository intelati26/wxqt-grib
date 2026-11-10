// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChartPickers.h"
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"
#include <algorithm>
#include <cmath>
#include "gfs/GfsChart.h"
#include "hurricane/HafsIntensity.h"
#include "util/Utility.h"

std::vector<std::string> ChartPickers::splitList(const std::string& text) {
    std::vector<std::string> out;
    for (size_t at = 0; at < text.size();) {
        auto end = text.find(',', at);
        end = end == std::string::npos ? text.size() : end;
        if (end > at) {
            out.push_back(text.substr(at, end - at));
        }
        at = end + 1;
    }
    return out;
}

std::string ChartPickers::joinList(const std::vector<std::string>& list) {
    std::string out;
    for (const auto& item : list) {
        out += (out.empty() ? "" : ",") + item;
    }
    return out;
}

std::vector<std::string> ChartPickers::favorites(const std::string& key) {
    return splitList(Utility::readPref(key, ""));
}

void ChartPickers::saveFavorites(const std::string& key, const std::vector<std::string>& ids) {
    Utility::writePref(key, joinList(ids));
}

std::string ChartPickers::areaLabel(const std::string& id) {
    double lat = 0, lon = 0, km = 0;
    std::string name;
    if (GfsChart::Sector box; GfsChart::parseBoxId(id, box)) {
        return "Domain of the run (" + std::to_string(static_cast<int>(std::lround(box.south))) + " to " + std::to_string(static_cast<int>(std::lround(box.north))) + " N, " +
            std::to_string(static_cast<int>(std::lround(-box.east))) + " to " + std::to_string(static_cast<int>(std::lround(-box.west))) + " W)";
    }
    if (GfsChart::parseLocalId(id, lat, lon, km, name)) {
        return (name.empty() ? std::string{"Saved location"} : name) + ", " + std::to_string(static_cast<int>(km)) + " km square";
    }
    return Hafs::label(id);
}

const std::vector<std::string>& ChartPickers::areaGroupOrder() {
    static const std::vector<std::string> order{"Around a saved location", "United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Storms in the model", "Other"};
    return order;
}

void ChartPickers::sortByGroup(std::vector<ProductPicker::Entry>& entries, const std::vector<std::string>& order) {
    std::stable_sort(entries.begin(), entries.end(), [&order] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
        return std::find(order.begin(), order.end(), a.group) < std::find(order.begin(), order.end(), b.group);
    });
}

std::vector<ProductPicker::Entry> ChartPickers::areaEntries(const std::vector<std::string>& ids) {
    std::vector<ProductPicker::Entry> entries;
    for (const auto& id : ids) {
        entries.push_back({id, areaLabel(id), ProductPicker::sectorGroup(id)});
    }
    sortByGroup(entries, areaGroupOrder());
    return entries;
}

std::vector<ProductPicker::Entry> ChartPickers::chartEntries(const std::string& model, const std::vector<std::string>& ids, const std::vector<std::string>& labels) {
    std::vector<ProductPicker::Entry> entries;
    for (size_t i = 0; i < ids.size() && i < labels.size(); i++) {
        const auto * product = GfsChart::product(ids[i], model);
        entries.push_back({ids[i], labels[i], product ? GfsChart::category(*product) : std::string{"Other"}});
    }
    return entries;
}

ProductPicker * ChartPickers::chartPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current,
                                          const std::function<void(const std::string&)>& onPick, const std::vector<GfsChart::OverlayChoice>& extras, const std::vector<std::string>& ticked) {
    const auto key = "MODELFAV_" + model;
    auto * picker = new ProductPicker{parent, model, entries, current, favorites(key), extras, ticked};
    picker->onPick = onPick;
    picker->onFavorites = [key] (const std::vector<std::string>& ids) { saveFavorites(key, ids); };
    return picker;
}

ProductPicker * ChartPickers::areaPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current, const std::function<void(const std::string&)>& onPick) {
    const auto key = "SECTORFAV_" + model;
    auto * picker = new ProductPicker{parent, model, entries, current, favorites(key), {}, {}};
    picker->setWording("areas", "Show this area");
    picker->resize(360, 560);
    picker->onPick = onPick;
    picker->onFavorites = [key] (const std::vector<std::string>& ids) { saveFavorites(key, ids); };
    return picker;
}

bool ChartPickers::chooseOverlayModels(QWidget * parent, const std::string& model, const std::string& param, std::vector<std::string>& chosen, bool * keepFill) {
    const auto * here = GfsChart::product(param, model);
    QDialog dialog{parent};
    dialog.setWindowTitle("Overlay models");
    auto * layout = new QVBoxLayout{&dialog};
    auto * about = new QLabel{"Lines of " + QString::fromStdString(here != nullptr ? here->label : param) + " from these models are drawn over " + QString::fromStdString(model) +
                              "'s, for the same valid time, each model in its own color (at most four), every set of lines of the chart, the later sets dashed. Models are listed when they draw this chart.", &dialog};
    about->setWordWrap(true);
    layout->addWidget(about);
    std::vector<QCheckBox *> boxes;
    for (const bool ensembles : {false, true}) {   // two lists: the deterministic models, then the ensembles
        bool headed = false;
        for (const auto& def : GfsModels::all()) {
            if (def.id == model || def.storm || GfsChart::product(param, def.id) == nullptr || (GfsModels::pickerGroup(def.id) == "Ensembles") != ensembles) {
                continue;
            }
            if (!headed) {
                headed = true;
                layout->addWidget(new QLabel{ensembles ? "<b>Ensembles</b>" : "<b>Deterministic models</b>", &dialog});
            }
            auto * box = new QCheckBox{QString::fromStdString(def.id + "   " + def.label), &dialog};
            box->setProperty("model", QString::fromStdString(def.id));
            box->setChecked(std::find(chosen.begin(), chosen.end(), def.id) != chosen.end());
            layout->addWidget(box);
            boxes.push_back(box);
        }
    }
    if (boxes.empty()) {
        layout->addWidget(new QLabel{"No other model draws this chart.", &dialog});
    }
    QCheckBox * fill = nullptr;
    if (keepFill != nullptr) {
        fill = new QCheckBox{"Keep " + QString::fromStdString(model) + "'s fill and barbs under the lines", &dialog};
        fill->setToolTip("Off: the lines alone, so they can be read. On: the first model's colors (and wind barbs) stay beneath them.");
        fill->setChecked(*keepFill);
        layout->addWidget(fill);
    }
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    std::vector<std::string> now;
    for (auto * box : boxes) {
        if (box->isChecked() && now.size() < 4) {
            now.push_back(box->property("model").toString().toStdString());
        }
    }
    chosen = now;
    if (fill != nullptr) {
        *keepFill = fill->isChecked();
    }
    return true;
}
