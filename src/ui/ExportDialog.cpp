// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "ui/ExportDialog.h"
#include <algorithm>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>

ExportDialog::ExportDialog(QWidget * parent, const QString& title, const QString& intro)
    : QDialog{parent}
{
    setWindowTitle(title);
    auto * column = new QVBoxLayout{this};
    if (!intro.isEmpty()) {
        auto * text = new QLabel{intro, this};
        text->setWordWrap(true);
        column->addWidget(text);
    }
    auto * kindBox = new QVBoxLayout;
    kindsLayout = kindBox;
    column->addLayout(kindBox);
    auto * form = new QFormLayout;
    optionsLayout = form;
    column->addLayout(form);
    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this};
    QObject::connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    column->addWidget(buttons);
}

void ExportDialog::addKind(const QString& id, const QString& label, const QString& tip) {
    auto * radio = new QRadioButton{label, this};
    radio->setToolTip(tip);
    kindsLayout->addWidget(radio);
    if (kinds.empty()) {
        radio->setChecked(true);
    }
    QObject::connect(radio, &QRadioButton::toggled, this, [this] (bool) { applyKind(); });
    kinds.push_back({id, radio});
    applyKind();
}

void ExportDialog::addOption(const QString& key, const QString& label, const std::vector<std::pair<QString, QString>>& items, const QString& current, const std::vector<QString>& onlyFor) {
    auto * box = new QComboBox{this};
    for (const auto& [id, text] : items) {
        box->addItem(text, id);
        if (id == current) {
            box->setCurrentIndex(box->count() - 1);
        }
    }
    auto * caption = new QLabel{label, this};
    static_cast<QFormLayout *>(optionsLayout)->addRow(caption, box);
    options.push_back({key, box, caption, onlyFor});
    applyKind();
}

QString ExportDialog::kind() const {
    for (const auto& k : kinds) {
        if (k.radio->isChecked()) {
            return k.id;
        }
    }
    return {};
}

void ExportDialog::selectKind(const QString& id) {
    for (const auto& k : kinds) {
        if (k.id == id) {
            k.radio->setChecked(true);
        }
    }
}

QString ExportDialog::value(const QString& key) const {
    const auto current = kind();
    for (const auto& o : options) {
        if (o.key == key) {
            const bool applies = o.onlyFor.empty() || std::find(o.onlyFor.begin(), o.onlyFor.end(), current) != o.onlyFor.end();
            return applies ? o.box->currentData().toString() : QString{};
        }
    }
    return {};
}

void ExportDialog::applyKind() {
    const auto current = kind();
    for (const auto& o : options) {
        const bool applies = o.onlyFor.empty() || std::find(o.onlyFor.begin(), o.onlyFor.end(), current) != o.onlyFor.end();
        o.box->setEnabled(applies);
        o.label->setEnabled(applies);
    }
}
