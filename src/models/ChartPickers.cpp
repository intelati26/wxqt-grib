// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChartPickers.h"
#include <QCheckBox>
#include <QDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include "gfs/GfsModels.h"
#include "gfs/GfsRender.h"
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

std::string ChartPickers::crossFillName(const std::string& crossFill) {
    const auto colon = crossFill.find(':');
    if (colon == std::string::npos) {
        return {};
    }
    const auto * product = GfsChart::product(crossFill.substr(colon + 1), crossFill.substr(0, colon));
    return crossFill.substr(0, colon) + " " + (product != nullptr && !product->label.empty() ? product->label : crossFill.substr(colon + 1));
}

bool ChartPickers::chooseCrossFill(QWidget * parent, const std::string& model, std::string& crossFill) {
    QDialog dialog{parent};
    dialog.setWindowTitle("Another model's field");
    dialog.resize(480, 520);
    auto * layout = new QVBoxLayout{&dialog};
    auto * about = new QLabel{"Lay a chart of another model over " + QString::fromStdString(model) + "'s as a translucent fill, for the same valid time: its newest run that has it (up to three runs back). "
                              "The lowest sixth of its scale is left clear so that the chart under it reads. A time the other model has no field for leaves the chart as it is, and its title says so.", &dialog};
    about->setWordWrap(true);
    layout->addWidget(about);
    auto * models = new QComboBox{&dialog};
    for (const auto& def : GfsModels::all()) {
        if (!def.storm && GfsRender::drawsModel(def.id)) {
            models->addItem(QString::fromStdString(def.id + "   " + def.label), QString::fromStdString(def.id));
        }
    }
    // the common ones first (the SPC's processing of the REFS: the calibrated thunderstorm probability of 4 hours is the most used); only those this build has
    auto * common = new QComboBox{&dialog};
    for (const auto& [id, label] : std::vector<std::pair<std::string, std::string>>{
             {"SPCPOST:thunder_4hr", "SPC REFS thunderstorm probability, 4-hour (REFSCT)"}, {"SPCPOST:thunder_1hr", "SPC REFS thunderstorm probability, 1-hour (REFSCT)"},
             {"SPCPOST:thunder_full", "SPC REFS thunderstorm probability, rest of the convective day (REFSCT)"}, {"SPCREFS:cref_max", "SPC REFS composite reflectivity, ensemble maximum"},
             {"SPCREFS:uh25_04h_max_overlap_max", "SPC REFS 4-h maximum 2-5 km updraft helicity, ensemble maximum"}}) {
        if (GfsChart::product(id.substr(id.find(':') + 1), id.substr(0, id.find(':'))) != nullptr) {
            common->addItem(QString::fromStdString(label), QString::fromStdString(id));
        }
    }
    common->insertItem(0, "Common choices...", QString{});
    common->setCurrentIndex(0);
    layout->addWidget(common);
    auto * search = new QLineEdit{&dialog};
    search->setPlaceholderText("Search the charts");
    search->setClearButtonEnabled(true);
    auto * list = new QListWidget{&dialog};
    layout->addWidget(models);
    layout->addWidget(search);
    layout->addWidget(list, 1);
    const auto fill = [models, search, list, model] {
        list->clear();
        const auto other = models->currentData().toString().toStdString();
        const auto needle = search->text().trimmed();
        for (const auto& product : GfsChart::products()) {
            if (product.source != other || !product.fill || product.linesOnly) {
                continue;
            }
            const auto text = QString::fromStdString(product.label + "   (" + product.id + ")");
            if (!needle.isEmpty() && !text.contains(needle, Qt::CaseInsensitive)) {
                continue;
            }
            auto * item = new QListWidgetItem{text, list};
            item->setData(Qt::UserRole, QString::fromStdString(product.id));
        }
    };
    const auto colon = crossFill.find(':');
    if (colon != std::string::npos) {
        const auto at = models->findData(QString::fromStdString(crossFill.substr(0, colon)));
        if (at >= 0) {
            models->setCurrentIndex(at);
        }
    }
    const auto choose = [models, list, search] (const std::string& id) {   // put the model and the chart on a "model:chart id"
        const auto colonAt = id.find(':');
        const auto at = models->findData(QString::fromStdString(id.substr(0, colonAt)));
        if (colonAt == std::string::npos || at < 0) {
            return;
        }
        search->clear();
        models->setCurrentIndex(at);   // (fills the list)
        for (int i = 0; i < list->count(); i++) {
            if (list->item(i)->data(Qt::UserRole).toString().toStdString() == id.substr(colonAt + 1)) {
                list->setCurrentRow(i);
                list->scrollToItem(list->item(i));
            }
        }
    };
    QObject::connect(common, &QComboBox::activated, &dialog, [common, choose] (int index) { choose(common->itemData(index).toString().toStdString()); });
    QObject::connect(models, &QComboBox::currentIndexChanged, &dialog, fill);
    QObject::connect(search, &QLineEdit::textChanged, &dialog, fill);
    fill();
    if (colon != std::string::npos) {
        for (int i = 0; i < list->count(); i++) {
            if (list->item(i)->data(Qt::UserRole).toString().toStdString() == crossFill.substr(colon + 1)) {
                list->setCurrentRow(i);
            }
        }
    }
    if (colon == std::string::npos && common->count() > 1) {   // nothing chosen yet: the first common choice is the one in the list
        choose(common->itemData(1).toString().toStdString());
    }
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog};
    auto * none = buttons->addButton("No field", QDialogButtonBox::DestructiveRole);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(none, &QPushButton::clicked, &dialog, [&dialog] { dialog.done(2); });
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    layout->addWidget(buttons);
    const int result = dialog.exec();
    if (result == 2) {
        crossFill.clear();
        return true;
    }
    if (result != QDialog::Accepted || list->currentItem() == nullptr) {
        return false;
    }
    crossFill = models->currentData().toString().toStdString() + ":" + list->currentItem()->data(Qt::UserRole).toString().toStdString();
    return true;
}
