// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityAnimationExport.h"
#include <algorithm>
#include <string>
#include <QBuffer>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include "objects/UtilityApng.h"
#include "objects/UtilityJxl.h"
#include "util/Utility.h"

namespace {
    // an image flattened onto white as opaque RGB32
    QImage flattened(const QImage& source, const QSize& size) {
        QImage canvas{size, QImage::Format_RGB32};
        canvas.fill(Qt::white);
        QPainter painter{&canvas};
        painter.drawImage(QRect{QPoint{0, 0}, size}, source);
        painter.end();
        return canvas;
    }

    // ---- external encoders ------------------------------------------------------

    struct RunResult {
        bool ok;
        QString error;
    };

    RunResult runTool(const QString& program, const QStringList& args, int timeoutMs) {
        QProcess process;
        process.start(program, args);
        if (!process.waitForStarted(10000)) {
            return {false, program + " could not be started"};
        }
        if (!process.waitForFinished(timeoutMs)) {
            process.kill();
            return {false, program + " did not finish in time"};
        }
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
            const auto text = QString::fromUtf8(process.readAllStandardError() + process.readAllStandardOutput())
                .trimmed().right(400);
            return {false, program + " failed" + (text.isEmpty() ? QString{} : ": " + text)};
        }
        return {true, {}};
    }

    // writes the frames as numbered PNGs in a temp dir (all the same size)
    bool writeFramePngs(const vector<QImage>& images, const QString& dirPath, bool flatten, QString& error) {
        if (images.empty()) {
            error = "there are no frames to export";
            return false;
        }
        const auto size = images.front().size();
        for (size_t i = 0; i < images.size(); i += 1) {
            const auto name = dirPath + "/frame_" + QString::number(i).rightJustified(4, '0') + ".png";
            const auto frame = flatten ? flattened(images[i], size)
                : (images[i].size() == size ? images[i]
                                            : images[i].scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            if (!frame.save(name, "PNG")) {
                error = "could not write a temporary frame file";
                return false;
            }
        }
        return true;
    }

    vector<QImage> decode(const vector<QByteArray>& frames) {
        vector<QImage> images;
        for (const auto& bytes : frames) {
            const auto image = QImage::fromData(bytes);
            if (!image.isNull()) {
                images.push_back(image);
            }
        }
        return images;
    }
}

bool UtilityAnimationExport::toolAvailable(const QString& name) {
    return !QStandardPaths::findExecutable(name).isEmpty();
}

vector<UtilityAnimationExport::Format> UtilityAnimationExport::formats(bool animated) {
    vector<Format> list;
    if (animated) {
        list.push_back({"apng", "Animated PNG", "png"});
    } else {
        list.push_back({"png", "PNG image", "png"});
    }
    if (UtilityJxl::available()) {
        list.push_back({"jxl", animated ? "JPEG XL animation" : "JPEG XL image", "jxl"});
    }
    if (toolAvailable("avifenc")) {
        list.push_back({"avif", animated ? "AVIF animation" : "AVIF image", "avif"});
    }
    if (toolAvailable("ffmpeg")) {
        list.push_back({"webp", animated ? "Animated WebP" : "WebP image", "webp"});
        if (animated) {
            list.push_back({"mp4", "MP4 video (H.264)", "mp4"});
        }
    }
    return list;
}

bool UtilityAnimationExport::encodeToFile(const Format& format, const vector<QByteArray>& frames, int frameDelayMs,
                                          const QByteArray& still, const QString& path, QString& error) {
    error.clear();
    const bool animated = frames.size() >= 2;
    const auto images = decode(frames);
    if (animated && images.size() < 2) {
        error = "the frames could not be decoded";
        return false;
    }
    auto writeBytes = [&] (const QByteArray& bytes) {
        if (bytes.isEmpty()) {
            error = "nothing was produced to save";
            return false;
        }
        QFile file{path};
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            error = "could not write " + path + ": " + file.errorString();
            return false;
        }
        return true;
    };
    // the single image as PNG bytes (for tools that want a file to read)
    auto stillPng = [&] {
        QImage image = QImage::fromData(still);
        if (image.isNull() && !images.empty()) {
            image = images.front();
        }
        QByteArray png;
        QBuffer buffer{&png};
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
        return png;
    };

    if (format.id == "apng") {
        return writeBytes(UtilityApng::fromFrames(frames, frameDelayMs));
    }
    if (format.id == "png") {
        return writeBytes(stillPng());
    }
    if (format.id == "jxl") {
        const auto input = animated ? UtilityApng::fromFrames(frames, frameDelayMs) : stillPng();
        if (input.isEmpty()) {
            error = "could not prepare the image for JPEG XL";
            return false;
        }
        QString reason;
        if (!UtilityJxl::encode(input, path, true, &reason)) {
            error = reason.isEmpty() ? QString{"cjxl failed to write the JPEG XL file"} : reason;
            return false;
        }
        return true;
    }

    // avif / webp / mp4: frames go to a temp dir as numbered PNGs
    QTemporaryDir temp;
    if (!temp.isValid()) {
        error = "could not create a temporary folder";
        return false;
    }
    const vector<QImage> source = animated ? images : vector<QImage>{QImage::fromData(stillPng())};
    QString writeError;
    if (!writeFramePngs(source, temp.path(), format.id == "mp4", writeError)) {
        error = writeError;
        return false;
    }
    QFile::remove(path);
    if (format.id == "avif") {
        QStringList args{"-q", "80", "-s", "6", "-j", "all"};
        if (animated) {
            args << "--timescale" << "1000" << "--duration" << QString::number(frameDelayMs);
        }
        for (size_t i = 0; i < source.size(); i += 1) {
            args << temp.path() + "/frame_" + QString::number(i).rightJustified(4, '0') + ".png";
        }
        args << path;
        const auto result = runTool("avifenc", args, 300000);
        error = result.error;
        return result.ok && QFile::exists(path);
    }
    if (format.id == "webp" || format.id == "mp4") {
        const auto rate = QString::number(1000.0 / std::max(1, frameDelayMs), 'f', 3);
        QStringList args{"-y", "-hide_banner", "-loglevel", "error"};
        if (animated) {
            args << "-framerate" << rate;
        }
        args << "-i" << temp.path() + "/frame_%04d.png";
        if (format.id == "webp") {
            args << "-c:v" << "libwebp_anim" << "-quality" << "90" << "-loop" << "0";
        } else {
            args << "-c:v" << "libx264" << "-crf" << "18" << "-pix_fmt" << "yuv420p"
                 << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2:color=white";
        }
        args << path;
        const auto result = runTool("ffmpeg", args, 300000);
        error = result.error;
        return result.ok && QFile::exists(path);
    }
    error = "unknown export format: " + format.id;
    return false;
}

bool UtilityAnimationExport::saveWithDialog(QWidget * parent, const vector<QByteArray>& frames, int frameDelayMs,
                                            const QByteArray& still, const QString& baseName) {
    const bool animated = frames.size() >= 2;
    if (!animated && still.isEmpty() && frames.empty()) {
        QMessageBox::information(parent, "Nothing to save", "There is no image to save yet.");
        return false;
    }
    const auto list = formats(animated);

    // dialog entries, the most recently used format first
    const std::string prefKey = animated ? "ANIM_EXPORT_FORMAT" : "STILL_EXPORT_FORMAT";
    const auto preferred = QString::fromStdString(Utility::readPref(prefKey, animated ? "jxl" : "png"));
    vector<Format> ordered;
    for (const auto& format : list) {
        if (format.id == preferred) {
            ordered.push_back(format);
        }
    }
    for (const auto& format : list) {
        if (format.id != preferred) {
            ordered.push_back(format);
        }
    }
    QStringList filters;
    for (const auto& format : ordered) {
        filters << format.label + " (*." + format.ext + ")";
    }
    auto caption = QString{animated ? "Save animation" : "Save image"};
    QStringList missing;
    if (!UtilityJxl::available()) {
        missing << "JPEG XL needs cjxl";
    }
    if (!toolAvailable("avifenc")) {
        missing << "AVIF needs avifenc";
    }
    if (!toolAvailable("ffmpeg")) {
        missing << QString{animated ? "WebP/MP4" : "WebP"} + " need ffmpeg";
    }
    if (!missing.isEmpty()) {
        caption += "  (not installed: " + missing.join("; ") + ")";
    }

    const auto picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto stem = picturesDir.isEmpty() ? baseName : picturesDir + "/" + baseName;
    QString selectedFilter = filters.first();
    const auto fileName = QFileDialog::getSaveFileName(parent, caption, stem + "." + ordered.front().ext,
                                                       filters.join(";;"), &selectedFilter);
    if (fileName.isEmpty()) {
        return false;   // the user cancelled
    }

    // the chosen file-type filter decides the format, unless the typed
    // extension clearly names a different available one
    Format chosen = ordered.front();
    for (size_t i = 0; i < ordered.size(); i += 1) {
        if (selectedFilter == filters[static_cast<int>(i)]) {
            chosen = ordered[i];
        }
    }
    const auto typedExtension = QFileInfo{fileName}.suffix().toLower();
    if (!typedExtension.isEmpty() && typedExtension != chosen.ext) {
        for (const auto& format : ordered) {
            if (format.ext == typedExtension) {
                chosen = format;
                break;
            }
        }
    }
    auto path = fileName;
    if (QFileInfo{path}.suffix().toLower() != chosen.ext) {
        path += "." + chosen.ext;
    }

    QString error;
    if (!encodeToFile(chosen, frames, frameDelayMs, still, path, error)) {
        QFile::remove(path);
        QMessageBox::warning(parent, "Export failed",
                             "Could not save as " + chosen.label + ":\n\n" + error);
        return false;
    }
    Utility::writePref(prefKey, chosen.id.toStdString());
    return true;
}
