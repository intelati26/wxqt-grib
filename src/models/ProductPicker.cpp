// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ProductPicker.h"
#include <QRegularExpression>
#include "models/PrecipitationPicker.h"
#include "hurricane/HafsIntensity.h"
#include "ui/UiStandards.h"
#include <algorithm>
#include <map>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
    constexpr int idRole = Qt::UserRole;
}

std::vector<std::string> ProductPicker::groupOrder() {
    return {"Upper air", "Surface", "Precipitation and moisture", "Storms and severe", "Aviation and visibility", "Tropical and dynamics", "Ensemble spread", "Changes and trends", "Anomalies", "Satellite",
            "Ocean and waves", "Swaths"};
}

ProductPicker::ProductPicker(QWidget * parent, const std::string& model, const std::vector<Entry>& entries, const std::string& current, const std::vector<std::string>& favorites,
                             const std::vector<GfsChart::OverlayChoice>& extras, const std::vector<std::string>& ticked)
    : QDialog{parent, Qt::Tool}
    , entries{entries}
    , favorites{favorites}
    , extras{extras}
    , current{current}
    , title{model}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString::fromStdString(model) + " charts");
    resize(430, 640);
    auto * column = new QVBoxLayout{this};
    search = new QLineEdit{this};
    search->setPlaceholderText(QString{"Search %1 charts"}.arg(entries.size()));
    search->setClearButtonEnabled(true);
    column->addWidget(search);
    tree = new QTreeWidget{this};
    tree->setHeaderHidden(true);
    tree->setRootIsDecorated(true);
    tree->setUniformRowHeights(true);
    tree->setMinimumHeight(340);
    column->addWidget(tree, 1);
    auto * row = new QHBoxLayout;
    star = new QPushButton{this};
    row->addWidget(star);
    row->addStretch();
    choose = UiStandards::button(this, "Show this chart");
    row->addWidget(choose);
    column->addLayout(row);
    if (!extras.empty()) {   // the lines and barbs that go on top of any chart: a section that opens, so the list keeps its room
        bool anyTicked = false;
        auto * toggle = new QPushButton{this};
        toggle->setFlat(true);
        toggle->setStyleSheet("text-align: left; font-weight: bold;");
        column->addWidget(toggle);
        auto * group = new QWidget{this};
        auto * inner = new QGridLayout{group};
        inner->setContentsMargins(8, 0, 0, 0);
        int row2 = 0;
        std::string last;
        int column2 = 0;
        for (const auto& extra : extras) {
            if (extra.group != last) {
                if (column2 != 0) {
                    row2++;
                    column2 = 0;
                }
                inner->addWidget(new QLabel{"<b>" + QString::fromStdString(extra.group) + "</b>", group}, row2++, 0, 1, 2);
                last = extra.group;
            }
            auto * box = new QCheckBox{QString::fromStdString(extra.label), group};
            const bool on = std::find(ticked.begin(), ticked.end(), extra.id) != ticked.end();
            box->setChecked(on);
            anyTicked = anyTicked || on;
            boxes.push_back(box);
            inner->addWidget(box, row2, column2);
            if (++column2 == 2) {
                column2 = 0;
                row2++;
            }
            QObject::connect(box, &QCheckBox::clicked, [this, box] {
                // one set of wind barbs at a time: ticking one clears the others
                for (size_t i = 0; i < boxes.size(); i++) {
                    if (boxes[i] == box && box->isChecked() && this->extras[i].group.find("barb") != std::string::npos) {
                        for (size_t k = 0; k < boxes.size(); k++) {
                            if (boxes[k] != box && this->extras[k].group.find("barb") != std::string::npos) {
                                boxes[k]->setChecked(false);
                            }
                        }
                    }
                }
                this->ticked();
            });
        }
        column->addWidget(group);
        const auto show = [toggle, group] (bool open) {
            group->setVisible(open);
            toggle->setText(open ? QString::fromUtf8("\u25BE Add lines and wind barbs to the chart") : QString::fromUtf8("\u25B8 Add lines and wind barbs to the chart"));
        };
        show(anyTicked);
        QObject::connect(toggle, &QPushButton::clicked, [group, show] { show(!group->isVisible()); });
    }
    {   // the precipitation charts as parts (what, period, lines) instead of the list of them
        std::vector<std::string> ids, labels;
        for (const auto& e : this->entries) {
            ids.push_back(e.id);
            labels.push_back(e.label);
        }
        if (PrecipitationPicker::offeredFor(ids)) {
            auto * parts = UiStandards::button(this, "Precip by parts...", "Precipitation by parts");
            parts->setToolTip("Choose the precipitation chart by what it shows (amount over a period, since the start, type, rate, snowfall), the period and the lines, instead of from the list");
            column->addWidget(parts);
            QObject::connect(parts, &QPushButton::clicked, [this, parts, ids, labels] {
                auto * picker = new PrecipitationPicker{this, ids, labels, this->current};
                picker->setAttribute(Qt::WA_DeleteOnClose);
                picker->onPick = [this, picker] (const std::string& id) {
                    this->current = id;
                    if (this->onPick) {
                        this->onPick(id);
                    }
                    picker->close();
                };
                picker->show();
                (void) parts;
            });
        }
    }
    fill();
    QObject::connect(search, &QLineEdit::textChanged, [this] (const QString& text) { filter(text); });
    QObject::connect(search, &QLineEdit::returnPressed, [this] { pickCurrent(); });
    QObject::connect(tree, &QTreeWidget::itemDoubleClicked, [this] (QTreeWidgetItem *, int) { pickCurrent(); });
    QObject::connect(tree, &QTreeWidget::currentItemChanged, [this] (QTreeWidgetItem *, QTreeWidgetItem *) { updateStar(); });
    QObject::connect(choose, &QPushButton::clicked, [this] { pickCurrent(); });
    QObject::connect(star, &QPushButton::clicked, [this] { toggleFavorite(); });
    search->setFocus();
}

void ProductPicker::setWording(const QString& plural, const QString& chooseText) {
    setWindowTitle(QString::fromStdString(title) + " " + plural);
    search->setPlaceholderText(QString{"Search %1 %2"}.arg(entries.size()).arg(plural));
    choose->setText(chooseText);
}

void ProductPicker::fill() {
    tree->clear();
    const auto isFavorite = [this] (const std::string& id) { return std::find(favorites.begin(), favorites.end(), id) != favorites.end(); };
    const auto add = [this, &isFavorite] (QTreeWidgetItem * parent, const Entry& e) {
        auto * item = new QTreeWidgetItem{parent, QStringList{(isFavorite(e.id) ? QString::fromUtf8("★ ") : QString{}) + QString::fromStdString(e.label)}};
        item->setData(0, idRole, QString::fromStdString(e.id));
        item->setToolTip(0, QString::fromStdString(e.id));
        if (e.id == current) {
            QFont bold = item->font(0);
            bold.setBold(true);
            item->setFont(0, bold);
        }
        return item;
    };
    QTreeWidgetItem * currentItem = nullptr;
    const auto addGroup = [this, &add, &currentItem] (const std::string& name, const std::vector<const Entry *>& list, bool open) {
        if (list.empty()) {
            return;
        }
        auto * top = new QTreeWidgetItem{tree, QStringList{QString::fromStdString(name) + QString{"  (%1)"}.arg(list.size())}};
        QFont bold = top->font(0);
        bold.setBold(true);
        top->setFont(0, bold);
        top->setFlags(top->flags() & ~Qt::ItemIsSelectable);
        for (const auto * e : list) {
            auto * item = add(top, *e);
            if (e->id == current && currentItem == nullptr) {
                currentItem = item;
            }
        }
        top->setExpanded(open);
    };
    std::vector<const Entry *> stars;
    for (const auto& id : favorites) {
        const auto found = std::find_if(entries.begin(), entries.end(), [&id] (const Entry& e) { return e.id == id; });
        if (found != entries.end()) {
            stars.push_back(&*found);
        }
    }
    addGroup("Favorites", stars, true);
    std::vector<std::string> order = groupOrder();
    for (const auto& e : entries) {
        if (std::find(order.begin(), order.end(), e.group) == order.end()) {
            order.push_back(e.group);
        }
    }
    for (const auto& name : order) {
        std::vector<const Entry *> list;
        for (const auto& e : entries) {
            if (e.group == name) {
                list.push_back(&e);
            }
        }
        if (name == "Upper air") {   // the levels from the top down (200, 250 ... 925), and in each the fields in one order: wind, temperature, humidity, vorticity, the rest (the list was in the order the charts were made)
            const auto level = [] (const Entry * e) {
                const auto label = QString::fromStdString(e->label);
                const auto digits = label.left(label.indexOf(QRegularExpression{"[^0-9]"}));
                return digits.isEmpty() ? 100000 : digits.toInt();
            };
            const auto field = [] (const Entry * e) {
                const auto label = QString::fromStdString(e->label).toLower();
                return label.contains("wind") ? 0 : label.contains("temperature") ? 1 : label.contains("humidity") ? 2 : label.contains("vorticity") ? 3 : 4;
            };
            std::stable_sort(list.begin(), list.end(), [&] (const Entry * a, const Entry * b) { return std::make_pair(level(a), field(a)) < std::make_pair(level(b), field(b)); });
        }
        bool holdsCurrent = false;
        for (const auto * e : list) {
            holdsCurrent = holdsCurrent || e->id == current;
        }
        addGroup(name, list, holdsCurrent);
    }
    if (currentItem) {
        tree->setCurrentItem(currentItem);
        tree->scrollToItem(currentItem);
    }
    updateStar();
}

void ProductPicker::filter(const QString& text) {
    const auto needle = text.trimmed().toLower();
    for (int g = 0; g < tree->topLevelItemCount(); g++) {
        auto * top = tree->topLevelItem(g);
        int shown = 0;
        for (int i = 0; i < top->childCount(); i++) {
            auto * item = top->child(i);
            const bool match = needle.isEmpty() || item->text(0).toLower().contains(needle) || item->data(0, idRole).toString().toLower().contains(needle);
            item->setHidden(!match);
            shown += match ? 1 : 0;
        }
        top->setHidden(shown == 0);
        if (!needle.isEmpty()) {
            top->setExpanded(shown > 0);
        }
    }
    if (!needle.isEmpty()) {   // the first match is the one Enter picks
        for (int g = 0; g < tree->topLevelItemCount(); g++) {
            auto * top = tree->topLevelItem(g);
            for (int i = 0; i < top->childCount(); i++) {
                if (!top->child(i)->isHidden()) {
                    tree->setCurrentItem(top->child(i));
                    return;
                }
            }
        }
    }
}

std::string ProductPicker::selectedId() const {
    const auto * item = tree->currentItem();
    return item && item->parent() ? item->data(0, idRole).toString().toStdString() : std::string{};
}

void ProductPicker::pickCurrent() {
    const auto id = selectedId();
    if (id.empty()) {
        return;
    }
    if (onPick) {
        onPick(id);
    }
    close();
}

void ProductPicker::updateStar() {
    const auto id = selectedId();
    const bool is = std::find(favorites.begin(), favorites.end(), id) != favorites.end();
    star->setEnabled(!id.empty());
    star->setText(is ? QString::fromUtf8("★ Remove from favorites") : QString::fromUtf8("☆ Add to favorites"));
}

void ProductPicker::toggleFavorite() {
    const auto id = selectedId();
    if (id.empty()) {
        return;
    }
    const auto found = std::find(favorites.begin(), favorites.end(), id);
    if (found != favorites.end()) {
        favorites.erase(found);
    } else {
        favorites.push_back(id);
    }
    if (onFavorites) {
        onFavorites(favorites);
    }
    const auto text = search->text();
    fill();
    filter(text);
}

void ProductPicker::ticked() {
    std::vector<std::string> out;
    for (size_t i = 0; i < boxes.size(); i++) {
        if (boxes[i]->isChecked()) {
            out.push_back(extras[i].id);
        }
    }
    if (onOverlays) {
        onOverlays(out);
    }
}

// where an area goes in the picker's tree
std::string ProductPicker::sectorGroup(const std::string& id) {
    static const std::map<std::string, std::string> groups{
        {"CONUS", "United States"}, {"NORTHEAST", "United States"}, {"MID-ATLANTIC", "United States"}, {"SOUTHEAST", "United States"}, {"GREAT-LAKES", "United States"},
        {"OHIO-VALLEY", "United States"}, {"S-PLAINS", "United States"}, {"C-PLAINS", "United States"}, {"N-PLAINS", "United States"}, {"ROCKIES", "United States"}, {"SOUTHWEST", "United States"},
        {"PACIFIC-NW", "United States"}, {"CALIFORNIA", "United States"}, {"GULF-COAST", "United States"}, {"ALASKA", "United States"}, {"HAWAII", "United States"},
        {"NAMER", "North America and the Caribbean"}, {"CENT-AMER", "North America and the Caribbean"}, {"CARIBBEAN", "North America and the Caribbean"},
        {"GULF-MEXICO", "North America and the Caribbean"},
        {"SAMER", "Continents"}, {"AFRICA", "Continents"}, {"EUROPE", "Continents"}, {"ASIA", "Continents"}, {"INDIA", "Continents"}, {"E-ASIA", "Continents"},
        {"SE-ASIA", "Continents"}, {"MIDDLE-EAST", "Continents"}, {"AUSTRALIA", "Continents"},
        {"WEST-ATL", "Oceans"}, {"ATLANTIC", "Oceans"}, {"NORTH-ATL", "Oceans"}, {"EAST-PAC", "Oceans"}, {"NORTH-PAC", "Oceans"}, {"SOUTH-PAC", "Oceans"},
        {"INDIAN-OCEAN", "Oceans"}, {"US-SAMOA", "Oceans"},
        {"GLOBAL", "World and poles"}, {"TROPICS", "World and poles"}, {"NORTHERN-HEMI", "World and poles"}, {"SOUTHERN-HEMI", "World and poles"},
        {"POLAR", "World and poles"}, {"ARTIC", "World and poles"}};
    if (id.compare(0, 6, "LOCAL:") == 0) {
        return "Around a saved location";
    }
    if (Hafs::isStormId(id)) {
        return "Storms in the model";
    }
    const auto found = groups.find(id);
    return found == groups.end() ? "Other" : found->second;
}

