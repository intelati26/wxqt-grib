// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/CamsViewer.h"
#include <algorithm>
#include <memory>
#include "objects/FutureVoid.h"
#include "util/To.h"
#include "util/Utility.h"

using UtilityCams::Run;

struct CamsViewer::Loaded {
    bool ok{false};
    string error;
    string modelId;
    UtilityCams::Model model;
    vector<Run> runs;
    std::map<string, string> sectors;
    UtilityCams::Catalog catalog;
    string sector;
    Run run;
};

namespace {
    string runLabel(const Run& run) {
        return run.date.substr(0, 4) + "-" + run.date.substr(4, 2) + "-" + run.date.substr(6, 2) + " " + run.time.substr(0, 2) + "Z";
    }
}

CamsViewer::CamsViewer(Window * parent)
    : Window{parent}
    , comboModel{this, {"loading..."}}
    , comboRun{this, {"-"}}
    , comboSector{this, {"-"}}
    , comboCategory{this, {"-"}}
    , comboProduct{this, {"-"}}
    , comboTime{this, {"-"}}
    , backForward{this, [this] { moveTime(-1); }, [this] { moveTime(1); }}
    , image{this}
    , animation{this, &image, [this] (string, string, int) {
        std::lock_guard<std::mutex> lock{loopMutex};
        return loopSpecs;
    }}
{
    setTitle("NSSL CAMs");
    notice.setWordWrap(true);
    notice.setStyleSheet("QLabel { background: #fff3cd; color: #664d03; padding: 4px 8px; border-radius: 3px; }");
    notice.hide();

    // the loop frames are layer specs: composited here, a failure reported on screen instead of dropped
    animation.frameFetcher = [this] (const string& spec) {
        string error;
        auto bytes = UtilityCams::composite(spec, error);
        if (bytes.isEmpty()) {
            std::lock_guard<std::mutex> lock{errorMutex};
            lastFrameError = error;
        }
        return bytes;
    };
    animation.labeler = [] (const string& spec, size_t index) {
        const auto label = UtilityCams::frameLabel(spec);
        return label.empty() ? To::string(static_cast<int>(index) + 1) : label;
    };
    animation.onFailure = [this] (const string& message) {
        string detail;
        {
            std::lock_guard<std::mutex> lock{errorMutex};
            detail = lastFrameError;
        }
        notify(message + (detail.empty() ? string{} : " (" + detail + ")"));
    };

    comboModel.connect([this] {
        const auto index = comboModel.getIndex();
        if (index >= 0 && index < static_cast<int>(modelList.size())) {
            loadModel(modelList[static_cast<size_t>(index)].first);
        }
    });
    comboRun.connect([this] { reloadCatalog(); });
    comboSector.connect([this] { reloadCatalog(); });
    comboCategory.connect([this] { fillProducts(); productChanged(); });
    comboProduct.connect([this] { productChanged(); });
    comboTime.connect([this] { showFrame(); });

    rowTop.addWidget(comboModel);
    rowTop.addWidget(comboRun);
    rowTop.addWidget(comboSector);
    rowTop.addWidget(comboCategory);
    rowTop.addWidget(comboProduct);
    rowTop.addWidget(comboTime);
    rowTop.addLayout(backForward);
    box.addLayout(rowTop);
    box.addWidgetReal(&notice);
    animation.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    setSize(1100, 900);
    discoverModels();
}

void CamsViewer::notify(const string& message) {
    if (message.empty()) {
        notice.hide();
        return;
    }
    notice.setText(QString::fromStdString(message));
    notice.show();
}

void CamsViewer::closeEventCustom() {
    closed = true;
    animation.stopAnimateNoDownload();
}

UtilityCams::Run CamsViewer::currentRun() const {
    const auto index = comboRun.getIndex();
    return index >= 0 && index < static_cast<int>(runs.size()) ? runs[static_cast<size_t>(index)] : Run{};
}

string CamsViewer::currentSector() const {
    const auto index = comboSector.getIndex();
    return index >= 0 && index < static_cast<int>(model.sectors.size()) ? model.sectors[static_cast<size_t>(index)] : string{};
}

const UtilityCams::Product * CamsViewer::currentProduct() const {
    const auto index = comboProduct.getIndex();
    return index >= 0 && index < static_cast<int>(productRows.size()) ? &catalog.products[productRows[static_cast<size_t>(index)]] : nullptr;
}

void CamsViewer::discoverModels() {
    notify("");
    generation += 1;
    const int thisGeneration = generation;
    auto listed = std::make_shared<vector<std::pair<string, string>>>();
    auto error = std::make_shared<string>();
    new FutureVoid{this,
        [listed, error] { UtilityCams::models(*listed, *error); },
        [this, listed, error, thisGeneration] {
            if (closed || thisGeneration != generation) return;
            if (listed->empty()) {
                notify("Could not load the model list: " + *error);
                return;
            }
            modelList = *listed;
            vector<string> names;
            for (const auto& entry : modelList) names.push_back(entry.second);
            const auto remembered = Utility::readPref("CAMS_MODEL", "");
            size_t chosen = 0;
            for (size_t i = 0; i < modelList.size(); i += 1) {
                if (modelList[i].first == remembered) chosen = i;
            }
            comboModel.block();
            comboModel.setList(names);
            comboModel.setIndex(chosen);
            comboModel.unblock();
            loadModel(modelList[chosen].first);
        }};
}

void CamsViewer::loadModel(const string& modelId) {
    notify("");
    generation += 1;
    const int thisGeneration = generation;
    animation.stopAnimateNoDownload();
    setTitle("NSSL CAMs - loading " + modelId + "...");
    const auto rememberedSector = Utility::readPref("CAMS_SECTOR", "");
    auto loaded = std::make_shared<Loaded>();
    new FutureVoid{this,
        [loaded, modelId, rememberedSector] {
            loaded->modelId = modelId;
            // one request at a time (see UtilityCams); the model info and sector names usually come from disk
            if (!UtilityCams::model(modelId, loaded->model, loaded->error)) return;
            if (!UtilityCams::recentRuns(modelId, 24, loaded->runs, loaded->error)) return;
            string sectorsError;
            UtilityCams::sectorNames(loaded->sectors, sectorsError);   // only labels; the ids work without them
            loaded->run = loaded->runs.front();
            const auto& sectors = loaded->model.sectors;
            loaded->sector = std::find(sectors.begin(), sectors.end(), rememberedSector) != sectors.end()
                ? rememberedSector : (sectors.empty() ? string{} : sectors.front());
            if (loaded->sector.empty()) {
                loaded->error = loaded->model.name + " lists no sectors";
                return;
            }
            loaded->ok = UtilityCams::catalog(loaded->model, loaded->run, loaded->sector, loaded->catalog, loaded->error);
        },
        [this, loaded, thisGeneration] {
            if (closed || thisGeneration != generation) return;
            if (!loaded->ok) {
                setTitle("NSSL CAMs");
                notify("Could not load " + loaded->modelId + ": " + loaded->error);
                return;
            }
            applyLoaded(*loaded, true);
        }};
}

void CamsViewer::reloadCatalog() {
    if (model.id.empty()) return;
    notify("");
    generation += 1;
    const int thisGeneration = generation;
    animation.stopAnimateNoDownload();
    const auto keepProduct = currentProduct() != nullptr ? currentProduct()->id : string{};
    auto loaded = std::make_shared<Loaded>();
    loaded->modelId = model.id;
    loaded->model = model;
    loaded->runs = runs;
    loaded->sectors = sectorNames;
    loaded->run = currentRun();
    loaded->sector = currentSector();
    new FutureVoid{this,
        [loaded] { loaded->ok = UtilityCams::catalog(loaded->model, loaded->run, loaded->sector, loaded->catalog, loaded->error); },
        [this, loaded, thisGeneration, keepProduct] {
            if (closed || thisGeneration != generation) return;
            if (!loaded->ok) {
                notify("Could not load the products for that run and sector: " + loaded->error);
                return;
            }
            applyLoaded(*loaded, false);
            // keep the product on screen when the new run / sector still has it
            for (size_t i = 0; i < productRows.size(); i += 1) {
                if (catalog.products[productRows[i]].id == keepProduct) {
                    comboProduct.block();
                    comboProduct.setIndex(i);
                    comboProduct.unblock();
                    productChanged();
                    break;
                }
            }
        }};
}

void CamsViewer::applyLoaded(const Loaded& loaded, bool newModel) {
    model = loaded.model;
    catalog = loaded.catalog;
    sectorNames = loaded.sectors;
    if (newModel) {
        runs = loaded.runs;
        vector<string> labels;
        for (const auto& run : runs) labels.push_back(runLabel(run));
        comboRun.block();
        comboRun.setList(labels);
        comboRun.setIndex(0);
        comboRun.unblock();

        vector<string> sectorLabels;
        size_t sectorIndex = 0;
        for (size_t i = 0; i < model.sectors.size(); i += 1) {
            const auto found = sectorNames.find(model.sectors[i]);
            sectorLabels.push_back(found != sectorNames.end() && !found->second.empty() ? found->second : model.sectors[i]);
            if (model.sectors[i] == loaded.sector) sectorIndex = i;
        }
        comboSector.block();
        comboSector.setList(sectorLabels);
        comboSector.setIndex(sectorIndex);
        comboSector.unblock();
        Utility::writePref("CAMS_MODEL", model.id);
    }
    fillCategories();
    fillProducts();
    // the remembered product when this model has it, else the first listed
    const auto remembered = Utility::readPref("CAMS_PRODUCT", "");
    for (size_t i = 0; i < productRows.size(); i += 1) {
        if (catalog.products[productRows[i]].id == remembered) {
            comboProduct.block();
            comboProduct.setIndex(i);
            comboProduct.unblock();
        }
    }
    productChanged();
}

void CamsViewer::fillCategories() {
    categoryIds.clear();
    vector<string> labels;
    categoryIds.push_back("");
    labels.push_back("All products");
    auto present = [this] (const string& category) {
        for (const auto& product : catalog.products) {
            if (product.category == category ||
                std::find(product.otherCategories.begin(), product.otherCategories.end(), category) != product.otherCategories.end()) {
                return true;
            }
        }
        return false;
    };
    for (const auto& group : catalog.groups) {
        for (const auto& id : group.second) {
            if (present(id)) {
                categoryIds.push_back(id);
                const auto label = catalog.categoryLabel.find(id);
                labels.push_back(group.first + ": " + (label != catalog.categoryLabel.end() ? label->second : id));
            }
        }
    }
    comboCategory.block();
    comboCategory.setList(labels);
    comboCategory.setIndex(0);
    comboCategory.unblock();
}

void CamsViewer::fillProducts() {
    const auto categoryIndex = comboCategory.getIndex();
    const string category = categoryIndex >= 0 && categoryIndex < static_cast<int>(categoryIds.size()) ? categoryIds[static_cast<size_t>(categoryIndex)] : string{};
    productRows.clear();
    vector<string> labels;
    for (size_t i = 0; i < catalog.products.size(); i += 1) {
        const auto& product = catalog.products[i];
        const bool inCategory = category.empty() || product.category == category ||
            std::find(product.otherCategories.begin(), product.otherCategories.end(), category) != product.otherCategories.end();
        if (inCategory) {
            productRows.push_back(i);
            labels.push_back(product.label);
        }
    }
    comboProduct.block();
    comboProduct.setList(labels);
    comboProduct.setIndex(0);
    comboProduct.unblock();
}

void CamsViewer::productChanged() {
    const auto * product = currentProduct();
    if (product == nullptr) {
        return;
    }
    Utility::writePref("CAMS_PRODUCT", product->id);
    notify("");
    generation += 1;
    const int thisGeneration = generation;
    const auto modelId = model.id;
    const auto run = currentRun();
    const auto productId = product->id;
    const auto sector = currentSector();
    auto latest = std::make_shared<int>(-1);
    // which forecast times are already published for this run is a separate (small) question to the site
    new FutureVoid{this,
        [latest, modelId, run, productId, sector] { *latest = UtilityCams::latestAvailableSeconds(modelId, run, productId, sector); },
        [this, latest, thisGeneration] {
            if (closed || thisGeneration != generation) return;
            fillTimes(*latest);
            showFrame();
            refreshLoop();
        }};
}

void CamsViewer::fillTimes(int latestSeconds) {
    times.clear();
    vector<string> labels;
    const auto * product = currentProduct();
    if (product == nullptr) return;
    const auto run = currentRun();
    for (const auto seconds : product->times) {
        if (latestSeconds >= 0 && seconds > latestSeconds) continue;   // not published yet
        times.push_back(seconds);
        string frame = UtilityCams::frameLabel(UtilityCams::imageUrl(model.id, run, product->id, "x", seconds));
        labels.push_back(frame + "  " + UtilityCams::validLabel(run, seconds));
    }
    comboTime.block();
    comboTime.setList(labels.empty() ? vector<string>{"none published yet"} : labels);
    comboTime.setIndex(0);
    comboTime.unblock();
    if (times.empty()) {
        notify("No forecast times of this product are published for this run yet - try an older run.");
    }
}

void CamsViewer::moveTime(int step) {
    if (times.empty()) return;
    const auto next = std::clamp(comboTime.getIndex() + step, 0, static_cast<int>(times.size()) - 1);
    comboTime.block();
    comboTime.setIndex(static_cast<size_t>(next));
    comboTime.unblock();
    showFrame();
}

void CamsViewer::showFrame() {
    const auto * product = currentProduct();
    const auto timeIndex = comboTime.getIndex();
    if (product == nullptr || timeIndex < 0 || timeIndex >= static_cast<int>(times.size())) {
        return;
    }
    const auto seconds = times[static_cast<size_t>(timeIndex)];
    const auto spec = UtilityCams::layerSpec(model.id, currentRun(), *product, currentSector(), seconds);
    setTitle("NSSL CAMs - " + model.name + " - " + product->label + " - " + UtilityCams::frameLabel(spec));
    generation += 1;
    const int thisGeneration = generation;
    auto bytes = std::make_shared<QByteArray>();
    auto error = std::make_shared<string>();
    new FutureVoid{this,
        [bytes, error, spec] { *bytes = UtilityCams::composite(spec, *error); },
        [this, bytes, error, thisGeneration] {
            if (closed || thisGeneration != generation) return;
            if (bytes->isEmpty()) {
                notify("Could not load this picture: " + *error);
                return;
            }
            notify("");
            image.setBytesKeepView(*bytes);
            animation.setCurrentBytes(*bytes);
        }};
}

void CamsViewer::refreshLoop() {
    const auto * product = currentProduct();
    if (product == nullptr || times.empty()) {
        animation.clear();
        return;
    }
    {
        std::lock_guard<std::mutex> lock{loopMutex};
        loopSpecs.clear();
        for (const auto seconds : times) {
            loopSpecs.push_back(UtilityCams::layerSpec(model.id, currentRun(), *product, currentSector(), seconds));
        }
    }
    const auto run = currentRun();
    animation.product = model.id + "_" + run.key() + "_" + product->id;   // names a saved file
    animation.sector = currentSector();
    animation.refresh();
}
