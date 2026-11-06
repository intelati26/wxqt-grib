// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SETTINGSGDALBOX_H
#define SETTINGSGDALBOX_H

#include <QWidget>

class QLabel;
class QLineEdit;

// The "GDAL" tab of the settings: the folder of the GDAL programs (gdalwarp, gdal_translate, gdalinfo) that decode the model files. Empty is what the program finds by itself (the folder `gdal` next to
// the program in the Windows package, then the PATH). Windows users can point it at an OSGeo4W, QGIS or conda install. It says which GDAL is in use and which of the formats the models need it has.
class SettingsGdalBox : public QWidget {
public:
    explicit SettingsGdalBox(QWidget * parent = nullptr);

private:
    void showEvent(QShowEvent *) override;
    void look();
    void checkNewest();
    QLineEdit * folder{};
    QLabel * found{};
    QLabel * newest{};
    QString installed;   // "3.13.3" of the GDAL in use, "" when unknown
    bool shown{false};
};

#endif  // SETTINGSGDALBOX_H
