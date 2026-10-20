// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "mapkit/GlobeAreaPicker.h"
#include <algorithm>
#include <map>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include "gfs/GfsChart.h"
#include "models/ProductPicker.h"

namespace {
    // a name that tells the picker when the pointer is over it
    class AreaButton : public QPushButton {
    public:
        AreaButton(const QString& text, QWidget * parent) : QPushButton{text, parent} {
            setFlat(true);
            setCursor(Qt::PointingHandCursor);
            setStyleSheet("QPushButton { color: #2f6fe0; font-weight: bold; padding: 4px 6px; } QPushButton:hover { background: #dfe9fb; }");
        }
        std::function<void()> entered, left;

    protected:
        void enterEvent(QEnterEvent * e) override {
            QPushButton::enterEvent(e);
            if (entered) entered();
        }
        void leaveEvent(QEvent * e) override {
            QPushButton::leaveEvent(e);
            if (left) left();
        }
    };
}

GlobeAreaPicker::GlobeAreaPicker(QWidget * parent) : QDialog{parent} {
    setWindowTitle("Model area");
    setAttribute(Qt::WA_DeleteOnClose);
    auto * outer = new QVBoxLayout{this};
    auto * note = new QLabel{"Point at a name to see the area on the globe; click to choose it. Pointing at the globe does the same.", this};
    note->setWordWrap(true);
    outer->addWidget(note);
    auto * scroll = new QScrollArea{this};
    scroll->setWidgetResizable(true);
    auto * holder = new QWidget{scroll};
    auto * column = new QVBoxLayout{holder};
    column->setSpacing(2);
    std::map<std::string, std::vector<std::string>> byGroup;
    for (const auto& sector : GfsChart::sectors()) {
        if (sector.id.compare(0, 6, "LOCAL:") == 0) {
            continue;
        }
        byGroup[ProductPicker::sectorGroup(sector.id)].push_back(sector.id);
    }
    static const std::vector<std::string> order{"United States", "North America and the Caribbean", "Continents", "Oceans", "World and poles", "Other"};
    std::vector<std::string> groups;
    for (const auto& name : order) {
        if (byGroup.count(name)) {
            groups.push_back(name);
        }
    }
    for (const auto& [name, ids] : byGroup) {
        if (std::find(groups.begin(), groups.end(), name) == groups.end()) {
            groups.push_back(name);
        }
    }
    for (const auto& group : groups) {
        auto * heading = new QLabel{QString::fromStdString(group), holder};
        heading->setStyleSheet("font-weight: bold; margin-top: 6px;");
        column->addWidget(heading);
        auto * grid = new QGridLayout;
        grid->setSpacing(1);
        int i = 0;
        for (const auto& id : byGroup[group]) {
            auto * button = new AreaButton{QString::fromStdString(id), holder};
            button->entered = [this, id] { if (onHover) onHover(id); };
            button->left = [this] { if (onHover) onHover({}); };
            QObject::connect(button, &QPushButton::clicked, this, [this, id] { if (onPick) onPick(id); });
            grid->addWidget(button, i / 3, i % 3);
            i++;
        }
        column->addLayout(grid);
    }
    column->addStretch();
    scroll->setWidget(holder);
    outer->addWidget(scroll);
    resize(420, 640);
}
