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
        std::string overrideId, text;   // an override file (`override = GFS`): the id and the file's text; it is not parsed on its own (GfsModels merges it into the built-in model)
    };
    Parsed parse(const std::string& text, const std::string& origin);
    // the definitions that are part of the program: (id, text), in the order of the registry
    const std::vector<std::pair<std::string, std::string>>& builtIn();
    // Override files (section 20 of docs/plans.md, A): a `.model` file of the person's folder with `override = GFS` (an id of a built-in model) changes only the keys it lists and leaves the rest of the
    // built-in definition. The rule is one: the file's lines of a key REPLACE the built-in lines of that key as a set (`url`, `file`, `lag_hours`, `hours` ...); `url_prepend` / `url_append` put its
    // places before / after the built-in `url` lines (the usual one: "try this server first"); a line that begins `when <condition> =>` replaces only the built-in line with the same condition.
    // Not overridable: `id`, `kind`, `label` (a credit follows the data's real source), `include` and every `clone` rule (which charts exist). The merged text is parsed as one definition by the same
    // parser, so its errors are the parser's, with the line of the MERGED text.
    // The id an override file is for ("" when the text is not one).
    std::string overrideTarget(const std::string& text);
    // The built-in definition's text with its `include` lines expanded ("" when there is no such model).
    std::string builtInText(const std::string& id);
    // The built-in text with the override's lines applied; false, with the reason (and the line of the override), when the override is not allowed.
    bool mergeOverride(const std::string& base, const std::string& over, std::string& merged, std::string& why);
    // `why = ...` and `link = kind | label | url` lines into the model's entry
    void applyAbout(GfsModels::Def& def, const std::string& text);
    // every *.model file of a folder (empty when there is none), each parsed
    std::vector<Parsed> loadFolder(const std::string& folder);
}

#endif  // GFSMODELFILE_H
