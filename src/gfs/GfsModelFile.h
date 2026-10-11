// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef GFSMODELFILE_H
#define GFSMODELFILE_H

#include <string>
#include <utility>
#include <vector>
#include "gfs/GfsModels.h"

// A model written as a text file: where its files are, when they appear, what is in them and which of the GFS charts it can draw. The built-in models that fit this are defined this way (the
// text is in GfsBuiltInModels.cpp), and a file in the user's models folder adds one without a build. See docs/model-files.md for the keys.
//
//   id = GEPS
//   label = ECCC GEPS 21 member ensemble, 0.5 degree
//   kind = whole                       (grib: an index beside each file; whole: one field per file; netcdf: variables read by range)
//   url = https://example.org/{date}/{cycle}/{file}_P{hour3}.grib2      (more `url` lines: other places to look, in order)
//   file = when level~"^[0-9]+ mb$" => {variable}_ISBL_{mb4}              (which file holds a want; the last line without `when` is the default)
//   cycle_hours = 12 ...
namespace GfsModelFile {
    struct Parsed {
        GfsModels::Def def;
        bool ok{false};
        std::string error;     // "line 12: ..." when not ok
        std::string origin;    // the file name, or "built in"
    };
    Parsed parse(const std::string& text, const std::string& origin);
    // the definitions that are part of the program: (id, text), in the order of the registry
    const std::vector<std::pair<std::string, std::string>>& builtIn();
    // what the built-in models are and why they matter (`why` and `link` lines), for every built-in model including those that are code
    const std::vector<std::pair<std::string, std::string>>& aboutBuiltIn();
    // `why = ...` and `link = kind | label | url` lines into the model's entry
    void applyAbout(GfsModels::Def& def, const std::string& text);
    // every *.model file of a folder (empty when there is none), each parsed
    std::vector<Parsed> loadFolder(const std::string& folder);
}

#endif  // GFSMODELFILE_H
