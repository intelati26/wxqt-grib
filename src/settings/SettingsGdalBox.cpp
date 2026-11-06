// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "settings/SettingsGdalBox.h"
#include <QDir>
#include <QCoreApplication>
#include <QPointer>
#include <thread>
#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QProcess>
#include <QRegularExpression>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include <tuple>
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
    auto * check = new QPushButton{"Check for a newer GDAL", box};
    check->setToolTip("Asks GitHub (api.github.com, OSGeo/gdal) for the newest release, only when pressed");
    buttons->addWidget(check);
    buttons->addStretch();
    column->addLayout(buttons);
    found = new QLabel{box};
    found->setWordWrap(true);
    found->setTextFormat(Qt::PlainText);
    found->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    found->setMinimumHeight(120);
    found->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    column->addWidget(found);
    newest = new QLabel{box};
    newest->setWordWrap(true);
    newest->setTextFormat(Qt::PlainText);
    newest->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    newest->setMinimumHeight(60);
    newest->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    column->addWidget(newest);
    QObject::connect(check, &QPushButton::clicked, this, [this] { checkNewest(); });
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
        if (qEnvironmentVariableIsSet("WXQT_GDAL_CHECK")) {   // (a development aid: presses "Check for a newer GDAL" for the screenshot)
            checkNewest();
        }
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
    const auto parsed = QRegularExpression{"GDAL (\\d+)\\.(\\d+)\\.(\\d+)"}.match(version);
    installed = parsed.hasMatch() ? parsed.captured(1) + "." + parsed.captured(2) + "." + parsed.captured(3) : QString{};
    if (!installed.isEmpty()) {
        text += "Major version: " + parsed.captured(1) + "   (release " + installed + ")\n";   // (for the eye: the program is built and tried on one major version at a time)
    }
    const auto formats = run(dir + "/gdalinfo", {"--formats"});
    const auto has = [&formats] (const QString& name) { return formats.contains(" " + name + " ") || formats.contains(" " + name + ":") || formats.contains(" " + name + " -"); };
    text += QString{"\nFormats the models need:  GRIB %1,  netCDF %2,  HDF5 %3"}.arg(has("GRIB") ? "yes" : "NO").arg(has("netCDF") ? "yes" : "NO").arg(has("HDF5") ? "yes" : "NO");
    text += "\nThe IFS and AIFS use CCSDS (AEC) compression, which a GDAL built without libaec cannot read; their charts say so when that is the case.";
    found->setText(text);
}

void SettingsGdalBox::checkNewest() {
    if (installed.isEmpty()) {
        newest->setText("The version in use is not known, so there is nothing to compare.");
        return;
    }
    newest->setText("Asking GitHub for the newest GDAL release...");
    const QString mine = installed;
    QPointer<SettingsGdalBox> self{this};   // (the tab may be gone when the answer comes: it is then not used)
    const auto show = [self, mine] (const QByteArray& bytes) {
        if (self.isNull()) {
            return;
        }
        auto * that = self.data();
        const auto tag = QJsonDocument::fromJson(bytes).object().value("tag_name").toString();   // "v3.13.3"
        const auto parsed = QRegularExpression{"^v?(\\d+)\\.(\\d+)\\.(\\d+)$"}.match(tag);
        if (!parsed.hasMatch()) {
            that->newest->setText("Could not read the newest release from GitHub (offline, or its answer changed).");
            return;
        }
        const auto have = mine.split('.');
        const int major = parsed.captured(1).toInt(), minor = parsed.captured(2).toInt(), patch = parsed.captured(3).toInt();
        const int haveMajor = have[0].toInt(), haveMinor = have[1].toInt(), havePatch = have[2].toInt();
        const QString latest = tag.startsWith('v') ? tag.mid(1) : tag;
        QString text = "Newest release: " + latest + "   In use: " + mine + "\n";
        if (major > haveMajor) {
            text += "A NEW MAJOR VERSION is out (" + QString::number(major) + ".x; yours is " + QString::number(haveMajor) + ".x). Nothing is suggested: this is left for you to look at by eye "
                    "(the program is built and tried on GDAL " + QString::number(haveMajor) + ".x; read the release notes at gdal.org first).";
        } else if (major < haveMajor || std::tie(minor, patch) < std::tie(haveMinor, havePatch)) {
            text += "The GDAL in use is newer than the newest release (a development build).";
        } else if (minor > haveMinor) {
            text += "A newer minor release is out (" + latest + "), in the same major version " + QString::number(haveMajor) + ".";
        } else if (patch > havePatch) {
            text += "A newer patch release is out (" + latest + "): bug and security fixes, same series.";
        } else {
            text += "This is the newest release.";
        }
        that->newest->setText(text);
    };
    std::thread{[show] {
        const auto bytes = URL::getBytes("https://api.github.com/repos/OSGeo/gdal/releases/latest");
        QMetaObject::invokeMethod(QCoreApplication::instance(), [show, bytes] { show(bytes); }, Qt::QueuedConnection);
    }}.detach();
}
