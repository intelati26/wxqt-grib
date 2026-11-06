// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsGdalBox.h"
#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include "models/UtilityGrib.h"
#include "util/Utility.h"

namespace {
    // what a GDAL program prints for these arguments ("" when it did not run)
    QString run(const QString& program, const QStringList& arguments) {
        QProcess process;
        process.start(program, arguments);
        if (!process.waitForFinished(15000)) {
            process.kill();
            return {};
        }
        return QString::fromUtf8(process.readAllStandardOutput());
    }
}

SettingsGdalBox::SettingsGdalBox(QWidget * parent)
    : QWidget{parent}
{
    setMaximumWidth(900);
    auto * layout = new QVBoxLayout{this};
    auto * about = new QLabel{"GDAL decodes the model files (GRIB, NetCDF, HDF5). The program looks for its tools in the folder `gdal` next to it (the Windows package has one), then on the PATH. To use another install, give "
                              "the folder that holds gdalwarp: on Windows the bin folder of OSGeo4W (C:\\OSGeo4W\\bin), of QGIS (...\\QGIS 3.xx\\bin) or of a conda environment (...\\Library\\bin). The folders of GDAL's and "
                              "PROJ's tables are taken from beside it (share\\gdal, share\\proj). Empty is what the program finds by itself.", this};
    about->setWordWrap(true);
    about->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    about->setMinimumHeight(90);
    layout->addWidget(about);
    auto * box = new QGroupBox{"Folder of the GDAL programs", this};
    auto * column = new QVBoxLayout{box};
    folder = new QLineEdit{box};
    folder->setText(QString::fromStdString(Utility::readPref("GDAL_BIN_FOLDER", "")));
    folder->setPlaceholderText("the program's own GDAL, then the PATH");
    folder->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    column->addWidget(folder);
    auto * buttons = new QHBoxLayout;
    auto * browse = new QPushButton{"Choose the folder...", box};
    auto * reset = new QPushButton{"Use the default", box};
    buttons->addWidget(browse);
    buttons->addWidget(reset);
    buttons->addStretch();
    column->addLayout(buttons);
    found = new QLabel{box};
    found->setWordWrap(true);
    found->setTextFormat(Qt::PlainText);
    found->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    found->setMinimumHeight(120);
    found->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    column->addWidget(found);
    layout->addWidget(box);
    layout->addStretch();
    const auto save = [this] {
        Utility::writePref("GDAL_BIN_FOLDER", folder->text().trimmed().toStdString());
        UtilityGrib::forgetGdal();   // the next use looks again
        look();
    };
    QObject::connect(folder, &QLineEdit::editingFinished, this, save);
    QObject::connect(browse, &QPushButton::clicked, this, [this, save] {
        const auto chosen = QFileDialog::getExistingDirectory(this, "Folder of gdalwarp", QDir::homePath());
        if (!chosen.isEmpty()) {
            folder->setText(QDir::toNativeSeparators(chosen));
            save();
        }
    });
    QObject::connect(reset, &QPushButton::clicked, this, [this, save] {
        folder->clear();
        save();
    });
}

void SettingsGdalBox::showEvent(QShowEvent * event) {
    QWidget::showEvent(event);
    if (!shown) {   // (asked when the tab is first seen: starting the programs takes a moment)
        shown = true;
        look();
    }
}

void SettingsGdalBox::look() {
    const QString asked = folder->text().trimmed();
    const auto dir = QString::fromStdString(UtilityGrib::gdalBinDir());
    QString text;
    if (!asked.isEmpty() && QDir::cleanPath(dir) != QDir::cleanPath(asked)) {
        text += "There is no gdalwarp in " + asked + ", so the default is in use.\n\n";
    }
    if (dir.isEmpty()) {
        found->setText(text + "No GDAL found: the model charts need it. Install it (Linux: the gdal package), or on Windows choose the bin folder of an OSGeo4W, QGIS or conda install.");
        return;
    }
    const auto version = run(dir + "/gdalinfo", {"--version"}).trimmed();
    text += "In use: " + QDir::toNativeSeparators(dir) + "\n" + (version.isEmpty() ? QString{"gdalinfo did not run."} : version) + "\n";
    const auto formats = run(dir + "/gdalinfo", {"--formats"});
    const auto has = [&formats] (const QString& name) { return formats.contains(" " + name + " ") || formats.contains(" " + name + ":") || formats.contains(" " + name + " -"); };
    text += QString{"\nFormats the models need:  GRIB %1,  netCDF %2,  HDF5 %3"}.arg(has("GRIB") ? "yes" : "NO").arg(has("netCDF") ? "yes" : "NO").arg(has("HDF5") ? "yes" : "NO");
    text += "\nThe IFS and AIFS use CCSDS (AEC) compression, which a GDAL built without libaec cannot read; their charts say so when that is the case.";
    found->setText(text);
}
