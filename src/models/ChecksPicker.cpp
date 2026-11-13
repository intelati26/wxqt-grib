// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChecksPicker.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <regex>
#include <set>
#include <tuple>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QIcon>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QWidgetAction>
#include <QVBoxLayout>
#include "gfs/ChartAlias.h"
#include "gfs/ChartPreset.h"
#include "models/ChartPickers.h"
#include "models/PaletteEditor.h"
#include "models/PaletteStore.h"
#include "settings/UIPreferences.h"
#include "util/Utility.h"

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

// a strip of a color scale (the one the user kept for the same default, else the program's)
QPixmap rampSwatch(const GfsChart::Ramp& program) {
    auto ramp = program;
    if (const auto kept = PaletteStore::all(); kept.find(GfsChart::rampKey(program)) != kept.end()) {
        ramp = kept.at(GfsChart::rampKey(program));
    }
    QPixmap pixmap{52, 14};
    pixmap.fill(Qt::transparent);
    if (ramp.stops.size() < 2) {
        return pixmap;
    }
    QPainter painter{&pixmap};
    const double low = ramp.stops.front().first, high = ramp.stops.back().first;
    QLinearGradient gradient{0, 0, 52, 0};
    for (const auto& stop : ramp.stops) {
        auto color = stop.second;
        color.setAlpha(255);
        gradient.setColorAt(high > low ? (stop.first - low) / (high - low) : 0.0, color);
    }
    painter.fillRect(0, 0, 52, 14, gradient);
    painter.setPen(QColor{90, 90, 90});
    painter.drawRect(0, 0, 51, 13);
    return pixmap;
}

// a short stretch of a line as it is drawn
QPixmap lineSwatch(const GfsChart::LineStyle& style) {
    QPixmap pixmap{52, 14};
    pixmap.fill(Qt::transparent);
    QPainter painter{&pixmap};
    QPen pen{style.color};
    pen.setWidthF(std::min(style.width * 1.5, 6.0));
    pen.setStyle(style.dashed ? Qt::DashLine : Qt::SolidLine);
    painter.setPen(pen);
    painter.drawLine(2, 7, 50, 7);
    return pixmap;
}

// the style of a line: its color, weight, dashes, the interval and the numbers; true when it was changed (kept, or taken away by "Standard")
bool editLineStyle(QWidget * parent, const std::string& id) {
    const auto info = GfsChart::lineStyle(id);
    if (!info.available) {
        return false;
    }
    QDialog dialog{parent};
    dialog.setWindowTitle(QString::fromStdString(info.title));
    auto * form = new QFormLayout{&dialog};
    QColor color = info.value.color;
    auto * colorButton = new QPushButton{&dialog};
    const auto paintColor = [&color, colorButton] {
        QPixmap chip{40, 16};
        chip.fill(color);
        colorButton->setIcon(QIcon{chip});
        colorButton->setText(color.name());
    };
    paintColor();
    QObject::connect(colorButton, &QPushButton::clicked, &dialog, [&] {
        const auto chosen = QColorDialog::getColor(color, &dialog, "Line color");
        if (chosen.isValid()) {
            color = chosen;
            paintColor();
        }
    });
    form->addRow("Color", colorButton);
    auto * width = new QDoubleSpinBox{&dialog};
    width->setRange(0.5, 6.0);
    width->setSingleStep(0.25);
    width->setValue(info.value.width);
    form->addRow("Weight", width);
    auto * dashed = new QCheckBox{"Dashed", &dialog};
    dashed->setChecked(info.value.dashed);
    form->addRow("", dashed);
    auto * interval = new QDoubleSpinBox{&dialog};
    interval->setRange(0.1, 1000.0);
    interval->setDecimals(1);
    interval->setValue(info.value.interval > 0.0 ? info.value.interval : 1.0);
    interval->setEnabled(info.standard.interval > 0.0);
    form->addRow("Every", interval);
    auto * labelled = new QCheckBox{"Write the numbers on the line", &dialog};
    labelled->setChecked(info.value.labelled);
    form->addRow("", labelled);
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, &dialog};
    buttons->button(QDialogButtonBox::RestoreDefaults)->setText("Standard");
    form->addRow(buttons);
    bool standard = false;
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, &dialog, [&] {
        standard = true;
        dialog.accept();
    });
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    if (standard) {
        GfsChart::clearLineStyle(id);
    } else {
        GfsChart::setLineStyle(id, {color, width->value(), dashed->isChecked(), info.standard.interval > 0.0 ? interval->value() : info.standard.interval, labelled->isChecked()});
    }
    return true;
}

}  // namespace

std::vector<ChecksPicker::Kind> ChecksPicker::kinds(const std::vector<ProductPicker::Entry>& entries) {
    std::vector<Kind> out;
    std::map<std::string, size_t> at;   // the alias group (the id with its numbers as '#') -> its place in out
    std::map<std::string, std::array<std::string, 3>> partsOf;   // chart id -> hour, type, limit (a product chart)
    for (const auto& e : entries) {
        const auto * alias = ChartAlias::find(e.id);
        std::string group = alias != nullptr && !ChartAlias::oneOff(*alias) ? std::string{alias->group} : "one:" + e.id;
        if (alias != nullptr) {
            const std::string family = alias->family;
            if ((family == "Chance" || family.compare(0, 12, "Product grid") == 0) && *alias->product != '\0') {   // a product of hours, types and limits: one kind for the product
                group = "product:" + std::string{alias->product};
                partsOf[e.id] = {alias->window, alias->kind, alias->limit};
            }
        }
        const auto found = at.find(e.group + "|" + group);
        if (found == at.end()) {
            at[e.group + "|" + group] = out.size();
            out.push_back({e.label, e.group, {e}, {}});
        } else {
            out[found->second].charts.push_back(e);
        }
    }
    for (auto& k : out) {
        if (k.charts.size() >= 2 && std::all_of(k.charts.begin(), k.charts.end(), [&partsOf] (const ProductPicker::Entry& c) { return partsOf.count(c.id) > 0; })) {
            std::set<std::array<std::string, 3>> seen;
            bool unique = true;
            for (const auto& c : k.charts) {
                unique = seen.insert(partsOf[c.id]).second && unique;
            }
            if (unique) {   // told apart by their hour, type and limit: the three lists do it, in the order hours (instant first, then by length), types, limits
                const auto hours = [] (const std::string& w) { return w == "instant" ? -1 : w.empty() ? -2 : std::atoi(w.c_str()); };
                std::stable_sort(k.charts.begin(), k.charts.end(), [&] (const ProductPicker::Entry& a, const ProductPicker::Entry& b) {
                    const auto& pa = partsOf[a.id];
                    const auto& pb = partsOf[b.id];
                    return std::make_tuple(hours(pa[0]), pa[1], std::atof(pa[2].c_str()), pa[2]) < std::make_tuple(hours(pb[0]), pb[1], std::atof(pb[2].c_str()), pb[2]);
                });
                for (const auto& c : k.charts) {
                    k.parts.push_back(partsOf[c.id]);
                    k.values.push_back(partsOf[c.id][0] + " " + partsOf[c.id][1] + " " + partsOf[c.id][2]);
                }
                const auto * alias = ChartAlias::find(k.charts.front().id);
                k.name = alias != nullptr && *alias->product != '\0' ? std::string{alias->product} : k.name;
                continue;
            }
        }
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
    , favorites{ChartPickers::favorites("MODELFAV_" + model)}
{
    for (const auto& e : entries) {
        labelOf[e.id] = e.label;
    }
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
    search = new QLineEdit{this};
    search->setPlaceholderText(QString{"Search %1 charts"}.arg(entries.size()));
    search->setClearButtonEnabled(true);
    row("Search", search);
    auto * chartRow = new QWidget{this};
    auto * chartLine = new QHBoxLayout{chartRow};
    chartLine->setContentsMargins(0, 0, 0, 0);
    kindBox = new QComboBox{chartRow};
    kindBox->setMinimumWidth(300);
    chartLine->addWidget(kindBox, 1);
    star = new QPushButton{chartRow};
    star->setToolTip("Add this chart to the favorites, or take it out");
    star->setMaximumWidth(40);
    chartLine->addWidget(star);
    row("Chart", chartRow);
    valueBox = new QComboBox{this};
    valueLabel = row("Value", valueBox);
    variantBody = new QWidget{this};
    {
        auto * line = new QHBoxLayout{variantBody};
        line->setContentsMargins(0, 0, 0, 0);
        for (size_t a = 0; a < 3; a++) {
            axes[a] = new QComboBox{variantBody};
            axes[a]->setMinimumWidth(a == 1 ? 170 : 110);
            auto * name = new QLabel{a == 0 ? "Hour" : a == 1 ? "Type" : "Limit", variantBody};
            line->addWidget(name);
            line->addWidget(axes[a], 1);
        }
    }
    variantLabel = row("Variant", variantBody);
    variantBody->hide();
    variantLabel->hide();
    auto * rule = new QFrame{this};
    rule->setFrameShape(QFrame::HLine);
    layout->addWidget(rule);
    // the lines, barbs and fills one dropdown to a family: "500mb height", "850mb height" are the levels 500mb and 850mb of the button "Height"; what has no level is a button of its own
    // ("Sea level pressure"). The buttons say how many levels are on, and a line under them says what is on.
    const auto splitLabel = [] (const std::string& label) {
        static const std::regex lead{R"(^((?:\d+-)?\d+\s?mb|\d+\s?m)\s+(.*)$)"};
        std::smatch m;
        return std::regex_match(label, m, lead) ? std::pair<std::string, std::string>{m[2].str(), m[1].str()} : std::pair<std::string, std::string>{"other", label};
    };
    const std::vector<std::pair<std::string, QString>> groups{{"Lines", "Contours"}, {"Wind barbs", "Barbs"}, {"Fill", "Fills (drawn over the chart's own)"}};
    for (const auto& [group, title] : groups) {
        std::vector<std::string> order;
        std::map<std::string, std::vector<const GfsChart::OverlayChoice *>> byFamily;
        for (const auto& c : choices) {
            if (c.group != group) {
                continue;
            }
            const auto family = group == "Wind barbs" ? std::string{"barbs"} : splitLabel(c.label).first;
            if (byFamily.find(family) == byFamily.end()) {
                order.push_back(family);
            }
            byFamily[family].push_back(&c);
        }
        if (order.empty()) {
            continue;
        }
        auto * body = new QWidget{this};
        auto * flow = new QHBoxLayout{body};
        flow->setContentsMargins(0, 0, 0, 0);
        for (const auto& family : order) {
            Family f;
            f.name = family == "barbs" ? "Barbs" : family == "other" ? "Pressure" : QString::fromStdString(family);
            f.name[0] = f.name[0].toUpper();
            f.group = group;
            f.button = new QPushButton{body};
            auto * menu = new QMenu{f.button};
            for (const auto * c : byFamily[family]) {
                const auto parts = splitLabel(c->label);
                auto * box = new QCheckBox{QString::fromStdString(parts.first == "other" ? c->label : parts.second), menu};
                box->setToolTip(QString::fromStdString(c->label));
                box->setProperty("id", QString::fromStdString(c->id));
                box->setChecked(std::find(ticked.begin(), ticked.end(), c->id) != ticked.end());
                QObject::connect(box, &QCheckBox::toggled, this, [this] { ticks(); });
                auto * action = new QWidgetAction{menu};   // (a widget in the menu: ticking it does not close the menu)
                const auto from = group == "Fill" ? GfsChart::fillFrom(c->id) : GfsChart::FillFrom{};
                if (from.available) {   // a fill: and where it starts (clear under it, tinted from it up), so it does not mix with the chart's own fill everywhere
                    auto * line = new QWidget{menu};
                    auto * lineLayout = new QHBoxLayout{line};
                    lineLayout->setContentsMargins(6, 2, 6, 2);
                    lineLayout->addWidget(box, 1);
                    if (const auto scale = GfsChart::fillScale(c->id); scale.available) {   // the fill's color scale: a strip of it, and the Colors editor behind it
                        auto * swatch = new QPushButton{line};
                        swatch->setFixedSize(60, 22);
                        swatch->setIconSize(QSize{52, 14});
                        swatch->setIcon(QIcon{rampSwatch(scale.ramp)});
                        swatch->setToolTip("Colors of this fill (shared with every chart that has the same scale)");
                        QObject::connect(swatch, &QPushButton::clicked, this, [this, scale, swatch, box] {
                            auto * editor = new PaletteEditor{this, scale.title, scale.ramp, scale.quantity, UIPreferences::unitsF, [this, scale, swatch, box] {
                                swatch->setIcon(QIcon{rampSwatch(scale.ramp)});
                                if (box->isChecked() && !quiet) {
                                    ticks();   // (the picture again, in the new colors)
                                }
                            }};
                            editor->setAttribute(Qt::WA_DeleteOnClose);
                        });
                        lineLayout->addWidget(swatch);
                    }
                    auto * spin = new QSpinBox{line};
                    spin->setPrefix("from ");
                    spin->setSuffix(QString::fromStdString(from.unit.empty() ? std::string{} : " " + from.unit));
                    spin->setRange(static_cast<int>(std::floor(from.low)), static_cast<int>(std::ceil(from.high)));
                    spin->setValue(static_cast<int>(std::lround(from.value)));
                    spin->setToolTip("The fill is clear under this value and tinted from it up (the lowest value shows it everywhere)");
                    const auto id = QString::fromStdString(c->id);
                    const int low = spin->minimum();
                    QObject::connect(spin, &QSpinBox::valueChanged, this, [this, id, box, low] (int v) {
                        Utility::writePref("FILLFROM_" + id.toStdString(), v <= low ? "all" : std::to_string(v));
                        if (box->isChecked() && !quiet) {
                            ticks();   // (the picture again, with the new start)
                        }
                    });
                    lineLayout->addWidget(spin);
                    action->setDefaultWidget(line);
                } else if (const auto style = group == "Lines" ? GfsChart::lineStyle(c->id) : GfsChart::LineStyleInfo{}; style.available) {   // a line: how it is drawn (color, weight, dashes, interval, numbers)
                    auto * line = new QWidget{menu};
                    auto * lineLayout = new QHBoxLayout{line};
                    lineLayout->setContentsMargins(6, 2, 6, 2);
                    lineLayout->addWidget(box, 1);
                    auto * swatch = new QPushButton{line};
                    swatch->setFixedSize(60, 22);
                    swatch->setIconSize(QSize{52, 14});
                    swatch->setIcon(QIcon{lineSwatch(style.value)});
                    swatch->setToolTip("How this line is drawn: color, weight, dashes, interval, numbers");
                    const auto id = c->id;
                    QObject::connect(swatch, &QPushButton::clicked, this, [this, id, swatch, box] {
                        if (editLineStyle(this, id)) {
                            swatch->setIcon(QIcon{lineSwatch(GfsChart::lineStyle(id).value)});
                            if (box->isChecked() && !quiet) {
                                ticks();   // (the picture again, drawn the new way)
                            }
                        }
                    });
                    lineLayout->addWidget(swatch);
                    action->setDefaultWidget(line);
                } else {
                    action->setDefaultWidget(box);
                }
                menu->addAction(action);
                f.boxes.push_back(box);
                boxes.push_back(box);
            }
            f.button->setMenu(menu);
            flow->addWidget(f.button);
            families.push_back(f);
        }
        flow->addStretch(1);
        row(title, body);
    }
    summary = new QLabel{this};
    summary->setWordWrap(true);
    summary->setStyleSheet("color: palette(mid);");
    layout->addWidget(summary);
    note = new QLabel{this};
    note->setWordWrap(true);
    layout->addWidget(note);
    refreshFamilies();
    fillKinds();
    fillPresets();
    QObject::connect(kindBox, &QComboBox::currentIndexChanged, this, [this] (int i) { pickKind(i); });
    QObject::connect(search, &QLineEdit::textChanged, this, [this] { fillKinds(); });
    QObject::connect(search, &QLineEdit::returnPressed, this, [this] { kindBox->showPopup(); });
    QObject::connect(star, &QPushButton::clicked, this, [this] { toggleFavorite(); });
    QObject::connect(valueBox, &QComboBox::currentIndexChanged, this, [this] (int i) { pickValue(i); });
    for (int a = 0; a < 3; a++) {
        QObject::connect(axes[static_cast<size_t>(a)], &QComboBox::currentIndexChanged, this, [this, a] (int) { pickAxis(a); });
    }
    resize(560, sizeHint().height());
}

void ChecksPicker::fillKinds() {
    quiet = true;
    kindBox->clear();
    auto * listModel = new QStandardItemModel{kindBox};
    const auto heading = [&listModel] (const std::string& text) {
        auto * head = new QStandardItem{QString::fromStdString(text)};
        head->setFlags(Qt::NoItemFlags);
        QFont f = head->font();
        f.setBold(true);
        head->setFont(f);
        listModel->appendRow(head);
    };
    const auto words = search->text().trimmed().toLower().split(' ', Qt::SkipEmptyParts);
    const auto matches = [&words] (const std::string& text) {
        const auto lower = QString::fromStdString(text).toLower();
        return std::all_of(words.begin(), words.end(), [&lower] (const QString& w) { return lower.contains(w); });
    };
    // the favorites first (a chart each, by its own label)
    bool headed = false;
    for (const auto& id : favorites) {
        const auto found = labelOf.find(id);
        if (found == labelOf.end() || (!words.isEmpty() && !matches(found->second + " " + id))) {
            continue;
        }
        if (!headed) {
            headed = true;
            heading("Favorites");
        }
        auto * item = new QStandardItem{QString::fromUtf8("   \xE2\x98\x85 ") + QString::fromStdString(found->second)};
        item->setData(QString::fromStdString(id), Qt::UserRole + 1);
        listModel->appendRow(item);
    }
    std::string group;
    int select = -1;
    for (size_t k = 0; k < kindList.size(); k++) {
        const auto& kind = kindList[k];
        const bool onShow = std::any_of(kind.charts.begin(), kind.charts.end(), [this] (const ProductPicker::Entry& c) { return c.id == chart; });
        bool found = words.isEmpty() || onShow || matches(kind.name + " " + kind.group);
        for (size_t i = 0; i < kind.charts.size() && !found; i++) {
            found = matches(kind.charts[i].label + " " + kind.charts[i].id + " " + kind.values[i]);
        }
        if (!found) {
            continue;
        }
        if (kind.group != group) {
            group = kind.group;
            heading(group);
        }
        auto * item = new QStandardItem{"   " + QString::fromStdString(kind.name)};
        item->setData(static_cast<int>(k), Qt::UserRole);
        listModel->appendRow(item);
        if (onShow) {
            select = listModel->rowCount() - 1;
        }
    }
    kindBox->setModel(listModel);
    kindBox->setCurrentIndex(select);
    star->setText(std::find(favorites.begin(), favorites.end(), chart) != favorites.end() ? QString::fromUtf8("\xE2\x98\x85") : QString::fromUtf8("\xE2\x98\x86"));
    quiet = false;
    if (select >= 0) {
        fillValues(static_cast<size_t>(kindBox->itemData(select, Qt::UserRole).toInt()), chart);
    }
}

void ChecksPicker::toggleFavorite() {
    const auto found = std::find(favorites.begin(), favorites.end(), chart);
    if (found != favorites.end()) {
        favorites.erase(found);
    } else {
        favorites.push_back(chart);
    }
    ChartPickers::saveFavorites("MODELFAV_" + model, favorites);
    fillKinds();
}

void ChecksPicker::fillValues(size_t kind, const std::string& select) {
    const auto& k = kindList[kind];
    shownKind = kind;
    const bool product = !k.parts.empty();
    valueBox->setVisible(!product);
    valueLabel->setVisible(!product);
    variantBody->setVisible(product);
    variantLabel->setVisible(product);
    if (product) {
        fillAxes(kind, select);
        return;
    }
    quiet = true;
    valueBox->clear();
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

void ChecksPicker::fillAxes(size_t kind, const std::string& chartId) {
    const auto& k = kindList[kind];
    size_t at = 0;
    for (size_t i = 0; i < k.charts.size(); i++) {
        if (k.charts[i].id == chartId) {
            at = i;
        }
    }
    const auto& now = k.parts[at];
    quiet = true;
    const auto fill = [] (QComboBox * box, const std::vector<std::string>& values, const std::string& select) {
        box->clear();
        int current = 0;
        for (const auto& v : values) {
            box->addItem(QString::fromStdString(v.empty() ? std::string{"-"} : v), QString::fromStdString(v));
            if (v == select) {
                current = box->count() - 1;
            }
        }
        box->setCurrentIndex(current);
        box->setEnabled(values.size() > 1);
    };
    std::vector<std::string> hours, types, limits;
    const auto add = [] (std::vector<std::string>& list, const std::string& v) {
        if (std::find(list.begin(), list.end(), v) == list.end()) {
            list.push_back(v);
        }
    };
    for (const auto& p : k.parts) {
        add(hours, p[0]);
    }
    for (const auto& p : k.parts) {
        if (p[0] == now[0]) {
            add(types, p[1]);
        }
    }
    for (const auto& p : k.parts) {
        if (p[0] == now[0] && p[1] == now[1]) {
            add(limits, p[2]);
        }
    }
    fill(axes[0], hours, now[0]);
    fill(axes[1], types, now[1]);
    fill(axes[2], limits, now[2]);
    quiet = false;
}

void ChecksPicker::pickAxis(int axis) {
    if (quiet || kindList.empty() || shownKind >= kindList.size() || kindList[shownKind].parts.empty()) {
        return;
    }
    const auto& k = kindList[shownKind];
    std::array<std::string, 3> want;
    for (size_t a = 0; a < 3; a++) {
        want[a] = axes[a]->currentData().toString().toStdString();
    }
    // the chart that has what was chosen, and as much of the rest as it can keep (a later list is made from the earlier ones, so the earlier choice wins)
    int best = -1, bestScore = -1;
    for (size_t i = 0; i < k.parts.size(); i++) {
        int score = 0;
        bool fits = true;
        for (int a = 0; a <= axis; a++) {
            fits = fits && k.parts[i][static_cast<size_t>(a)] == want[static_cast<size_t>(a)];
        }
        if (!fits) {
            continue;
        }
        for (int a = axis + 1; a < 3; a++) {
            score = score * 2 + (k.parts[i][static_cast<size_t>(a)] == want[static_cast<size_t>(a)] ? 1 : 0);
        }
        if (score > bestScore) {
            bestScore = score;
            best = static_cast<int>(i);
        }
    }
    if (best < 0) {
        return;
    }
    const auto id = k.charts[static_cast<size_t>(best)].id;
    fillAxes(shownKind, id);
    if (id != chart) {
        chart = id;
        if (onPick) {
            onPick(id);
        }
    }
}

void ChecksPicker::pickKind(int row) {
    if (quiet || row < 0) {
        return;
    }
    if (const auto favorite = kindBox->itemData(row, Qt::UserRole + 1); favorite.isValid()) {   // a favorite: that chart, in its own kind
        const auto id = favorite.toString().toStdString();
        if (id != chart) {
            chart = id;
            if (onPick) {
                onPick(id);
            }
        }
        fillKinds();
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
    if (!k.parts.empty()) {   // a product: the chart with the hour and type now chosen when it has them, else its first
        std::string first = k.charts.front().id;
        const auto hour = axes[0]->currentData().toString().toStdString();
        const auto type = axes[1]->currentData().toString().toStdString();
        for (size_t i = 0; i < k.charts.size(); i++) {
            if (k.parts[i][0] == hour && k.parts[i][1] == type) {
                first = k.charts[i].id;
                break;
            }
        }
        fillValues(static_cast<size_t>(kind.toInt()), first);
        if (first != chart) {
            chart = first;
            if (onPick) {
                onPick(first);
            }
        }
        return;
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

void ChecksPicker::refreshFamilies() {
    QStringList on;
    for (const auto& f : families) {
        QStringList levels;
        for (auto * box : f.boxes) {
            if (box->isChecked()) {
                levels << box->text();
            }
        }
        f.button->setText(f.name + (levels.isEmpty() ? QString{} : "  (" + QString::number(levels.size()) + ")"));
        f.button->setStyleSheet(levels.isEmpty() ? QString{} : "font-weight: bold;");
        if (!levels.isEmpty()) {
            on << f.name + (f.group == "Fill" ? " fill" : "") + " " + (f.name == "Pressure" && levels.size() == 1 ? QString{} : levels.join(", "));
        }
    }
    summary->setText(on.isEmpty() ? QString{"Ticked on top of the chart: nothing."} : "Ticked on top of the chart: " + on.join("  |  "));
}

void ChecksPicker::ticks() {
    refreshFamilies();
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
    refreshFamilies();
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
    refreshFamilies();
    chart = r.selection.param;
    fillKinds();
    if (onPick) {
        onPick(chart);
    }
    if (onOverlays) {
        onOverlays(r.selection.overlays);
    }
}
