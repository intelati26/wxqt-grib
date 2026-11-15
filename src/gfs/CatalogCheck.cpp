// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "gfs/CatalogCheck.h"
#include <algorithm>
#include <set>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

std::string CatalogCheck::key(const Field& f) {
    return f.variable + "|" + f.level + "|" + f.detail + "|" + f.file;
}

std::string CatalogCheck::statusFile() {
    return (QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/servers/catalog-status.json").toStdString();
}

std::string CatalogCheck::Report::summary() const {
    if (!checked) {
        return "not checked: " + note;
    }
    return std::to_string(found.size()) + " of " + std::to_string(found.size() + gone.size() + neverFound.size()) + " fields found; " + std::to_string(gone.size()) + " gone, " + std::to_string(neverFound.size()) + " never found" +
           (firstCheck ? " (a first check: nothing to compare with)" : "") + "; " + std::to_string(notCataloged.size()) + " not cataloged";
}

CatalogCheck::Report CatalogCheck::make(const std::string& model, const std::string& run, const GfsData::Inventory& inventory, const std::vector<std::string>& foundBefore, bool hadBefore) {
    Report r;
    r.model = model;
    r.run = run;
    r.checkedAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    r.checked = inventory.checked;
    r.note = inventory.note;
    r.firstCheck = !hadBefore;
    if (!inventory.checked) {
        return r;
    }
    const std::set<std::string> before{foundBefore.begin(), foundBefore.end()};
    r.found = inventory.found;
    for (const auto& f : inventory.gone) {
        (before.count(key(f)) ? r.gone : r.neverFound).push_back(f);
    }
    r.notCataloged = inventory.notCataloged;
    return r;
}

namespace {
    QJsonArray fields(const std::vector<CatalogCheck::Field>& list, size_t limit, const std::map<std::string, std::vector<std::string>> * usedBy = nullptr) {
        QJsonArray out;
        for (size_t i = 0; i < list.size() && i < limit; i++) {
            QJsonObject o;
            if (usedBy != nullptr) {   // the charts that ask for it
                const auto found = usedBy->find(CatalogCheck::key(list[i]));
                if (found != usedBy->end()) {
                    QJsonArray charts;
                    for (size_t c = 0; c < found->second.size() && c < 8; c++) {
                        charts.append(QString::fromStdString(found->second[c]));
                    }
                    o["used_by"] = charts;
                    o["used_by_count"] = static_cast<int>(found->second.size());
                }
            }
            o["variable"] = QString::fromStdString(list[i].variable);
            o["level"] = QString::fromStdString(list[i].level);
            if (!list[i].detail.empty()) {
                o["detail"] = QString::fromStdString(list[i].detail);
            }
            if (!list[i].file.empty()) {
                o["file"] = QString::fromStdString(list[i].file);
            }
            out.append(o);
        }
        return out;
    }
}

QJsonObject CatalogCheck::toJson(const Report& r) {
    QJsonObject o;
    o["model"] = QString::fromStdString(r.model);
    o["run"] = QString::fromStdString(r.run);
    o["checked_at"] = QString::fromStdString(r.checkedAt);
    o["checked"] = r.checked;
    o["note"] = QString::fromStdString(r.note);
    o["summary"] = QString::fromStdString(r.summary());
    o["found_count"] = static_cast<int>(r.found.size());
    o["found"] = fields(r.found, 5000, &r.usedBy);
    o["gone"] = fields(r.gone, 500, &r.usedBy);
    o["never_found"] = fields(r.neverFound, 500, &r.usedBy);
    o["not_cataloged"] = fields(r.notCataloged, 2000);
    o["not_cataloged_count"] = static_cast<int>(r.notCataloged.size());
    return o;
}

std::vector<std::string> CatalogCheck::keptFound(const std::string& model, bool& had) {
    had = false;
    QFile file{QString::fromStdString(statusFile())};
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    const auto entry = QJsonDocument::fromJson(file.readAll()).object().value(QString::fromStdString(model)).toObject();
    if (entry.isEmpty()) {
        return {};
    }
    had = true;
    std::vector<std::string> out;
    for (const auto& v : entry.value("found").toArray()) {
        out.push_back(v.toString().toStdString());
    }
    return out;
}

void CatalogCheck::keep(const Report& r) {
    if (!r.checked) {
        return;   // (a check that could not be made keeps what the last one found)
    }
    QJsonObject root;
    {
        QFile file{QString::fromStdString(statusFile())};
        if (file.open(QIODevice::ReadOnly)) {
            root = QJsonDocument::fromJson(file.readAll()).object();
        }
    }
    QJsonObject entry;
    entry["run"] = QString::fromStdString(r.run);
    entry["checked_at"] = QString::fromStdString(r.checkedAt);
    QJsonArray found;
    for (const auto& f : r.found) {
        found.append(QString::fromStdString(key(f)));
    }
    // a field that is gone stays in the kept list until it has been missing for a check of its own: it is the way the next check still calls it gone and not never found
    for (const auto& f : r.gone) {
        found.append(QString::fromStdString(key(f)));
    }
    entry["found"] = found;
    root[QString::fromStdString(r.model)] = entry;
    QDir{}.mkpath(QString::fromStdString(statusFile()).section('/', 0, -2));
    QSaveFile out{QString::fromStdString(statusFile())};
    if (out.open(QIODevice::WriteOnly)) {
        out.write(QJsonDocument{root}.toJson(QJsonDocument::Compact));
        out.commit();
    }
}
