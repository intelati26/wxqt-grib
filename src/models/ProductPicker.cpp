// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ProductPicker.h"
#include <algorithm>
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
    auto * choose = new QPushButton{"Show this chart", this};
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
    fill();
    QObject::connect(search, &QLineEdit::textChanged, [this] (const QString& text) { filter(text); });
    QObject::connect(search, &QLineEdit::returnPressed, [this] { pickCurrent(); });
    QObject::connect(tree, &QTreeWidget::itemDoubleClicked, [this] (QTreeWidgetItem *, int) { pickCurrent(); });
    QObject::connect(tree, &QTreeWidget::currentItemChanged, [this] (QTreeWidgetItem *, QTreeWidgetItem *) { updateStar(); });
    QObject::connect(choose, &QPushButton::clicked, [this] { pickCurrent(); });
    QObject::connect(star, &QPushButton::clicked, [this] { toggleFavorite(); });
    search->setFocus();
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
