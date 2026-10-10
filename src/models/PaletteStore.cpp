// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "models/PaletteStore.h"
#include <mutex>
#include <QJsonDocument>
#include <QJsonObject>
#include "util/Utility.h"

namespace {
    const char * const pref = "MODEL_PALETTES";
    std::mutex mutex;
    bool loaded{false};
    std::map<std::string, GfsChart::Ramp> kept;

    void load() {   // under the lock
        if (loaded) {
            return;
        }
        loaded = true;
        const auto document = QJsonDocument::fromJson(QByteArray::fromStdString(Utility::readPref(pref, "{}")));
        const auto object = document.object();
        for (auto it = object.begin(); it != object.end(); ++it) {
            GfsChart::Ramp ramp;
            std::string error;
            if (GfsChart::rampFromText(it.value().toString().toStdString(), ramp, error)) {
                kept[it.key().toStdString()] = std::move(ramp);
            }
        }
    }

    void save() {   // under the lock
        QJsonObject object;
        for (const auto& [key, ramp] : kept) {
            object[QString::fromStdString(key)] = QString::fromStdString(GfsChart::rampToText(ramp));
        }
        Utility::writePref(pref, QJsonDocument{object}.toJson(QJsonDocument::Compact).toStdString());
    }
}

std::map<std::string, GfsChart::Ramp> PaletteStore::all() {
    const std::lock_guard lock{mutex};
    load();
    return kept;
}

bool PaletteStore::has(const std::string& key) {
    const std::lock_guard lock{mutex};
    load();
    return kept.count(key) > 0;
}

void PaletteStore::set(const std::string& key, const GfsChart::Ramp& ramp) {
    const std::lock_guard lock{mutex};
    load();
    kept[key] = ramp;
    save();
}

void PaletteStore::clear(const std::string& key) {
    const std::lock_guard lock{mutex};
    load();
    if (kept.erase(key) > 0) {
        save();
    }
}
