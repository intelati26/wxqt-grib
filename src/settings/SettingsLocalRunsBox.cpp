// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsLocalRunsBox.h"
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include "gfs/GfsData.h"
#include "gfs/GfsModels.h"
#include "util/Utility.h"

namespace {
    std::vector<std::string> split(const std::string& text) {
        std::vector<std::string> out;
        for (const auto& part : QString::fromStdString(text).split(';', Qt::SkipEmptyParts)) {
            if (!part.trimmed().isEmpty()) {
                out.push_back(part.trimmed().toStdString());
            }
        }
        return out;
    }
    std::string defaults(const std::string& model) {
        const auto * def = GfsModels::find(model);
        std::string text;
        if (def != nullptr) {
            for (const auto& folder : def->source("").wrf.defaultFolders) {
                text += (text.empty() ? "" : "; ") + folder;
            }
        }
        return text;
    }
}

SettingsLocalRunsBox::SettingsLocalRunsBox(QWidget * parent)
    : QWidget{parent}
{
    setMaximumWidth(900);   // (the settings window does not wrap what is wider than it)
    auto * layout = new QVBoxLayout{this};
    auto * about = new QLabel{"Models made on this computer: the Model Viewer lists their runs with the others, and they can be compared with any model or with the MRMS radar at the same valid time. Each is a "
                              "folder of WRF-format history files (wrfout_d01_...) for every run. Give the folders to look in, separated by ; (a * stands for any name). Empty is the places the program "
                              "looks by itself.", this};
    about->setWordWrap(true);
    about->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    about->setMinimumHeight(60);
    layout->addWidget(about);
    rows.reserve(2);
    addRow(layout, "ARWEN", "WRF_FOLDERS_ARWEN");
    addRow(layout, "WOOF-NAM", "WRF_FOLDERS_WOOFNAM");
    layout->addStretch();
    for (auto& row : rows) {
        look(row);
    }
}

void SettingsLocalRunsBox::addRow(QVBoxLayout * layout, const std::string& model, const std::string& setting) {
    rows.push_back({model, setting, nullptr, nullptr});
    const size_t index = rows.size() - 1;
    auto * box = new QGroupBox{QString::fromStdString(model), this};
    auto * column = new QVBoxLayout{box};
    auto * line = new QHBoxLayout;
    auto * edit = new QLineEdit{box};
    const auto saved = Utility::readPref(setting, "");
    edit->setText(QString::fromStdString(saved));
    edit->setPlaceholderText(QString::fromStdString(defaults(model)));
    edit->setToolTip("Folders (globs allowed), separated by ;. Empty: " + QString::fromStdString(defaults(model)));
    edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);   // a long list of folders does not widen the tab
    line->addWidget(edit, 1);
    column->addLayout(line);
    auto * buttons = new QHBoxLayout;
    auto * browse = new QPushButton{"Add a folder...", box};
    auto * reset = new QPushButton{"Use the default", box};
    buttons->addWidget(browse);
    buttons->addWidget(reset);
    buttons->addStretch();
    column->addLayout(buttons);
    auto * found = new QLabel{box};
    found->setWordWrap(true);
    found->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    found->setMinimumHeight(24);
    column->addWidget(found);
    layout->addWidget(box);
    rows[index].folders = edit;
    rows[index].found = found;
    // saved as it is typed: a folder is looked at when the tab is built or when the text is accepted
    QObject::connect(edit, &QLineEdit::editingFinished, this, [this, index] {
        auto& row = rows[index];
        Utility::writePref(row.setting, row.folders->text().trimmed().toStdString());
        look(row);
    });
    QObject::connect(browse, &QPushButton::clicked, this, [this, index] {
        auto& row = rows[index];
        const auto folder = QFileDialog::getExistingDirectory(this, "Folder of runs", QDir::homePath());
        if (!folder.isEmpty()) {
            const auto now = row.folders->text().trimmed();
            row.folders->setText(now.isEmpty() ? folder : now + "; " + folder);
            Utility::writePref(row.setting, row.folders->text().trimmed().toStdString());
            look(row);
        }
    });
    QObject::connect(reset, &QPushButton::clicked, this, [this, index] {
        auto& row = rows[index];
        row.folders->clear();
        Utility::writePref(row.setting, "");
        look(row);
    });
}

void SettingsLocalRunsBox::look(Row& row) {
    std::vector<std::string> folders = split(row.folders->text().toStdString());
    if (folders.empty()) {
        folders = split(row.folders->placeholderText().toStdString());
    }
    const auto runs = GfsData::wrfRunsIn(folders);
    if (runs.empty()) {
        row.found->setText("No runs found in these folders.");
        return;
    }
    QString text = QString::number(runs.size()) + (runs.size() == 1 ? " run found: " : " runs found: ");
    int shown = 0;
    for (auto it = runs.rbegin(); it != runs.rend() && shown < 6; ++it, ++shown) {   // the newest first
        text += (shown == 0 ? "" : ", ") + QString::fromStdString(it->id.substr(4, 4) + " " + it->id.substr(8, 2)) + "Z (" + QString::number(it->files) + " h)";
    }
    if (runs.size() > 6) {
        text += ", ...";
    }
    row.found->setText(text);
}
