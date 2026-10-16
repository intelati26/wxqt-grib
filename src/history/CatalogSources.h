// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef CATALOGSOURCES_H
#define CATALOGSOURCES_H

#include <vector>
#include "history/CatalogDialog.h"

// The catalogs the History page offers: each a CatalogDialog::Source. Another kind of catalog is one more function here and one more line in all().
namespace CatalogSources {
    CatalogDialog::Source tornadoOutbreaks();   // the SPC tornado database grouped into outbreaks
    CatalogDialog::Source biggestDays();        // the biggest snow, rain, heat and cold days of the station nearest the page's point (ACIS)
    CatalogDialog::Source highRiskDays();       // the days the SPC issued a Day 1 High risk (from 2002), with what the tornado database says came of them
    CatalogDialog::Source tropicalStorms();     // the named storms of HURDAT2 (Atlantic and eastern Pacific)
    std::vector<CatalogDialog::Source> all();
}

#endif  // CATALOGSOURCES_H
