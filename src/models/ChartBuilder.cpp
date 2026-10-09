// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/ChartBuilder.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>
#include "util/Utility.h"

ChartBuilder::ChartBuilder(QWidget * parent, const std::string& model, const std::vector<GfsChart::Template>& templates)
    : QDialog{parent, Qt::Tool}
    , model{model}
    , templates{templates}
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Build a chart - " + QString::fromStdString(model));
    auto * column = new QVBoxLayout{this};
    which = new QComboBox{this};
    for (const auto& t : templates) {
        which->addItem(QString::fromStdString(t.title));
    }
    column->addWidget(which);
    settingsArea = new QWidget{this};
    new QFormLayout{settingsArea};
    column->addWidget(settingsArea);
    summary = new QLabel{this};
    summary->setWordWrap(true);
    column->addWidget(summary);
    auto * hint = new QLabel{"The chart is for the hour chosen on the timeline: a period ends at that hour. It counts the members of the ensemble (the share that pass the limit), so the first draw of a long period "
                             "fetches many fields; the ones read stay for the next hour.", this};
    hint->setWordWrap(true);
    hint->setStyleSheet("color: gray;");
    column->addWidget(hint);
    auto * row = new QHBoxLayout;
    row->addStretch();
    auto * show = new QPushButton{"Show", this};
    show->setDefault(true);
    row->addWidget(show);
    auto * close = new QPushButton{"Close", this};
    row->addWidget(close);
    column->addLayout(row);
    QObject::connect(which, &QComboBox::currentIndexChanged, this, [this] { rebuildSettings(); });
    QObject::connect(show, &QPushButton::clicked, this, [this] {
        if (onBuilt) {
            onBuilt(chosenId());
        }
    });
    QObject::connect(close, &QPushButton::clicked, this, [this] { this->close(); });
    which->setCurrentIndex(std::clamp(Utility::readPrefInt("MODEL_BUILD_TEMPLATE", 0), 0, static_cast<int>(templates.size()) - 1));
    rebuildSettings();
}

void ChartBuilder::rebuildSettings() {
    const auto& t = templates[static_cast<size_t>(std::max(0, which->currentIndex()))];
    Utility::writePref("MODEL_BUILD_TEMPLATE", std::to_string(which->currentIndex()));
    auto * form = static_cast<QFormLayout *>(settingsArea->layout());
    while (form->rowCount() > 0) {
        form->removeRow(0);
    }
    boxes.clear();
    for (const auto& s : t.settings) {
        auto * box = new QComboBox{settingsArea};
        const auto stored = Utility::readPref("MODEL_BUILD_" + t.id + "_" + s.key, "");
        const double wanted = stored.empty() ? s.standard : std::atof(stored.c_str());
        int at = 0;
        for (size_t i = 0; i < s.choices.size(); i++) {
            auto text = QString::number(s.choices[i], 'f', 2);
            while (text.contains('.') && (text.endsWith('0') || text.endsWith('.'))) {
                text.chop(1);
            }
            box->addItem(text + " " + QString::fromStdString(s.unit), s.choices[i]);
            if (std::abs(s.choices[i] - wanted) < 1e-9) {
                at = static_cast<int>(i);
            }
        }
        box->setCurrentIndex(at);
        QObject::connect(box, &QComboBox::currentIndexChanged, this, [this] { updateText(); });
        form->addRow(QString::fromStdString(s.label), box);
        boxes.push_back(box);
    }
    updateText();
}

std::string ChartBuilder::chosenId() const {
    const auto& t = templates[static_cast<size_t>(std::max(0, which->currentIndex()))];
    std::vector<double> values;
    for (size_t i = 0; i < boxes.size(); i++) {
        values.push_back(boxes[i]->currentData().toDouble());
        Utility::writePref("MODEL_BUILD_" + t.id + "_" + t.settings[i].key, std::to_string(values.back()));
    }
    return GfsChart::generatedChart(t.id, values);
}

void ChartBuilder::updateText() {
    const auto * product = GfsChart::product(chosenId(), model);
    summary->setText(product ? QString::fromStdString(product->label) : QString{"This chart is not available."});
}
