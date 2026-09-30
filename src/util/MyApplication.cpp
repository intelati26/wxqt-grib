// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "MyApplication.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "common/GlobalVariables.h"
#include "radar/Metar.h"
#include "radar/RadarSites.h"
#include "settings/Location.h"
#include "settings/RadarPreferences.h"
#include "settings/UIPreferences.h"
#include "settings/UtilityStorePreferences.h"
#include "util/SoundingSites.h"
#include "util/WfoSites.h"

QSettings * MyApplication::preferences;

#ifdef Q_OS_WIN
namespace {
    // Windows only: QSettings would otherwise write to the registry, which
    // defeats a portable folder install. Use an INI file next to the exe
    // when that folder is writable, else the per-user INI location. Settings
    // already in the registry are copied over once, the first time the INI
    // is created, so nothing is orphaned.
    QSettings * makeWindowsSettings(const QString& org, const QString& app) {
        const auto portablePath = QCoreApplication::applicationDirPath() + "/" + app + ".ini";
        const auto portableWritable = QFileInfo::exists(portablePath)
            ? QFileInfo{portablePath}.isWritable()
            : QFileInfo{QCoreApplication::applicationDirPath()}.isWritable();
        QSettings * settings = portableWritable
            ? new QSettings{portablePath, QSettings::IniFormat}
            : new QSettings{QSettings::IniFormat, QSettings::UserScope, org, app};
        if (!QFileInfo::exists(settings->fileName())) {
            const QSettings registry{QSettings::NativeFormat, QSettings::UserScope, org, app};
            for (const auto& key : registry.allKeys()) {
                settings->setValue(key, registry.value(key));
            }
            settings->sync();
        }
        return settings;
    }
}
#endif

void MyApplication::onCreate() {
    const auto org = QString::fromStdString(GlobalVariables::appOrgName);
    const auto app = QString::fromStdString(GlobalVariables::appName);
#ifdef Q_OS_WIN
    preferences = makeWindowsSettings(org, app);
#else
    preferences = new QSettings{org, app};
#endif
    UtilityStorePreferences::setDefaults();
    SoundingSites::initialize();
    WfoSites::initialize();
    RadarSites::initialize();
    Metar::initialize();
    Location::refresh();
    RadarPreferences::initialize();
    UIPreferences::initialize();
}
