// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#ifndef UTILITYANIMATIONEXPORT_H
#define UTILITYANIMATIONEXPORT_H

#include <functional>
#include <string>
#include <vector>
#include <QByteArray>
#include <QDateTime>
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
//   if installed:  JPEG XL (cjxl), AVIF (avifenc), WebP (img2webp - bundled
//                  with the portable packages - else ffmpeg), MP4 (ffmpeg)
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
    //
    // The default file name is "<yyyyMMdd_HHmm>Z_<baseName>" when the picture
    // was downloaded here and the server said when it was produced (see
    // datedName); `metaBytes` names which bytes to look that up for (default:
    // the still image, else the newest frame). Rendered images that have no
    // download record keep `baseName` as given (it already carries run / hour).
    // `datePrefix` = false skips that lookup (the caller already put the time in
    // baseName, e.g. via modelName() / validName()). When both a loop and the
    // current image exist the user is asked which to save; `stillBaseName` (if
    // given) names the single image, since `baseName` then describes the whole loop.
    static bool saveWithDialog(QWidget * parent, const vector<QByteArray>& frames, int frameDelayMs,
                               const QByteArray& still, const QString& baseName,
                               const QByteArray& metaBytes = QByteArray{}, bool datePrefix = true,
                               const QString& stillBaseName = QString{});

    // File-name building for images WE render from model runs (RRFS, REFS,
    // SHIP/STP, SPC Post): "<run date>_<cycle>z_f<hour>_v<valid time>Z_<product>",
    // e.g. "20260930_06z_f012_v20260930_1800Z_rrfs_tmp2m_conus". The run, hour(s)
    // are read from the screen's own status lines ("... 2026-09-30 06z    F12
    // valid ..." or a max composite's "... F001-F030 (24 hrs)"); the valid time is
    // run + hour in UTC. For a loaded animation pass the first and last frame's
    // status: the hour becomes "f001-f024" and the valid time a range. Falls back
    // to `product` alone if a status can't be read.
    static QString modelName(const QString& firstStatus, const QString& lastStatus, const QString& product);
    // "<first>Z[-<last>Z]_<product>" from explicit valid times (analyses such as
    // RTMA that have no model run)
    static QString validName(const QDateTime& first, const QDateTime& last, const QString& product);
    // For images WE render from model runs: returns the PNG with the SPC-style
    // information bars (model, product, region, run, forecast hour, valid time)
    // drawn over its top and bottom edges, for saving. Everything is read from the
    // screen's status line ("RRFS 2026-09-30 06z    F12 valid ...    2m Temperature    CONUS"),
    // so each frame of an animation gets its own hour. `units` is appended to the
    // product name. Returns the bytes unchanged if the status can't be read.
    static QByteArray withHeader(const QByteArray& imageBytes, const QString& status, const QString& units = QString{});
    // the same for every frame of an animation, one status per frame
    static vector<QByteArray> withHeaders(const vector<QByteArray>& frames, const vector<std::string>& statuses,
                                          const QString& units = QString{});

    // lower-case, a-z0-9 and single underscores only, at most `length` characters
    static QString slug(const QString& text, int length);

    // "20260930_1639Z_product" (UTC) from the download metadata recorded for
    // `bytes` (server Last-Modified, else the time we fetched it); just
    // `product` if nothing was recorded.
    static QString datedName(const QString& product, const QByteArray& bytes);
    // "Updated 2026-09-30 16:39 UTC" for `bytes`, or an empty string
    static QString updatedText(const QByteArray& bytes);

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

    // WebP encoder settings, asked for in the Save flow (webpOptionsDialog)
    // and remembered between saves - except the frame delay, which starts
    // from the animation's current speed each time. encodeToFile uses the
    // ones last confirmed in the dialog (the saved ones before that).
    struct WebpOptions {
        enum Mode { Lossy, Lossless, Mixed };
        Mode mode{Lossy};
        int quality{90};         // 0-100 (lossy / mixed)
        int effort{4};           // 0-6, higher = smaller but slower
        int frameDelayMs{0};     // 0 = the animation's own speed
        int lastFrameHoldMs{0};  // extra pause on the last frame, 0 = none
        int loopCount{0};        // 0 = forever
        bool sharpYuv{false};    // sharper colour edges for lossy (slower)
    };
    static WebpOptions savedWebpOptions();
    // the options an export started now would use (confirmed ones, else the saved ones)
    static WebpOptions webpOptionsInUse();
    // encodeToFile for a background worker: the WebP options are the given ones (a snapshot taken when the job was queued), not
    // whatever a later Save dialog has confirmed since. Safe to call off the UI thread.
    static bool encodeToFileWith(const WebpOptions&, const Format&, const vector<QByteArray>& frames, int frameDelayMs,
                                 const QByteArray& still, const QString& path, QString& error);
    // false if the user cancelled; on true `options` is filled and remembered
    static bool webpOptionsDialog(QWidget * parent, bool animated, int frameDelayMs, WebpOptions& options);

private:
    static bool toolAvailable(const QString& name);
    static bool webpAvailable();
    static bool encodeWebp(size_t frameCount, bool animated, int frameDelayMs, const QString& framesDir,
                           const QString& path, QString& error);
    static WebpOptions webpOptions;
    static bool webpOptionsSet;
};

#endif  // UTILITYANIMATIONEXPORT_H
