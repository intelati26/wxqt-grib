// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityTools.h"
#include <QCoreApplication>
#include <QStandardPaths>

QString UtilityTools::folder() {
    return QCoreApplication::applicationDirPath() + "/tools";
}

QString UtilityTools::find(const QString& name) {
    const auto local = QStandardPaths::findExecutable(name, {folder(), QCoreApplication::applicationDirPath()});
    if (!local.isEmpty()) {
        return local;
    }
    return QStandardPaths::findExecutable(name);
}
