// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CATALOGCHECK_H
#define CATALOGCHECK_H

#include <map>
#include <string>
#include <vector>
#include <QJsonObject>
#include "gfs/GfsData.h"

// The product catalogue against what a model's files really list (section 20 of docs/plans.md, "Gone or not cataloged"): every field the model's charts ask for, and every record the newest file lists.
//   found         wanted and listed
//   gone          wanted, not listed, and listed at the last check: a rename or a removal, the signal
//   neverFound    wanted, not listed, and never listed here (a chart that has never worked on this model; all of them at a first check)
//   notCataloged  listed, and no chart asks for it: information, and the list of what the model offers that the program does not draw
// The last check's found fields are kept (`catalog-status.json` in the program's data folder) so the next one can tell gone from never found.
namespace CatalogCheck {
    using Field = GfsData::InventoryField;
    struct Report {
        std::string model, label, run, checkedAt, note;   // note: why nothing was checked (empty when it was)
        bool checked{false}, firstCheck{false};
        std::vector<Field> found, gone, neverFound, notCataloged;
        std::map<std::string, std::vector<std::string>> usedBy;   // key(field) -> the ids of the charts that ask for it (found, gone and never found fields)
        std::string summary() const;   // "212 of 214 fields found; 1 gone, 1 never found; 480 not cataloged"
    };
    // The report of an inventory: the previous status of `model` (the fields found last time) decides what is gone and what was never found.
    Report make(const std::string& model, const std::string& run, const GfsData::Inventory& inventory, const std::vector<std::string>& foundBefore, bool hadBefore);
    QJsonObject toJson(const Report&);
    // The kept status: "variable|level|detail|file" of the fields found at the last check of each model.
    std::vector<std::string> keptFound(const std::string& model, bool& had);
    void keep(const Report&);
    std::string key(const Field&);
    std::string statusFile();
}

#endif  // CATALOGCHECK_H
