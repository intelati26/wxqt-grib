// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/PaletteEditor.h"
#include <algorithm>
#include <cmath>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include "models/PaletteStore.h"

// the scale drawn as a bar, from the first stop to the last, over a checkerboard so transparency shows
class PaletteBar : public QWidget {
public:
    explicit PaletteBar(QWidget * parent) : QWidget{parent} {
        setMinimumHeight(34);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    void setRamp(const GfsChart::Ramp& r) {
        ramp = r;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter{this};
        const QRect box = rect().adjusted(1, 1, -1, -1);
        for (int y = box.top(); y < box.bottom(); y += 8) {
            for (int x = box.left(); x < box.right(); x += 8) {
                painter.fillRect(QRect{x, y, 8, 8}.intersected(box), ((x - box.left()) / 8 + (y - box.top()) / 8) % 2 ? QColor{200, 200, 200} : QColor{240, 240, 240});
            }
        }
        if (ramp.stops.size() >= 2) {
            const double low = ramp.stops.front().first, high = ramp.stops.back().first;
            for (int x = box.left(); x <= box.right(); x++) {
                const double v = low + (high - low) * (x - box.left()) / std::max(1, box.width() - 1);
                const bool below = ramp.banded && v < low;
                if (!below) {
                    painter.setPen(QColor::fromRgba(ramp.at(v)));
                    painter.drawLine(x, box.top(), x, box.bottom());
                }
            }
        }
        painter.setPen(palette().color(QPalette::Mid));
        painter.drawRect(box);
    }

private:
    GfsChart::Ramp ramp;
};

PaletteEditor::PaletteEditor(QWidget * parent, const std::string& title, const GfsChart::Ramp& original, GfsChart::Quantity quantity, bool us, std::function<void()> changed)
    : QDialog{parent}
    , key{GfsChart::rampKey(original)}
    , original{original}
    , quantity{quantity}
    , us{us}
    , changed{std::move(changed)}
{
    setWindowTitle("Colors: " + QString::fromStdString(title));
    resize(520, 560);
    auto * layout = new QVBoxLayout{this};
    const auto unit = GfsChart::unitLabel(quantity, us);
    auto * about = new QLabel{"The color scale of this kind of chart" + QString{unit.empty() ? "" : " (values in " + QString::fromStdString(unit) + ")"} +
                              ". Every chart that uses the same default scale changes with it. Colors can be partly or fully transparent.", this};
    about->setWordWrap(true);
    layout->addWidget(about);
    bar = new PaletteBar{this};
    layout->addWidget(bar);
    table = new QTableWidget{0, 2, this};
    table->setHorizontalHeaderLabels({unit.empty() ? "Value" : QString::fromStdString("Value (" + unit + ")"), "Color (double-click to change)"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table, 1);
    QObject::connect(table, &QTableWidget::cellDoubleClicked, this, [this] (int row, int column) {
        if (column == 1) {
            pickColor(row);
        }
    });
    banded = new QCheckBox{"Flat bands (each color holds from its value up to the next stop, nothing below the first)", this};
    QObject::connect(banded, &QCheckBox::toggled, this, [this] { refreshBar(); });
    layout->addWidget(banded);
    auto * row1 = new QHBoxLayout;
    for (const auto& [text, handler] : std::vector<std::pair<QString, std::function<void()>>>{
             {"Add stop", [this] { addStop(); }}, {"Remove stop", [this] { removeStop(); }}, {"Back to default", [this] { reset(); }},
             {"Import...", [this] { import(); }}, {"Export...", [this] { exportFile(); }}}) {
        auto * button = new QPushButton{text, this};
        QObject::connect(button, &QPushButton::clicked, this, [handler = handler] { handler(); });
        row1->addWidget(button);
    }
    layout->addLayout(row1);
    note = new QLabel{this};
    note->setWordWrap(true);
    layout->addWidget(note);
    auto * row2 = new QHBoxLayout;
    row2->addStretch();
    auto * applyButton = new QPushButton{"Apply", this};
    applyButton->setDefault(true);
    QObject::connect(applyButton, &QPushButton::clicked, this, [this] { apply(); });
    auto * closeButton = new QPushButton{"Close", this};
    QObject::connect(closeButton, &QPushButton::clicked, this, [this] { close(); });
    row2->addWidget(applyButton);
    row2->addWidget(closeButton);
    layout->addLayout(row2);
    const auto stored = PaletteStore::all();
    const auto found = stored.find(key);
    fill(found != stored.end() ? found->second : original);
    say(found != stored.end() ? "Your own scale is in use for this kind of chart." : "This is the default scale.");
    setAttribute(Qt::WA_DeleteOnClose);
    show();
}

void PaletteEditor::say(const QString& text) {
    note->setText(text);
}

void PaletteEditor::fill(const GfsChart::Ramp& ramp) {
    table->setRowCount(0);
    banded->setChecked(ramp.banded);
    for (const auto& [value, color] : ramp.stops) {
        const int row = table->rowCount();
        table->insertRow(row);
        auto * spin = new QDoubleSpinBox;
        spin->setDecimals(3);
        spin->setRange(-1e6, 1e6);
        spin->setValue(GfsChart::shownValue(quantity, value, us));
        QObject::connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { refreshBar(); });
        table->setCellWidget(row, 0, spin);
        auto * item = new QTableWidgetItem;
        item->setData(Qt::UserRole, color);
        table->setItem(row, 1, item);
        item->setBackground(color);
        item->setText(color.name(QColor::HexArgb));
        item->setForeground(color.lightness() > 128 ? Qt::black : Qt::white);
    }
    refreshBar();
}

GfsChart::Ramp PaletteEditor::current() const {
    GfsChart::Ramp ramp;
    ramp.banded = banded->isChecked();
    for (int row = 0; row < table->rowCount(); row++) {
        const auto * spin = qobject_cast<QDoubleSpinBox *>(table->cellWidget(row, 0));
        const auto color = table->item(row, 1)->data(Qt::UserRole).value<QColor>();
        ramp.stops.emplace_back(GfsChart::storedValue(quantity, spin ? spin->value() : 0.0, us), color);
    }
    std::stable_sort(ramp.stops.begin(), ramp.stops.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    return ramp;
}

void PaletteEditor::refreshBar() {
    bar->setRamp(current());
}

void PaletteEditor::pickColor(int row) {
    const auto before = table->item(row, 1)->data(Qt::UserRole).value<QColor>();
    const auto chosen = QColorDialog::getColor(before, this, "Color of the stop", QColorDialog::ShowAlphaChannel);
    if (!chosen.isValid()) {
        return;
    }
    auto * item = table->item(row, 1);
    item->setData(Qt::UserRole, chosen);
    item->setBackground(chosen);
    item->setText(chosen.name(QColor::HexArgb));
    item->setForeground(chosen.lightness() > 128 ? Qt::black : Qt::white);
    refreshBar();
}

void PaletteEditor::addStop() {   // between the selected stop and the next (or after the last): the scale's color there, so nothing changes until it is edited
    auto ramp = current();
    if (ramp.stops.empty()) {
        ramp.stops = {{0.0, QColor{255, 255, 255}}, {1.0, QColor{0, 0, 0}}};
    } else {
        const int at = std::clamp(table->currentRow(), 0, static_cast<int>(ramp.stops.size()) - 1);
        const double low = ramp.stops[static_cast<size_t>(at)].first;
        const double high = static_cast<size_t>(at) + 1 < ramp.stops.size() ? ramp.stops[static_cast<size_t>(at) + 1].first : low + (low - (at > 0 ? ramp.stops[static_cast<size_t>(at) - 1].first : low - 1.0));
        const double value = (low + high) / 2.0;
        ramp.stops.emplace_back(value, QColor::fromRgba(ramp.at(value)));
        std::stable_sort(ramp.stops.begin(), ramp.stops.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    }
    fill(ramp);
}

void PaletteEditor::removeStop() {
    if (table->rowCount() <= 2) {
        say("A scale needs at least two stops.");
        return;
    }
    const int row = table->currentRow();
    if (row < 0) {
        say("Select a stop to remove.");
        return;
    }
    table->removeRow(row);
    refreshBar();
}

void PaletteEditor::apply() {
    const auto ramp = current();
    if (ramp.stops.size() < 2) {
        say("A scale needs at least two stops.");
        return;
    }
    for (size_t i = 1; i < ramp.stops.size(); i++) {
        if (ramp.stops[i].first == ramp.stops[i - 1].first) {
            say("Two stops have the same value; give each its own.");
            return;
        }
    }
    PaletteStore::set(key, ramp);
    say("Applied: every chart of this kind is drawn with it now.");
    if (changed) {
        changed();
    }
}

void PaletteEditor::reset() {
    PaletteStore::clear(key);
    fill(original);
    say("Back to the default scale.");
    if (changed) {
        changed();
    }
}

void PaletteEditor::import() {
    const auto path = QFileDialog::getOpenFileName(this, "Import a color scale", {}, "Color scales (*.txt *.pal);;All files (*)");
    if (path.isEmpty()) {
        return;
    }
    QFile file{path};
    if (!file.open(QIODevice::ReadOnly)) {
        say("Could not read " + path);
        return;
    }
    GfsChart::Ramp ramp;
    std::string error;
    if (!GfsChart::rampFromText(file.readAll().toStdString(), ramp, error)) {
        say("Not a color scale: " + QString::fromStdString(error));
        return;
    }
    fill(ramp);
    say("Imported from " + path + " (press Apply to use it). Its values are in the grids' units: " + QString::fromStdString(GfsChart::unitLabel(quantity, false).empty() ? "the chart's own" : GfsChart::unitLabel(quantity, false)) + ".");
}

void PaletteEditor::exportFile() {
    const auto path = QFileDialog::getSaveFileName(this, "Export the color scale", "palette.txt", "Color scales (*.txt)");
    if (path.isEmpty()) {
        return;
    }
    QFile file{path};
    if (!file.open(QIODevice::WriteOnly) || file.write(("# wxqt color scale: value (in the grids' units) red green blue alpha\n" + GfsChart::rampToText(current())).c_str()) < 0) {
        say("Could not write " + path);
        return;
    }
    say("Saved to " + path);
}
