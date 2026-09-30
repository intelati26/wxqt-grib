// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYANIMATIONEXPORT_H
#define UTILITYANIMATIONEXPORT_H

#include <functional>
#include <vector>
#include <QByteArray>
#include <QImage>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QWidget>

using std::vector;

// One place for "save this animation (or still image) as ...", used by every
// screen with a play/save bar. The file type is chosen in the Save dialog:
//   always available (built in, no external tools): Animated PNG
//   (GIF is deliberately not offered: 256 colours and 1-bit transparency)
//   if installed:  JPEG XL (cjxl), AVIF (avifenc), animated WebP and MP4 (ffmpeg)
// A format whose tool is missing is simply not offered, and any encode/write
// failure is reported to the user in a message box - nothing is silently
// replaced by a different format any more.
class UtilityAnimationExport {
public:
    struct Format {
        QString id;      // "apng", "jxl", "avif", "webp", "mp4", "png"
        QString label;   // shown in the dialog's file-type list
        QString ext;     // without the dot
    };

    // formats offered for an animation (animated == true) or a single image
    static vector<Format> formats(bool animated);

    // Save dialog + encode + error report. `frames` are encoded images
    // (PNG/JPEG/GIF bytes) in playback order; with fewer than two, `still` is
    // exported as a single image instead. Returns true only if a file was written.
    static bool saveWithDialog(QWidget * parent, const vector<QByteArray>& frames, int frameDelayMs,
                               const QByteArray& still, const QString& baseName);

    // The encoders themselves (also usable without a dialog). On failure returns
    // false with a human-readable reason in `error`.
    // Gives a picture label a right-click "Save image..." that exports the
    // source bytes last stored on it with setSourceBytes() (through
    // saveWithDialog, so the same format choice and visible error reporting).
    // The bytes live on the label itself - not in a captured owner pointer -
    // because Photo / Image objects are moved around inside vectors.
    // Nothing is offered while there are no source bytes (icons etc.).
    static void installContextSave(QLabel * label);
    static void setSourceBytes(QLabel * label, const QByteArray& bytes);

    // Explains how to add the optional formats (JPEG XL, AVIF, WebP, MP4) that
    // are not bundled: install commands for the current system, download
    // links, and a button that opens the "tools" folder next to the program
    // where the files can simply be dropped in. `missing` = the tool names
    // not found (cjxl / avifenc / ffmpeg); only those are described.
    static void showInstallHelp(QWidget * parent, const QStringList& missing);

    static bool encodeToFile(const Format& format, const vector<QByteArray>& frames, int frameDelayMs,
                             const QByteArray& still, const QString& path, QString& error);

private:
    static bool toolAvailable(const QString& name);
};

#endif  // UTILITYANIMATIONEXPORT_H
