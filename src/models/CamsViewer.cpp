// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/CamsViewer.h"
#include <algorithm>
#include <memory>
#include "objects/FutureVoid.h"
#include "util/To.h"
#include "objects/UtilityAnimationExport.h"
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
    , labelValid{this}
    , buttonSave{"Save image / loop...", this}
    , image{this}
{
    setTitle("NSSL CAMs");
    notice.setWordWrap(true);
    notice.setStyleSheet("QLabel { background: #fff3cd; color: #664d03; padding: 4px 8px; border-radius: 3px; }");
    notice.hide();

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

    rowTop.addWidget(comboModel);
    rowTop.addWidget(comboRun);
    rowTop.addWidget(comboSector);
    rowTop.addWidget(comboCategory);
    rowTop.addWidget(comboProduct);
    rowTop.addWidgetReal(&labelValid);
    rowTop.addWidgetReal(&buttonSave);
    box.addLayout(rowTop);
    box.addWidgetReal(&notice);
    strip = new TimeStrip{this};
    strip->onSelect = [this] (int index) { playTimer.stop(); if (strip->current() == index) { selectIndex(index); } };
    strip->onPlay = [this] (bool on) { startPlaying(on); };
    box.addWidgetReal(strip, 0, Qt::Alignment{});
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    QObject::connect(&playTimer, &QTimer::timeout, [this] {
        // the next picture that is drawn (a frame still on its way is waited for); the loop starts again after the last
        const int count = strip->count();
        for (int step = 1; step <= count; step++) {
            const int next = (strip->current() + step) % std::max(1, count);
            if (next < static_cast<int>(times.size()) && frames.count(times[static_cast<size_t>(next)]) > 0) {
                strip->setCurrent(next);
                selectIndex(next);
                break;
            }
        }
    });
    QObject::connect(&buttonSave, &QPushButton::clicked, [this] { saveFrames(); });
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
    playTimer.stop();
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
    startPlaying(false);
    frames.clear();
    inFlight.clear();
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
    startPlaying(false);
    frames.clear();
    inFlight.clear();
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
            prefetchGeneration += 1;
            showFrame();
            prefetch(prefetchGeneration);
        }};
}

void CamsViewer::fillTimes(int latestSeconds) {
    startPlaying(false);
    frames.clear();
    inFlight.clear();
    times.clear();
    vector<string> labels;
    const auto * product = currentProduct();
    if (product == nullptr) return;
    const auto run = currentRun();
    for (const auto seconds : product->times) {
        if (latestSeconds >= 0 && seconds > latestSeconds) continue;   // not published yet
        times.push_back(seconds);
        string frame = UtilityCams::frameLabel(UtilityCams::imageUrl(model.id, run, product->id, "x", seconds));   // "F06" (":30" for a half hour)
        labels.push_back(frame.size() > 1 && frame[0] == 'F' ? frame.substr(1).size() < 2 ? "0" + frame.substr(1) : frame.substr(1) : frame);
    }
    strip->setTimes(labels, 0);
    refreshStrip();
    if (times.empty()) {
        labelValid.setText("none published yet");
        notify("No forecast times of this product are published for this run yet - try an older run.");
    }
}

// the bar of the hours that are drawn, and the count
void CamsViewer::refreshStrip() {
    std::vector<bool> ready;
    int have = 0;
    for (const auto seconds : times) {
        const bool drawn = frames.count(seconds) > 0;
        ready.push_back(drawn);
        have += drawn ? 1 : 0;
    }
    strip->setLoaded(ready, QString{"Ready %1 of %2"}.arg(have).arg(times.size()));
}

void CamsViewer::selectIndex(int index) {
    if (index < 0 || index >= static_cast<int>(times.size())) {
        return;
    }
    showFrame();
}

void CamsViewer::startPlaying(bool on) {
    if (on) {
        playTimer.start(strip->intervalMs());
    } else {
        playTimer.stop();
        if (strip != nullptr) {
            strip->stop();
        }
    }
}

// every hour of the product, three at a time, so the loop can play and the strip shows what is ready
void CamsViewer::prefetch(int generationNow) {
    const auto * product = currentProduct();
    if (product == nullptr || generationNow != prefetchGeneration) {
        return;
    }
    for (const auto seconds : times) {
        if (static_cast<int>(inFlight.size()) >= 3) {
            return;
        }
        if (frames.count(seconds) > 0 || inFlight.count(seconds) > 0) {
            continue;
        }
        inFlight.insert(seconds);
        const auto spec = UtilityCams::layerSpec(model.id, currentRun(), *product, currentSector(), seconds);
        auto bytes = std::make_shared<QByteArray>();
        auto error = std::make_shared<string>();
        new FutureVoid{this,
            [bytes, error, spec] { *bytes = UtilityCams::composite(spec, *error); },
            [this, bytes, error, seconds, generationNow] {
                if (closed || generationNow != prefetchGeneration) {
                    return;
                }
                inFlight.erase(seconds);
                if (!bytes->isEmpty()) {
                    frames[seconds] = *bytes;
                } else {
                    lastFrameError = *error;
                }
                refreshStrip();
                prefetch(generationNow);
            }};
    }
}

void CamsViewer::showFrame() {
    const auto * product = currentProduct();
    const auto timeIndex = strip->current();
    if (product == nullptr || timeIndex < 0 || timeIndex >= static_cast<int>(times.size())) {
        return;
    }
    const auto seconds = times[static_cast<size_t>(timeIndex)];
    const auto run = currentRun();
    const auto spec = UtilityCams::layerSpec(model.id, run, *product, currentSector(), seconds);
    const auto label = UtilityCams::frameLabel(spec);
    setTitle("NSSL CAMs - " + model.name + " - " + product->label + " - " + label);
    labelValid.setText(QString::fromStdString(label + "  valid " + UtilityCams::validLabel(run, seconds)));
    const auto have = frames.find(seconds);
    if (have != frames.end()) {
        notify("");
        shown = have->second;
        image.setBytesKeepView(shown);
        return;
    }
    generation += 1;
    const int thisGeneration = generation;
    auto bytes = std::make_shared<QByteArray>();
    auto error = std::make_shared<string>();
    new FutureVoid{this,
        [bytes, error, spec] { *bytes = UtilityCams::composite(spec, *error); },
        [this, bytes, error, thisGeneration, seconds] {
            if (closed || thisGeneration != generation) return;
            if (bytes->isEmpty()) {
                notify("Could not load this picture: " + *error);
                return;
            }
            notify("");
            frames[seconds] = *bytes;
            shown = *bytes;
            image.setBytesKeepView(shown);
            refreshStrip();
        }};
}

// the picture on view, or all the hours drawn so far as a loop
void CamsViewer::saveFrames() {
    if (shown.isEmpty()) {
        return;
    }
    const auto * product = currentProduct();
    vector<QByteArray> loop;
    for (const auto seconds : times) {
        const auto found = frames.find(seconds);
        if (found != frames.end()) {
            loop.push_back(found->second);
        }
    }
    const auto run = currentRun();
    const QString name = QString::fromStdString(model.id + "_" + run.key() + "_" + (product != nullptr ? product->id : string{"cams"}) + "_" + currentSector());
    UtilityAnimationExport::saveWithDialog(this, loop.size() >= 2 ? loop : vector<QByteArray>{}, strip->intervalMs(), shown, name, QByteArray{}, false, name);
}
