// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYJXL_H
#define UTILITYJXL_H

#include <string>
#include <QByteArray>
#include <QString>

using std::string;

// Shells out to the system "cjxl" (libjxl) to turn PNG/APNG/GIF/JPEG bytes -
// as already produced by UtilityApng or any of the app's own PNG renders -
// into JPEG XL, lossless by default. An APNG input produces an animated JXL
// automatically (cjxl reads every frame + its delay), so this is a drop-in
// replacement for a plain PNG/APNG save wherever cjxl is available.
class UtilityJxl {
public:
    static bool available();

    // Encodes `bytes` (any format QImage/cjxl can read) to a JPEG XL file at
    // outPath. lossless=true uses "-d 0" (mathematically lossless); false
    // uses "-q 90" (visually lossless, smaller). Returns false on any
    // failure (cjxl missing, bad input, process error) - outPath is left
    // untouched in that case.
    static bool encode(const QByteArray& bytes, const QString& outPath, bool lossless = true);

    // ".jxl" when cjxl is available, else a sensible extension sniffed from
    // `fallbackBytes`'s actual format (for building a save-dialog suggestion).
    static string preferredExtension(const QByteArray& fallbackBytes);

    // One-shot save: JPEG-XL-encodes `bytes` to `path` if cjxl is available,
    // otherwise writes `bytes` out as-is, correcting the extension (from
    // ".jxl" to whatever `bytes` actually is) so the file is never mislabeled.
    // Returns the path actually written.
    static QString save(const QByteArray& bytes, QString path);

private:
    static string cjxlPath();
    static string cacheDir();
};

#endif  // UTILITYJXL_H
