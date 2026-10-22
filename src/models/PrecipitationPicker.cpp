// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/PrecipitationPicker.h"
#include "ui/UiStandards.h"
#include <algorithm>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace {
    const std::vector<const char *>& whats() {
        static const std::vector<const char *> all{"Precipitation over a period", "Precipitation since the start of the run", "Type: rain, snow or mixed", "Rate and type", "Rate", "Snowfall since the start",
                                                    "Snow depth change", "Precipitable water"};
        return all;
    }
    const std::vector<const char *>& lineChoices() {
        static const std::vector<const char *> all{"No lines", "MSLP and 1000-500 mb thickness", "MSLP and 1000-850 mb thickness", "MSLP and 850-700 mb thickness", "MSLP and 850 mb temperature", "MSLP and 10 m wind"};
        return all;
    }
}

bool PrecipitationPicker::offeredFor(const std::vector<std::string>& ids) {
    return std::count_if(ids.begin(), ids.end(), [] (const std::string& id) { return id.compare(0, 8, "precip_p") == 0; }) >= 2;
}

PrecipitationPicker::PrecipitationPicker(QWidget * parent, const std::vector<std::string>& modelIds, const std::vector<std::string>& modelLabels, const std::string& current)
    : QDialog{parent, Qt::Tool}
    , ids{modelIds}
    , labels{modelLabels}
{
    setWindowTitle("Precipitation charts");
    // every chart this picker can make, by its parts (the ids are the charts' own; one that the model lacks is dropped)
    const auto has = [this] (const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };
    const auto add = [this, &has] (int what, int period, int lines, const std::string& id) {
        if (has(id)) {
            rows.push_back({what, period, lines, id});
        }
    };
    for (const int period : {1, 3, 6, 12, 24, 36, 48, 60}) {
        add(0, period, 0, "precip_p" + std::string{period < 10 ? "0" : ""} + std::to_string(period));
    }
    add(0, 0, 1, "1000_500_thick");
    add(0, 0, 2, "1000_850_thick");
    add(0, 0, 3, "850_700_thick");
    add(0, 0, 4, "850_temp_mslp_precip");
    add(0, 0, 5, "10m_wnd_precip");
    add(1, 0, 0, "precip_ptot");
    add(2, 0, 0, "precip_type");
    add(3, 0, 0, "precip_rate_type");
    add(4, 0, 0, "precip_rate");
    add(5, 0, 0, "snow_total");
    add(5, 0, 0, "snow_ptot");
    add(6, 0, 0, "snodpth_chng");
    add(7, 0, 0, "precip_pwat");
    add(7, 0, 0, "mslp_pwat");
    auto * column = new QVBoxLayout{this};
    auto * form = new QFormLayout;
    whatBox = UiStandards::combo(this);
    periodBox = UiStandards::combo(this);
    linesBox = UiStandards::combo(this);
    form->addRow("What", whatBox);
    form->addRow("Period", periodBox);
    form->addRow("Lines", linesBox);
    column->addLayout(form);
    result = new QLabel{this};
    result->setWordWrap(true);
    column->addWidget(result);
    auto * buttons = new QHBoxLayout;
    standard = UiStandards::button(this, "Standard", "The usual chart: the precipitation over 6 hours (or the nearest period the model has)");
    use = UiStandards::button(this, "Show this chart");
    buttons->addWidget(standard);
    buttons->addStretch();
    buttons->addWidget(use);
    column->addLayout(buttons);
    // start from the chart on show when it is one of ours
    const auto at = std::find_if(rows.begin(), rows.end(), [&current] (const Row& r) { return r.id == current; });
    building = true;
    for (size_t i = 0; i < whats().size(); i++) {
        if (std::any_of(rows.begin(), rows.end(), [i] (const Row& r) { return r.what == static_cast<int>(i); })) {
            whatBox->addItem(whats()[i], static_cast<int>(i));
        }
    }
    if (at != rows.end()) {
        whatBox->setCurrentIndex(std::max(0, whatBox->findData(at->what)));
    }
    building = false;
    rebuild();
    if (at != rows.end()) {
        building = true;
        periodBox->setCurrentIndex(std::max(0, periodBox->findData(at->period)));
        linesBox->setCurrentIndex(std::max(0, linesBox->findData(at->lines)));
        building = false;
        refreshResult();
    }
    QObject::connect(whatBox, &QComboBox::currentIndexChanged, this, [this] (int) { if (!building) { rebuild(); } });
    QObject::connect(periodBox, &QComboBox::currentIndexChanged, this, [this] (int) { if (!building) { refreshResult(); } });
    QObject::connect(linesBox, &QComboBox::currentIndexChanged, this, [this] (int) { if (!building) { refreshResult(); } });
    QObject::connect(standard, &QPushButton::clicked, this, [this] {
        building = true;
        whatBox->setCurrentIndex(std::max(0, whatBox->findData(0)));
        building = false;
        rebuild();
        const auto preferred = periodBox->findData(6);
        if (preferred >= 0) {
            periodBox->setCurrentIndex(preferred);
        }
        linesBox->setCurrentIndex(std::max(0, linesBox->findData(0)));
    });
    QObject::connect(use, &QPushButton::clicked, this, [this] {
        const auto id = chosenId();
        if (!id.empty() && onPick) {
            onPick(id);
        }
    });
    resize(420, 220);
}

// the periods and the lines that go with the "what" chosen: only those that make a chart the model has
void PrecipitationPicker::rebuild() {
    const int what = whatBox->currentData().toInt();
    building = true;
    periodBox->clear();
    linesBox->clear();
    std::vector<int> periods, lines;
    for (const auto& r : rows) {
        if (r.what != what) {
            continue;
        }
        if (r.period > 0 && std::find(periods.begin(), periods.end(), r.period) == periods.end()) {
            periods.push_back(r.period);
        }
        if (std::find(lines.begin(), lines.end(), r.lines) == lines.end()) {
            lines.push_back(r.lines);
        }
    }
    if (periods.empty()) {
        periodBox->addItem(what == 0 ? "the model's own step (3 or 6 hours)" : "not a choice for this", 0);
    }
    for (const int p : periods) {
        periodBox->addItem(QString::number(p) + (p == 1 ? " hour" : " hours"), p);
    }
    if (what == 0 && !periods.empty()) {
        // the lines make the period the model's own step: a choice of lines is a different chart than the period one
    }
    for (const int l : lines) {
        linesBox->addItem(lineChoices()[static_cast<size_t>(l)], l);
    }
    periodBox->setEnabled(periods.size() > 1);
    linesBox->setEnabled(lines.size() > 1);
    building = false;
    refreshResult();
}

std::string PrecipitationPicker::chosenId() const {
    const int what = whatBox->currentData().toInt(), lines = linesBox->currentData().toInt();
    int period = periodBox->currentData().toInt();
    // lines come with the model's own step; the periods come without lines
    for (const auto& r : rows) {
        if (r.what == what && r.lines == lines && (lines != 0 || r.period == period || (r.period == 0 && period == 0))) {
            return r.id;
        }
    }
    for (const auto& r : rows) {
        if (r.what == what && r.lines == lines) {
            return r.id;
        }
    }
    return {};
}

void PrecipitationPicker::refreshResult() {
    // a chart with lines is of the model's own step: the period is not a choice then
    const bool withLines = linesBox->currentData().toInt() != 0;
    periodBox->setEnabled(!withLines && periodBox->count() > 1);
    const auto id = chosenId();
    QString name;
    for (size_t i = 0; i < ids.size() && i < labels.size(); i++) {
        if (ids[i] == id) {
            name = QString::fromStdString(labels[i]);
        }
    }
    result->setText(id.empty() ? "The model has no chart for this choice." : "Chart: " + name);
    use->setEnabled(!id.empty());
}
