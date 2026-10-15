// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef SURFACESTATIONTEXT_H
#define SURFACESTATIONTEXT_H

#include <QString>
#include "obs/SurfaceStation.h"

// The words about one surface station: the hover text and the whole card.
namespace SurfaceStationText {
    QString summary(const SurfaceStation&, bool fahrenheit);
    QString details(const SurfaceStation&);
}

#endif  // SURFACESTATIONTEXT_H
