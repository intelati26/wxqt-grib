// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChecksPicker.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QMenu>
#include <QStandardItemModel>
#include <QVBoxLayout>
#include "gfs/ChartAlias.h"
#include "gfs/ChartPreset.h"

namespace {

// the numbers of a label, with the unit that follows each ("500mb", "6 hours")
std::vector<std::string> numbersOf(const std::string& label) {
    std::vector<std::string> out;
    static const std::regex number{R"(\d+(?:\.\d+)?(?:-\d+(?:\.\d+)?)?(?:\s?(?:mb|hours?|hrs?|in|kt|mph|%)|m\b)?)"};
    for (std::sregex_iterator it{label.begin(), label.end(), number}, end; it != end; ++it) {
        out.push_back((*it)[0].str());
    }
    return out;
}

}  // namespace

std::vector<ChecksPicker::Kind> ChecksPicker::kinds(const std::vector<ProductPicker::Entry>& entries) {
    std::vector<Kind> out;
    std::map<std::string, size_t> at;   // the alias group (the id with its numbers as '#') -> its place in out
    for (const auto& e : entries) {
        const auto * alias = ChartAlias::find(e.id);
        const std::string group = alias != nullptr && !ChartAlias::oneOff(*alias) ? std::string{alias->group} : "one:" + e.id;
        const auto found = at.find(e.group + "|" + group);
        if (found == at.end()) {
            at[e.group + "|" + group] = out.size();
            out.push_back({e.label, e.group, {e}, {}});
        } else {
            out[found->second].charts.push_back(e);
        }
    }
    for (auto& k : out) {
        if (k.charts.size() < 2) {
            k.values.assign(k.charts.size(), std::string{});
            continue;
        }
        // the name: the first label with the numbers that differ between the charts written '#'; each value: those numbers
        std::vector<std::vector<std::string>> nums;
        for (const auto& c : k.charts) {
            nums.push_back(numbersOf(c.label));
        }
        const bool same = std::all_of(nums.begin(), nums.end(), [&nums] (const auto& n) { return n.size() == nums.front().size(); });
        std::vector<size_t> varying;
        if (same) {
            for (size_t i = 0; i < nums.front().size(); i++) {
                if (std::any_of(nums.begin(), nums.end(), [&nums, i] (const auto& n) { return n[i] != nums.front()[i]; })) {
                    varying.push_back(i);
                }
            }
        }
        if (!same || varying.empty()) {
            for (const auto& c : k.charts) {
                k.values.push_back(c.label);
            }
            continue;
        }
        {   // in the order of the first number that varies (500, 700, 850; 1, 3, 6 hours), the list's order for the rest
            std::vector<size_t> order(k.charts.size());
            for (size_t c = 0; c < order.size(); c++) {
                order[c] = c;
            }
            const auto first = [&nums, &varying] (size_t c) { return std::atof(nums[c][varying.front()].c_str()); };
            std::stable_sort(order.begin(), order.end(), [&first] (size_t a, size_t b) { return first(a) < first(b); });
            std::vector<ProductPicker::Entry> sorted;
            std::vector<std::vector<std::string>> sortedNums;
            for (const auto c : order) {
                sorted.push_back(k.charts[c]);
                sortedNums.push_back(nums[c]);
            }
            k.charts = sorted;
            nums = sortedNums;
        }
        for (size_t c = 0; c < k.charts.size(); c++) {
            std::string v;
            for (const auto i : varying) {
                v += (v.empty() ? "" : " / ") + nums[c][i];
            }
            k.values.push_back(v);
        }
        std::string name = k.charts.front().label;
        for (const auto i : varying) {
            const auto token = nums.front()[i];
            const auto place = name.find(token);
            if (place != std::string::npos) {
                name.replace(place, token.size(), "#");
            }
        }
        if (name.compare(0, 2, "# ") == 0) {   // ("#mb Wind and Height": the number is the Value row's)
            name.erase(0, 2);
        }
        k.name = name;
    }
    for (auto& k : out) {   // charts whose labels do not differ (the blend's periods are all "Total Precipitation"): the numbers of their ids, and the ids when those do not differ either
        if (k.charts.size() < 2 || std::set<std::string>{k.values.begin(), k.values.end()}.size() == k.values.size()) {
            continue;
        }
        std::vector<std::string> byNumbers;
        for (const auto& c : k.charts) {
            std::string v;
            static const std::regex digits{R"(\d+(?:\.\d+)?)"};
            for (std::sregex_iterator it{c.id.begin(), c.id.end(), digits}, end; it != end; ++it) {
                auto d = (*it)[0].str();
                d.erase(0, std::min(d.find_first_not_of('0'), d.size() - 1));
                v += (v.empty() ? "" : " / ") + d;
            }
            byNumbers.push_back(v);
        }
        k.values = std::set<std::string>{byNumbers.begin(), byNumbers.end()}.size() == byNumbers.size() ? byNumbers : [&k] { std::vector<std::string> ids; for (const auto& c : k.charts) { ids.push_back(c.id); } return ids; }();
    }
    return out;
}

ChecksPicker::ChecksPicker(QWidget * parent, const std::string& model, const std::vector<ProductPicker::Entry>& entries, const std::string& current, const std::vector<GfsChart::OverlayChoice>& choices,
                           const std::vector<std::string>& ticked)
    : QDialog{parent}
    , model{model}
    , kindList{kinds(entries)}
    , choices{choices}
    , chart{current}
{
    setWindowTitle(QString::fromStdString(model) + " charts as checks");
    auto * layout = new QVBoxLayout{this};
    const auto row = [this, layout] (const QString& name, QWidget * body) {
        auto * line = new QHBoxLayout;
        auto * label = new QLabel{name, this};
        label->setMinimumWidth(170);
        line->addWidget(label);
        line->addWidget(body, 1);
        layout->addLayout(line);
        return label;
    };
    presetRow = new QWidget{this};
    new QHBoxLayout{presetRow};
    row("Presets", presetRow);
    kindBox = new QComboBox{this};
    kindBox->setMinimumWidth(300);
    row("Chart", kindBox);
    valueBox = new QComboBox{this};
    valueLabel = row("Value", valueBox);
    auto * rule = new QFrame{this};
    rule->setFrameShape(QFrame::HLine);
    layout->addWidget(rule);
    // the lines and barbs one row to a family: "500mb height", "850mb height" are the chips 500mb and 850mb of the row "height"; what has no level is a chip of its own in the row "other"
    const auto splitLabel = [] (const std::string& label) {
        static const std::regex lead{R"(^((?:\d+-)?\d+\s?mb|\d+\s?m)\s+(.*)$)"};
        std::smatch m;
        return std::regex_match(label, m, lead) ? std::pair<std::string, std::string>{m[2].str(), m[1].str()} : std::pair<std::string, std::string>{"other", label};
    };
    for (const auto& group : {std::string{"Lines"}, std::string{"Wind barbs"}, std::string{"Fill"}}) {
        std::vector<std::string> order;
        std::map<std::string, std::vector<const GfsChart::OverlayChoice *>> rows;
        for (const auto& c : choices) {
            if (c.group != group) {
                continue;
            }
            const auto family = group == "Wind barbs" ? std::string{"barbs"} : splitLabel(c.label).first;
            if (rows.find(family) == rows.end()) {
                order.push_back(family);
            }
            rows[family].push_back(&c);
        }
        for (const auto& family : order) {
            auto * body = new QWidget{this};
            auto * flow = new QHBoxLayout{body};
            flow->setContentsMargins(0, 0, 0, 0);
            for (const auto * c : rows[family]) {
                const auto parts = splitLabel(c->label);
                auto * box = new QCheckBox{QString::fromStdString(group == "Wind barbs" ? (parts.first == "other" ? c->label : parts.second) : (parts.first == "other" ? c->label : parts.second)), body};
                box->setToolTip(QString::fromStdString(c->label));
                box->setProperty("id", QString::fromStdString(c->id));
                box->setChecked(std::find(ticked.begin(), ticked.end(), c->id) != ticked.end());
                QObject::connect(box, &QCheckBox::toggled, this, [this] { ticks(); });
                flow->addWidget(box);
                boxes.push_back(box);
            }
            flow->addStretch(1);
            auto name = family == "barbs" ? std::string{"Barbs"} : family == "other" ? std::string{"Lines"} : family;
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
            if (group == "Fill") {   // (a fill ticked on is drawn over the chart's own, lighter)
                name = "Fill: " + family;
            }
            row(QString::fromStdString(name), body);
        }
    }
    note = new QLabel{this};
    note->setWordWrap(true);
    layout->addWidget(note);
    fillKinds();
    fillPresets();
    QObject::connect(kindBox, &QComboBox::currentIndexChanged, this, [this] (int i) { pickKind(i); });
    QObject::connect(valueBox, &QComboBox::currentIndexChanged, this, [this] (int i) { pickValue(i); });
    resize(560, sizeHint().height());
}

void ChecksPicker::fillKinds() {
    quiet = true;
    kindBox->clear();
    auto * model = new QStandardItemModel{kindBox};
    std::string group;
    int select = -1;
    for (size_t k = 0; k < kindList.size(); k++) {
        if (kindList[k].group != group) {
            group = kindList[k].group;
            auto * head = new QStandardItem{QString::fromStdString(group)};
            head->setFlags(Qt::NoItemFlags);
            QFont f = head->font();
            f.setBold(true);
            head->setFont(f);
            model->appendRow(head);
        }
        auto * item = new QStandardItem{"   " + QString::fromStdString(kindList[k].name)};
        item->setData(static_cast<int>(k), Qt::UserRole);
        model->appendRow(item);
        for (const auto& c : kindList[k].charts) {
            if (c.id == chart) {
                select = model->rowCount() - 1;
            }
        }
    }
    kindBox->setModel(model);
    kindBox->setCurrentIndex(select);
    quiet = false;
    if (select >= 0) {
        fillValues(static_cast<size_t>(kindBox->itemData(select, Qt::UserRole).toInt()), chart);
    }
}

void ChecksPicker::fillValues(size_t kind, const std::string& select) {
    quiet = true;
    valueBox->clear();
    const auto& k = kindList[kind];
    int at = 0;
    for (size_t i = 0; i < k.charts.size(); i++) {
        valueBox->addItem(QString::fromStdString(k.values[i].empty() ? k.charts[i].label : k.values[i]), QString::fromStdString(k.charts[i].id));
        if (k.charts[i].id == select) {
            at = static_cast<int>(i);
        }
    }
    valueBox->setCurrentIndex(at);
    valueBox->setEnabled(k.charts.size() > 1);
    valueLabel->setEnabled(k.charts.size() > 1);
    quiet = false;
}

void ChecksPicker::pickKind(int row) {
    if (quiet || row < 0) {
        return;
    }
    const auto kind = kindBox->itemData(row, Qt::UserRole);
    if (!kind.isValid()) {
        return;
    }
    const auto& k = kindList[static_cast<size_t>(kind.toInt())];
    // the same value in the new kind when it has one (500mb stays 500mb), else its first
    std::string keep;
    const auto now = valueBox->currentText().toStdString();
    for (size_t i = 0; i < k.charts.size(); i++) {
        if (!now.empty() && k.values[i] == now) {
            keep = k.charts[i].id;
        }
    }
    fillValues(static_cast<size_t>(kind.toInt()), keep);
    pickValue(valueBox->currentIndex());
}

void ChecksPicker::pickValue(int row) {
    if (quiet || row < 0) {
        return;
    }
    const auto id = valueBox->itemData(row).toString().toStdString();
    if (id.empty() || id == chart) {
        return;
    }
    chart = id;
    if (onPick) {
        onPick(id);
    }
}

void ChecksPicker::ticks() {
    if (quiet) {
        return;
    }
    std::vector<std::string> ids;
    for (auto * box : boxes) {
        if (box->isChecked()) {
            ids.push_back(box->property("id").toString().toStdString());
        }
    }
    if (onOverlays) {
        onOverlays(ids);
    }
}

void ChecksPicker::setState(const std::string& newChart, const std::vector<std::string>& ticked) {
    quiet = true;
    for (auto * box : boxes) {
        box->setChecked(std::find(ticked.begin(), ticked.end(), box->property("id").toString().toStdString()) != ticked.end());
    }
    quiet = false;
    if (newChart != chart) {
        chart = newChart;
        fillKinds();
    }
}

void ChecksPicker::fillPresets() {
    auto * layout = qobject_cast<QHBoxLayout *>(presetRow->layout());
    while (auto * item = layout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    layout->setContentsMargins(0, 0, 0, 0);
    for (const auto& p : ChartPreset::load()) {
        auto * button = new QPushButton{QString::fromStdString(p.name), presetRow};
        button->setFlat(false);
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        const auto name = p.name;
        QObject::connect(button, &QPushButton::clicked, this, [this, name] { usePreset(name); });
        QObject::connect(button, &QPushButton::customContextMenuRequested, this, [this, button, name] (const QPoint& at) {
            QMenu menu;
            auto * del = menu.addAction("Delete \"" + QString::fromStdString(name) + "\"");
            if (menu.exec(button->mapToGlobal(at)) == del) {
                ChartPreset::erase(name);
                fillPresets();
            }
        });
        layout->addWidget(button);
    }
    auto * starters = new QPushButton{"Starters  \xE2\x96\xBE", presetRow};
    QObject::connect(starters, &QPushButton::clicked, this, [this, starters] {
        QMenu menu;
        for (const auto& p : ChartPreset::starters()) {
            const auto resolved = ChartPreset::resolve(p, model);
            auto * action = menu.addAction(QString::fromStdString(p.name));
            action->setEnabled(resolved.found);
            action->setToolTip(resolved.found ? QString{} : QString::fromStdString(model) + " has no such chart");
            const auto name = p.name;
            QObject::connect(action, &QAction::triggered, this, [this, p] { applyPreset(p); });
        }
        menu.exec(starters->mapToGlobal(QPoint{0, starters->height()}));
    });
    layout->addWidget(starters);
    auto * save = new QPushButton{"Save checks as preset...", presetRow};
    QObject::connect(save, &QPushButton::clicked, this, [this] { savePreset(); });
    layout->addWidget(save);
    layout->addStretch(1);
}

void ChecksPicker::savePreset() {
    bool ok = false;
    const auto name = QInputDialog::getText(this, "Save preset", "Name for these checks (the chart and its lines and barbs):", QLineEdit::Normal, {}, &ok);
    if (!ok) {
        return;
    }
    std::vector<std::string> ids;
    for (auto * box : boxes) {
        if (box->isChecked()) {
            ids.push_back(box->property("id").toString().toStdString());
        }
    }
    ChartPreset::put({ChartPreset::cleanName(name.toStdString()), chart, ids});
    fillPresets();
}

void ChecksPicker::usePreset(const std::string& name) {
    for (const auto& p : ChartPreset::load()) {
        if (p.name == name) {
            applyPreset(p);
            return;
        }
    }
}

void ChecksPicker::applyPreset(const ChartPreset::Preset& p) {
    const auto r = ChartPreset::resolve(p, model);
    if (!r.found) {
        note->setText("\"" + QString::fromStdString(p.name) + "\": " + QString::fromStdString(model) + " has no such chart.");
        return;
    }
    note->setText(r.dropped.empty() ? QString{} : "\"" + QString::fromStdString(p.name) + "\": " + QString::number(r.dropped.size()) + " of its lines or barbs are not offered by " + QString::fromStdString(model) + ".");
    quiet = true;
    for (auto * box : boxes) {
        box->setChecked(std::find(r.selection.overlays.begin(), r.selection.overlays.end(), box->property("id").toString().toStdString()) != r.selection.overlays.end());
    }
    quiet = false;
    chart = r.selection.param;
    fillKinds();
    if (onPick) {
        onPick(chart);
    }
    if (onOverlays) {
        onOverlays(r.selection.overlays);
    }
}
