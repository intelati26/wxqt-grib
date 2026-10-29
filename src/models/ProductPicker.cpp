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
    return {"Upper air", "Surface", "Precipitation and moisture", "Severe: instability", "Severe: rotation and shear", "Severe: storm structure", "Severe: composite indices", "Storms and severe",
            "Boundary layer and fire weather", "Air quality and smoke", "Aviation and visibility", "Tropical and dynamics", "Probability: precipitation", "Probability: severe and storms",
            "Probability: aviation and visibility", "Probability: temperature and wind", "Ensemble spread", "Ensemble: percentiles and extremes", "Ensemble: spaghetti and paintballs", "Ensemble: single members",
            "Changes and trends", "Anomalies", "Satellite",
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
        inner->setHorizontalSpacing(8);
        // The wind, temperature, humidity and height as a table: a row for each level and, for each field, a box for the color fill (laid over the chart's own), for lines and (the wind) for barbs.
        // The rest (the sea level pressure, the thicknesses) are listed under it.
        struct Cell {
            int row{-1}, column{-1};
        };
        const std::vector<std::string> tags{"10m", "850", "700", "500", "300", "250", "200"};
        const std::vector<std::string> rowNames{"10 m / 2 m", "850 mb", "700 mb", "500 mb", "300 mb", "250 mb", "200 mb"};
        // the columns: wind fill, lines, barbs; temperature fill, lines; humidity fill, lines; height fill, lines
        const std::vector<std::pair<std::string, std::vector<std::string>>> fields{{"Wind", {"fill", "lines", "barbs"}}, {"Temperature", {"fill", "lines"}}, {"Humidity", {"fill", "lines"}}, {"Height", {"fill", "lines"}}};
        const auto cellOf = [&] (const std::string& id) {
            Cell cell;
            std::string field, style, tag;
            if (id.compare(0, 5, "fill_") == 0 || id.compare(0, 5, "line_") == 0) {
                const auto first = id.find('_', 5);
                if (first == std::string::npos) {
                    return cell;
                }
                style = id.compare(0, 4, "fill") == 0 ? "fill" : "lines";
                field = id.substr(5, first - 5);
                tag = id.substr(first + 1);
            } else if (id.compare(0, 6, "barbs_") == 0) {
                field = "wind";
                style = "barbs";
                tag = id.substr(6);
            } else if (id.size() == 4 && id[0] == 'z') {   // z500: the height lines
                field = "hgt";
                style = "lines";
                tag = id.substr(1);
            } else {
                return cell;
            }
            const std::string fieldName = field == "wind" ? "Wind" : field == "temp" ? "Temperature" : field == "rh" ? "Humidity" : field == "hgt" ? "Height" : "";
            int column = 1;
            for (const auto& [name, styles] : fields) {
                for (const auto& each : styles) {
                    if (name == fieldName && each == style) {
                        cell.column = column;
                    }
                    column++;
                }
            }
            if (tag == "2m") {
                tag = "10m";   // the ground row is one
            }
            const auto at = std::find(tags.begin(), tags.end(), tag);
            if (at != tags.end() && cell.column > 0) {
                cell.row = static_cast<int>(at - tags.begin()) + 2;
            }
            return cell;
        };
        {   // the headings
            int column = 1;
            for (const auto& [name, styles] : fields) {
                auto * title = new QLabel{"<b>" + QString::fromStdString(name) + "</b>", group};
                title->setAlignment(Qt::AlignCenter);
                inner->addWidget(title, 0, column, 1, static_cast<int>(styles.size()));
                for (const auto& style : styles) {
                    auto * small = new QLabel{QString::fromStdString(style), group};
                    small->setAlignment(Qt::AlignCenter);
                    UiStandards::muted(small);
                    inner->addWidget(small, 1, column++);
                }
            }
            for (size_t i = 0; i < rowNames.size(); i++) {
                inner->addWidget(new QLabel{QString::fromStdString(rowNames[i]), group}, static_cast<int>(i) + 2, 0);
            }
        }
        int more = 0;   // the lines that are not of the table, two to a row under it
        int moreRow = static_cast<int>(rowNames.size()) + 3;
        bool moreHeading = false;
        for (const auto& extra : extras) {
            const auto cell = cellOf(extra.id);
            auto * box = new QCheckBox{cell.row >= 0 ? QString{} : QString::fromStdString(extra.label), group};
            box->setToolTip(QString::fromStdString(extra.label + (extra.group == "Fill" ? " (a translucent color fill over the chart's own)" : extra.group == "Lines" ? " (lines)" : extra.group.find("barb") != std::string::npos ? " (wind barbs)" : "")));
            const bool on = std::find(ticked.begin(), ticked.end(), extra.id) != ticked.end();
            box->setChecked(on);
            anyTicked = anyTicked || on;
            boxes.push_back(box);
            if (cell.row >= 0) {
                inner->addWidget(box, cell.row, cell.column, Qt::AlignCenter);
            } else {
                if (!moreHeading) {
                    inner->addWidget(new QLabel{"<b>Other lines</b>", group}, moreRow++, 0, 1, 10);
                    moreHeading = true;
                }
                inner->addWidget(box, moreRow, (more % 2) * 5, 1, 5);
                if (++more % 2 == 0) {
                    moreRow++;
                }
            }
            QObject::connect(box, &QCheckBox::clicked, [this, box] {   // winds of several levels stack on the chart (each its own color); a fill is one at a time
                for (size_t i = 0; i < boxes.size(); i++) {
                    if (boxes[i] == box && box->isChecked() && this->extras[i].group == "Fill") {
                        for (size_t k = 0; k < boxes.size(); k++) {
                            if (boxes[k] != box && this->extras[k].group == "Fill") {
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
            toggle->setText(open ? QString::fromUtf8("\u25BE Add fields to the chart: fill, lines, barbs") : QString::fromUtf8("\u25B8 Add fields to the chart: fill, lines, barbs"));
        };
        show(anyTicked);
        QObject::connect(toggle, &QPushButton::clicked, [group, show] { show(!group->isVisible()); });
        // the levels of the lower troposphere together: a chart with the 850 and 700 mb winds and heights stacked on it (850 red, 700 green)
        const auto has = [this] (const std::string& id) { return std::any_of(this->extras.begin(), this->extras.end(), [&id] (const auto& e) { return e.id == id; }); };
        const auto hasChart = [this] (const std::string& id) { return std::any_of(this->entries.begin(), this->entries.end(), [&id] (const Entry& e) { return e.id == id; }); };
        if (has("z850") && has("z700") && has("barbs_850") && has("barbs_700")) {
            const auto preset = [this, column, show] (const std::string& chart, const char * label, const char * tip) {
                auto * levels = UiStandards::button(this, label, tip);
                column->addWidget(levels);
                QObject::connect(levels, &QPushButton::clicked, [this, show, chart] {
                    const std::vector<std::string> want{"z850", "z700", "barbs_850", "barbs_700"};
                    for (size_t i = 0; i < boxes.size(); i++) {
                        boxes[i]->setChecked(std::find(want.begin(), want.end(), this->extras[i].id) != want.end());
                    }
                    show(true);
                    this->ticked();
                    if (this->onPick) {
                        this->current = chart;
                        this->onPick(this->current);
                    }
                });
            };
            if (hasChart("500_wnd_ht")) {
                preset("500_wnd_ht", "850/700/500 wind", "The 500 mb wind and height chart with the 850 and 700 mb winds and heights stacked on it: 850 red, 700 green, 500 black");
            }
            if (hasChart("850_700_rh")) {
                preset("850_700_rh", "850/700 humidity, wind", "The mean relative humidity of 850 and 700 mb with the winds and heights of both levels stacked on it: 850 red, 700 green");
            }
        }
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

void ProductPicker::setNote(const QString& text) {
    auto * note = new QLabel{text, this};
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    if (auto * column = qobject_cast<QVBoxLayout *>(layout())) {
        column->insertWidget(2, note);   // (below the search box and the list)
    }
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
        if (name == "Precipitation and moisture") {   // the amounts by their period (1, 3, 6 ... hours, then the total), then the charts with lines and the rest as they were made
            const auto rank = [] (const Entry * e) {
                static const QRegularExpression period{"precip_p(\\d+)$"};
                const auto m = period.match(QString::fromStdString(e->id));
                if (m.hasMatch()) {
                    return m.captured(1).toInt();
                }
                return e->id.find("precip_ptot") != std::string::npos ? 1000 : 100000;
            };
            std::stable_sort(list.begin(), list.end(), [&] (const Entry * a, const Entry * b) { return rank(a) < rank(b); });
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

