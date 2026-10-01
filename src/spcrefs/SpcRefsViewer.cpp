// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "spcrefs/SpcRefsViewer.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <QBuffer>
#include <QImage>
#include <QPainter>
#include <QTimeZone>
#include "models/UtilityGrib.h"
#include "objects/FutureVoid.h"
#include "objects/UtilityAnimationExport.h"
#include "spcrefs/LambertGrid.h"
#include "util/Utility.h"

namespace {
    const char * lastProductPref = "SPCREFS_LAST_PRODUCT";
    constexpr size_t maxLoopFrames = 24;

    QString timeLabel(const QDateTime& init, const QDateTime& valid) {
        const auto hour = static_cast<int>(std::llround(init.secsTo(valid) / 3600.0));
        const auto local = valid.toLocalTime();
        return QString{"F%1  %2Z  (%3 %4)"}
            .arg(hour, 2, 10, QChar{'0'})
            .arg(valid.toUTC().toString("ddd MMM d HH:mm"))
            .arg(local.toString("h AP"), QTimeZone::systemTimeZone().abbreviation(local));
    }

    struct Loaded {
        vector<QDateTime> cycles;
        std::shared_ptr<ZarrStore> store;
        vector<string> members;
        vector<double> times;
        vector<float> values;
        QByteArray png;
        string error;
    };
}

SpcRefsViewer::SpcRefsViewer(Window * parent)
    : Window{parent}
    , comboProduct{this, {"Loading products..."}}
    , comboCycle{this, {"Finding cycles..."}}
    , comboTime{this, {"--"}}
    , comboMember{this, {"--"}}
    , backForward{this, [this] { moveBack(); }, [this] { moveForward(); }}
    , textStatus{this, ""}
    , animBar{this,
        [this] (int start, int end) { onRangeRequested(start, end); },
        [this] (int local) { onFrameShown(local); },
        [this] (int global) { onScrub(global); },
        [this] { onSave(); }}
    , image{this}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setTitle("SPC REFS");
    comboProduct.getView()->setToolTip("SPC's REFS ensemble products, read from SPC's own data");
    comboCycle.getView()->setToolTip("The forecast cycle (UTC)");
    comboMember.getView()->setToolTip("Which ensemble member (for single-member fields)");
    comboProduct.getView()->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    comboProduct.getView()->setMinimumContentsLength(30);
    comboProduct.getView()->setMaxVisibleItems(30);
    comboMember.getView()->hide();
    textStatus.setWordWrap(false);
    QObject::connect(&image, &ZoomImage::hovered, this, [this] (double fx, double fy) { onHover(fx, fy); });
    QObject::connect(&image, &ZoomImage::hoverEnded, this, [this] { hoverText.clear(); updateStatus(); });
    comboProduct.connect([this] { userPickedProduct = true; productChanged(); });
    comboCycle.connect([this] { openCycle(); });
    comboTime.connect([this] { animBar.stopIfAnimating(); showTime(); });
    comboMember.connect([this] { animBar.stopIfAnimating(); animGeneration += 1; animBar.clearFrames(); showTime(); });
    rowTop.addWidget(comboProduct);
    rowTop.addWidget(comboCycle);
    rowTop.addLayout(backForward);
    rowTop.addWidget(comboTime);
    rowTop.addWidget(comboMember);
    rowTop.addWidget(textStatus);
    rowTop.addStretch();
    box.addLayout(rowTop);
    animBar.addTo(box);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    say("Looking for SPC REFS cycles...");
    auto result = std::make_shared<Loaded>();
    new FutureVoid{this,
        [result] { result->cycles = UtilitySpcRefs::findCycles(8); },
        [this, result] {
            if (closed) {
                return;
            }
            if (result->cycles.empty()) {
                say("No SPC REFS cycle was found on SPC's server (spc.noaa.gov/exper/refs)");
                return;
            }
            cycles = result->cycles;
            vector<string> labels;
            for (const auto& c : cycles) {
                labels.push_back(c.toString("ddd yyyy-MM-dd HH").toStdString() + "Z");
            }
            comboCycle.block();
            comboCycle.setList(labels);
            comboCycle.setIndex(0);
            comboCycle.unblock();
            openCycle();
        }};
}

const UtilitySpcRefs::Product& SpcRefsViewer::product() const {
    static const UtilitySpcRefs::Product none;
    const auto index = comboProduct.getIndex();
    return index >= 0 && index < static_cast<int>(products.size()) ? products[static_cast<size_t>(index)] : none;
}

int SpcRefsViewer::memberIndex() const {
    return std::max(0, comboMember.getIndex());
}

void SpcRefsViewer::say(const string& text) {
    textStatus.setText(QString::fromStdString(text));
}

void SpcRefsViewer::closeEventCustom() {
    closed = true;
    animBar.stopIfAnimating();
}

void SpcRefsViewer::updateStatus() {
    if (!hoverText.isEmpty()) {
        say(hoverText.toStdString());
        return;
    }
    const auto index = comboTime.getIndex();
    if (index >= 0 && index < static_cast<int>(validTimes.size())) {
        say(product().label + "   " + timeLabel(initTime, validTimes[static_cast<size_t>(index)]).toStdString());
    }
}

// a cycle was chosen: open its store (metadata, member names) and rebuild the product list from what it holds
void SpcRefsViewer::openCycle() {
    const int index = comboCycle.getIndex();
    if (index < 0 || index >= static_cast<int>(cycles.size())) {
        return;
    }
    animBar.stopIfAnimating();
    const auto gen = ++generation;
    animGeneration += 1;
    initTime = cycles[static_cast<size_t>(index)];
    say("Opening the " + initTime.toString("HH").toStdString() + "Z cycle...");
    auto result = std::make_shared<Loaded>();
    const auto url = UtilitySpcRefs::cycleUrl(initTime);
    new FutureVoid{this,
        [result, url] {
            auto opened = std::make_shared<ZarrStore>();
            if (!opened->open(url, result->error)) {
                return;
            }
            result->store = opened;
            string ignored;
            opened->readStrings("member", result->members, ignored);
        },
        [this, result, gen] {
            if (closed || gen != generation) {
                return;
            }
            if (!result->error.empty() || result->store == nullptr) {
                say(result->error.empty() ? "Could not open the cycle" : result->error);
                return;
            }
            store = result->store;
            memberNames = result->members;
            vector<string> names;
            for (const auto& raw : memberNames) {
                names.push_back(UtilitySpcRefs::memberDisplayName(raw));
            }
            comboMember.block();
            comboMember.setList(names.empty() ? vector<string>{"--"} : names);
            comboMember.setIndex(0);
            comboMember.unblock();
            // keep the product shown (if this cycle has it), else the last one used, else the first
            const auto wanted = userPickedProduct && comboProduct.getIndex() >= 0 ? product().id : Utility::readPref(lastProductPref, "uh25_04h_max_overlap_max");
            rebuildProducts(wanted);
            productChanged();
        }};
}

void SpcRefsViewer::rebuildProducts(const string& selectId) {
    products = UtilitySpcRefs::catalog(*store);
    vector<string> labels;
    size_t selected = 0;
    for (size_t i = 0; i < products.size(); i += 1) {
        labels.push_back(products[i].group + " - " + products[i].label);
        if (products[i].id == selectId) {
            selected = i;
        }
    }
    comboProduct.block();
    comboProduct.setList(labels.empty() ? vector<string>{"(no products)"} : labels);
    comboProduct.setIndex(selected);
    comboProduct.unblock();
}

void SpcRefsViewer::productChanged() {
    if (store == nullptr || products.empty()) {
        return;
    }
    animBar.stopIfAnimating();
    Utility::writePref(lastProductPref, product().id);
    if (product().members) {
        comboMember.getView()->show();
    } else {
        comboMember.getView()->hide();
    }
    loadTimes();
}

// the product's time coordinate (valid times), then the picture for the chosen time
void SpcRefsViewer::loadTimes() {
    const auto gen = ++generation;
    animGeneration += 1;
    const auto dimension = product().timeDimension;
    auto result = std::make_shared<Loaded>();
    auto s = store;
    say("Loading " + product().label + "...");
    new FutureVoid{this,
        [result, s, dimension] {
            if (!s->readNumbers(dimension, result->times, result->error)) {
                return;
            }
        },
        [this, result, gen] {
            if (closed || gen != generation) {
                return;
            }
            if (!result->error.empty()) {
                say(result->error);
                return;
            }
            validTimes.clear();
            vector<string> labels;
            for (const double seconds : result->times) {
                validTimes.push_back(QDateTime::fromSecsSinceEpoch(static_cast<qint64>(seconds), QTimeZone::utc()));
                labels.push_back(timeLabel(initTime, validTimes.back()).toStdString());
            }
            // start on the first time that has not passed yet (else the first)
            size_t pick = 0;
            const auto now = QDateTime::currentDateTimeUtc().addSecs(-1800);
            for (size_t i = 0; i < validTimes.size(); i += 1) {
                if (validTimes[i] >= now) {
                    pick = i;
                    break;
                }
            }
            comboTime.block();
            comboTime.setList(labels.empty() ? vector<string>{"--"} : labels);
            comboTime.setIndex(pick);
            comboTime.unblock();
            animBar.setAvailableLabels(labels);
            animBar.setSliderPosition(static_cast<int>(pick));
            frameInfos.clear();
            showTime();
        }};
}

void SpcRefsViewer::showTime() {
    const int timeIndex = comboTime.getIndex();
    if (store == nullptr || timeIndex < 0 || timeIndex >= static_cast<int>(validTimes.size())) {
        return;
    }
    const auto gen = ++generation;
    const auto p = product();
    auto s = store;
    const auto members = memberNames;
    const int member = memberIndex();
    auto result = std::make_shared<Loaded>();
    say("Loading " + p.label + "  " + timeLabel(initTime, validTimes[static_cast<size_t>(timeIndex)]).toStdString() + "...");
    new FutureVoid{this,
        [result, s, p, members, member, timeIndex] {
            vector<int> chunk = p.members ? vector<int>{member, timeIndex, 0, 0} : vector<int>{timeIndex, 0, 0};
            if (!s->readChunk(p.id, chunk, result->values, result->error)) {
                return;
            }
            result->png = UtilitySpcRefs::renderPng(p, result->values, members);
        },
        [this, result, gen] {
            if (closed || gen != generation) {
                return;
            }
            if (!result->error.empty()) {
                say(result->error);
                return;
            }
            values = result->values;
            shownPng = result->png;
            image.setBytesKeepView(shownPng);
            hoverText.clear();
            animBar.setSliderPosition(comboTime.getIndex());
            updateStatus();
            setTitle("SPC REFS - " + product().label + " - " + timeLabel(initTime, validTimes[static_cast<size_t>(comboTime.getIndex())]).toStdString());
        }};
}

void SpcRefsViewer::moveBack() {
    const int i = comboTime.getIndex();
    if (i > 0) {
        comboTime.setIndex(static_cast<size_t>(i - 1));   // its signal shows the time
    }
}

void SpcRefsViewer::moveForward() {
    const int i = comboTime.getIndex();
    if (i + 1 < static_cast<int>(validTimes.size())) {
        comboTime.setIndex(static_cast<size_t>(i + 1));
    }
}

void SpcRefsViewer::onHover(double fx, double fy) {
    if (values.empty()) {
        return;
    }
    const int column = static_cast<int>(std::floor(fx * LambertGrid::columns));
    const int row = LambertGrid::rows - 1 - static_cast<int>(std::floor(fy * LambertGrid::rows));
    const auto text = UtilitySpcRefs::readout(product(), values, column, row, memberNames);
    hoverText = QString::fromStdString(text);
    updateStatus();
}

void SpcRefsViewer::onScrub(int globalIndex) {
    comboTime.block();
    comboTime.setIndex(static_cast<size_t>(globalIndex));
    comboTime.unblock();
    showTime();
}

void SpcRefsViewer::onFrameShown(int localIndex) {
    const auto bytes = animBar.frameAt(localIndex);
    if (bytes.isEmpty()) {
        return;
    }
    shownPng = bytes;
    image.setBytesKeepView(bytes);
    const auto globalIndex = animBar.globalIndexAt(localIndex);
    if (globalIndex >= 0) {
        comboTime.block();
        comboTime.setIndex(static_cast<size_t>(globalIndex));
        comboTime.unblock();
        updateStatus();
    }
}

// Play / Save over a range of times: each one is downloaded and rendered, one after another
void SpcRefsViewer::onRangeRequested(int rangeStart, int rangeEnd) {
    vector<int> indices;
    for (int i = rangeStart; i <= rangeEnd && i < static_cast<int>(validTimes.size()); i += 1) {
        indices.push_back(i);
    }
    if (indices.size() > maxLoopFrames) {
        vector<int> sub;
        for (size_t i = 0; i < maxLoopFrames; i += 1) {
            const auto idx = indices[i * (indices.size() - 1) / (maxLoopFrames - 1)];
            if (sub.empty() || sub.back() != idx) {
                sub.push_back(idx);
            }
        }
        indices = sub;
    }
    if (indices.size() < 2) {
        animBar.cancelPending();
        return;
    }
    const int gen = ++animGeneration;
    struct Sweep {
        vector<int> indices;
        vector<QByteArray> frames;
        vector<FrameInfo> infos;
        string error;
    };
    auto sweep = std::make_shared<Sweep>();
    sweep->indices = indices;
    const auto p = product();
    auto s = store;
    const auto members = memberNames;
    const int member = memberIndex();
    const auto times = validTimes;
    const auto init = initTime;
    auto step = std::make_shared<std::function<void(size_t)>>();
    *step = [this, gen, sweep, p, s, members, member, times, init, step] (size_t at) {
        if (closed || gen != animGeneration) {
            return;
        }
        if (at >= sweep->indices.size()) {
            if (sweep->frames.size() < 2) {
                animBar.cancelPending();
                say(sweep->error.empty() ? "The loop could not be loaded" : sweep->error);
                return;
            }
            frameInfos = sweep->infos;
            vector<int> loadedIndices;
            for (const auto& info : sweep->infos) {
                loadedIndices.push_back(info.timeIndex);
            }
            animBar.setFrames(sweep->frames, loadedIndices);
            return;
        }
        say("Loading loop frame " + std::to_string(at + 1) + " of " + std::to_string(sweep->indices.size()) + "...");
        const int timeIndex = sweep->indices[at];
        auto one = std::make_shared<Loaded>();
        new FutureVoid{this,
            [one, s, p, members, member, timeIndex] {
                vector<int> chunk = p.members ? vector<int>{member, timeIndex, 0, 0} : vector<int>{timeIndex, 0, 0};
                if (s->readChunk(p.id, chunk, one->values, one->error)) {
                    one->png = UtilitySpcRefs::renderPng(p, one->values, members);
                }
            },
            [this, gen, sweep, one, timeIndex, times, init, step, at] {
                if (closed || gen != animGeneration) {
                    return;
                }
                if (one->error.empty() && !one->png.isEmpty()) {
                    sweep->frames.push_back(one->png);
                    FrameInfo info;
                    info.timeIndex = timeIndex;
                    info.valid = times[static_cast<size_t>(timeIndex)];
                    info.hour = static_cast<int>(std::llround(init.secsTo(info.valid) / 3600.0));
                    sweep->infos.push_back(info);
                } else if (sweep->error.empty()) {
                    sweep->error = one->error;
                }
                (*step)(at + 1);
            }};
    };
    (*step)(0);
}

// the information bars of the other model viewers: product and where it comes from on top; run, hour and valid time below
QByteArray SpcRefsViewer::withBorder(const QByteArray& png, const FrameInfo& info) const {
    QImage map = QImage::fromData(png);
    if (map.isNull()) {
        return png;
    }
    const int bar = UtilityGrib::headerBarHeight(map.width());
    QImage out{map.width(), map.height() + 2 * bar, QImage::Format_ARGB32_Premultiplied};
    out.fill(Qt::white);
    {
        QPainter painter{&out};
        painter.drawImage(0, bar, map);
    }
    UtilityGrib::MapHeader header;
    header.topLeft = "SPC REFS  " + QString::fromStdString(product().label);
    header.topRight = "CONUS";
    const auto local = info.valid.toLocalTime();
    header.bottomLeft = "Run " + initTime.toUTC().toString("yyyy-MM-dd HH") + "Z   F" + QString::number(info.hour).rightJustified(2, '0');
    header.bottomRight = "Valid " + info.valid.toUTC().toString("ddd yyyy-MM-dd HH:mm") + "Z  (" + local.toString("ddd h:mm AP") + " " +
        QTimeZone::systemTimeZone().abbreviation(local) + ")";
    UtilityGrib::drawMapHeader(out, header);
    QByteArray bytes;
    QBuffer buffer{&bytes};
    buffer.open(QIODevice::WriteOnly);
    out.save(&buffer, "PNG");
    return bytes;
}

void SpcRefsViewer::onSave() {
    if (shownPng.isEmpty() || validTimes.empty()) {
        return;
    }
    const bool animated = animBar.frameCount() >= 2 && frameInfos.size() == static_cast<size_t>(animBar.frameCount());
    vector<QByteArray> frames;
    QByteArray still;
    FrameInfo first;
    FrameInfo last;
    if (animated) {
        for (size_t i = 0; i < frameInfos.size(); i += 1) {
            frames.push_back(withBorder(animBar.frameAt(static_cast<int>(i)), frameInfos[i]));
        }
        first = frameInfos.front();
        last = frameInfos.back();
    } else {
        const int timeIndex = std::clamp(comboTime.getIndex(), 0, static_cast<int>(validTimes.size()) - 1);
        first.timeIndex = timeIndex;
        first.valid = validTimes[static_cast<size_t>(timeIndex)];
        first.hour = static_cast<int>(std::llround(initTime.secsTo(first.valid) / 3600.0));
        last = first;
        still = withBorder(shownPng, first);
    }
    QString name = initTime.toUTC().toString("yyyyMMdd_HH") + "z_f" + QString::number(first.hour).rightJustified(3, '0');
    if (animated) {
        name += "-f" + QString::number(last.hour).rightJustified(3, '0');
    }
    name += "_v" + first.valid.toUTC().toString("yyyyMMdd_HHmm") + "Z_spcrefs_" + UtilityAnimationExport::slug(QString::fromStdString(product().id), 50);
    UtilityAnimationExport::saveWithDialog(this, frames, 500, still, name, QByteArray{}, false);
}
