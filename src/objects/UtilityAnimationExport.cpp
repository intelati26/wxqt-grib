// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityAnimationExport.h"
#include <algorithm>
#include <string>
#include <QBuffer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QRegularExpression>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimeZone>
#include <QStringList>
#include <QTemporaryDir>
#include <QUrl>
#include "models/UtilityGrib.h"
#include "objects/URL.h"
#include "objects/UtilityApng.h"
#include "objects/UtilityJxl.h"
#include "objects/UtilityTools.h"
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

UtilityAnimationExport::WebpOptions UtilityAnimationExport::webpOptions;
bool UtilityAnimationExport::webpOptionsSet{false};

bool UtilityAnimationExport::toolAvailable(const QString& name) {
    return !UtilityTools::find(name).isEmpty();
}

bool UtilityAnimationExport::webpAvailable() {
    return toolAvailable("img2webp") || toolAvailable("ffmpeg");
}

UtilityAnimationExport::WebpOptions UtilityAnimationExport::savedWebpOptions() {
    WebpOptions options;
    options.mode = static_cast<WebpOptions::Mode>(std::clamp(Utility::readPrefInt("WEBP_EXPORT_MODE", options.mode), 0, 2));
    options.quality = std::clamp(Utility::readPrefInt("WEBP_EXPORT_QUALITY", options.quality), 0, 100);
    options.effort = std::clamp(Utility::readPrefInt("WEBP_EXPORT_EFFORT", options.effort), 0, 6);
    options.lastFrameHoldMs = std::clamp(Utility::readPrefInt("WEBP_EXPORT_LAST_HOLD", options.lastFrameHoldMs), 0, 10000);
    options.loopCount = std::clamp(Utility::readPrefInt("WEBP_EXPORT_LOOP", options.loopCount), 0, 100);
    options.sharpYuv = Utility::readPrefInt("WEBP_EXPORT_SHARP_YUV", options.sharpYuv ? 1 : 0) != 0;
    return options;
}

bool UtilityAnimationExport::webpOptionsDialog(QWidget * parent, bool animated, int frameDelayMs, WebpOptions& options) {
    options = savedWebpOptions();
    QDialog dialog{parent};
    dialog.setWindowTitle("WebP options");
    auto * form = new QFormLayout{&dialog};

    auto * mode = new QComboBox{&dialog};
    mode->addItems({"Lossy (smallest)", "Lossless (exact pixels)", "Mixed (best of both per frame)"});
    mode->setCurrentIndex(options.mode);
    form->addRow("Compression", mode);

    auto * quality = new QSpinBox{&dialog};
    quality->setRange(0, 100);
    quality->setValue(options.quality);
    quality->setToolTip("Higher keeps more detail and makes a bigger file");
    form->addRow("Quality", quality);

    auto * effort = new QSpinBox{&dialog};
    effort->setRange(0, 6);
    effort->setValue(options.effort);
    effort->setToolTip("0 = fastest, 6 = smallest file (slowest)");
    form->addRow("Effort (0-6)", effort);

    auto * sharp = new QCheckBox{"Sharper colour edges (slower)", &dialog};
    sharp->setChecked(options.sharpYuv);
    sharp->setToolTip("Keeps thin coloured lines and text crisp in lossy mode");
    form->addRow("", sharp);

    QSpinBox * delay = nullptr;
    QSpinBox * hold = nullptr;
    QSpinBox * loop = nullptr;
    if (animated) {
        delay = new QSpinBox{&dialog};
        delay->setRange(20, 10000);
        delay->setSingleStep(50);
        delay->setSuffix(" ms");
        delay->setValue(std::clamp(frameDelayMs, 20, 10000));
        form->addRow("Frame delay", delay);

        hold = new QSpinBox{&dialog};
        hold->setRange(0, 10000);
        hold->setSingleStep(250);
        hold->setSuffix(" ms");
        hold->setSpecialValueText("None");
        hold->setValue(options.lastFrameHoldMs);
        hold->setToolTip("Extra pause on the last frame before the loop starts over");
        form->addRow("Pause on last frame", hold);

        loop = new QSpinBox{&dialog};
        loop->setRange(0, 100);
        loop->setSpecialValueText("Forever");
        loop->setValue(options.loopCount);
        form->addRow("Play count", loop);
    }
    if (!toolAvailable("img2webp")) {
        auto * note = new QLabel{"Using ffmpeg: \"Mixed\", sharper colour edges and the last-frame pause "
                                 "need img2webp (included in the portable packages).", &dialog};
        note->setWordWrap(true);
        form->addRow(note);
    }

    auto updateEnabled = [mode, quality, sharp] {
        const auto lossless = mode->currentIndex() == WebpOptions::Lossless;
        quality->setEnabled(!lossless);
        sharp->setEnabled(!lossless);
    };
    QObject::connect(mode, &QComboBox::currentIndexChanged, &dialog, updateEnabled);
    updateEnabled();

    auto * buttons = new QDialogButtonBox{QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog};
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    options.mode = static_cast<WebpOptions::Mode>(mode->currentIndex());
    options.quality = quality->value();
    options.effort = effort->value();
    options.sharpYuv = sharp->isChecked();
    options.frameDelayMs = delay ? delay->value() : 0;
    if (hold) {
        options.lastFrameHoldMs = hold->value();
    }
    if (loop) {
        options.loopCount = loop->value();
    }
    Utility::writePrefInt("WEBP_EXPORT_MODE", options.mode);
    Utility::writePrefInt("WEBP_EXPORT_QUALITY", options.quality);
    Utility::writePrefInt("WEBP_EXPORT_EFFORT", options.effort);
    Utility::writePrefInt("WEBP_EXPORT_LAST_HOLD", options.lastFrameHoldMs);
    Utility::writePrefInt("WEBP_EXPORT_LOOP", options.loopCount);
    Utility::writePrefInt("WEBP_EXPORT_SHARP_YUV", options.sharpYuv ? 1 : 0);
    webpOptions = options;
    webpOptionsSet = true;
    return true;
}

// frames are already in framesDir as frame_0000.png, frame_0001.png, ...
bool UtilityAnimationExport::encodeWebp(size_t frameCount, bool animated, int frameDelayMs, const QString& framesDir,
                                        const QString& path, QString& error) {
    if (!webpOptionsSet) {
        webpOptions = savedWebpOptions();
        webpOptionsSet = true;
    }
    const auto& options = webpOptions;
    const auto delay = std::max(20, options.frameDelayMs > 0 ? options.frameDelayMs : frameDelayMs);
    auto frameFile = [&] (size_t i) { return framesDir + "/frame_" + QString::number(i).rightJustified(4, '0') + ".png"; };

    const auto img2webp = UtilityTools::find("img2webp");
    if (!img2webp.isEmpty()) {
        // file-level options first, then per-frame options, which img2webp
        // applies to every frame file that follows them
        QStringList args;
        if (animated) {
            args << "-loop" << QString::number(options.loopCount);
        }
        if (options.mode == WebpOptions::Mixed) {
            args << "-mixed";
        }
        if (options.sharpYuv && options.mode != WebpOptions::Lossless) {
            args << "-sharp_yuv";
        }
        args << (options.mode == WebpOptions::Lossless ? "-lossless" : "-lossy")
             << "-q" << QString::number(options.mode == WebpOptions::Lossless ? 75 : options.quality)
             << "-m" << QString::number(options.effort)
             << "-d" << QString::number(delay);
        for (size_t i = 0; i < frameCount; i += 1) {
            if (animated && i + 1 == frameCount && options.lastFrameHoldMs > 0) {
                args << "-d" << QString::number(delay + options.lastFrameHoldMs);
            }
            args << frameFile(i);
        }
        args << "-o" << path;
        const auto result = runTool(img2webp, args, 300000);
        error = result.error;
        return result.ok && QFile::exists(path);
    }

    QStringList args{"-y", "-hide_banner", "-loglevel", "error"};
    if (animated) {
        args << "-framerate" << QString::number(1000.0 / delay, 'f', 3);
    }
    args << "-i" << framesDir + "/frame_%04d.png" << "-c:v" << "libwebp_anim";
    if (options.mode == WebpOptions::Lossless) {
        args << "-lossless" << "1";
    } else {
        args << "-quality" << QString::number(options.quality);
    }
    args << "-compression_level" << QString::number(options.effort)
         << "-loop" << QString::number(animated ? options.loopCount : 0) << path;
    const auto result = runTool(UtilityTools::find("ffmpeg"), args, 300000);
    error = result.error;
    return result.ok && QFile::exists(path);
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
    if (webpAvailable()) {
        list.push_back({"webp", animated ? "Animated WebP" : "WebP image", "webp"});
    }
    if (animated && toolAvailable("ffmpeg")) {
        list.push_back({"mp4", "MP4 video (H.264)", "mp4"});
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
        const auto result = runTool(UtilityTools::find("avifenc"), args, 300000);
        error = result.error;
        return result.ok && QFile::exists(path);
    }
    if (format.id == "webp") {
        return encodeWebp(source.size(), animated, frameDelayMs, temp.path(), path, error);
    }
    if (format.id == "mp4") {
        const auto rate = QString::number(1000.0 / std::max(1, frameDelayMs), 'f', 3);
        QStringList args{"-y", "-hide_banner", "-loglevel", "error"};
        if (animated) {
            args << "-framerate" << rate;
        }
        args << "-i" << temp.path() + "/frame_%04d.png"
             << "-c:v" << "libx264" << "-crf" << "18" << "-pix_fmt" << "yuv420p"
             << "-vf" << "pad=ceil(iw/2)*2:ceil(ih/2)*2:color=white";
        args << path;
        const auto result = runTool(UtilityTools::find("ffmpeg"), args, 300000);
        error = result.error;
        return result.ok && QFile::exists(path);
    }
    error = "unknown export format: " + format.id;
    return false;
}

void UtilityAnimationExport::setSourceBytes(QLabel * label, const QByteArray& bytes) {
    label->setProperty("wxqtSourceBytes", bytes);
    // hovering shows when the picture was produced and how to save it
    if (bytes.isEmpty()) {
        label->setToolTip({});
        return;
    }
    const auto updated = updatedText(bytes);
    label->setToolTip((updated.isEmpty() ? QString{} : updated + "\n") + "Right-click to save");
}

void UtilityAnimationExport::installContextSave(QLabel * label) {
    label->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(label, &QLabel::customContextMenuRequested, label, [label] (const QPoint& point) {
        const auto bytes = label->property("wxqtSourceBytes").toByteArray();
        if (bytes.isEmpty()) {
            return;
        }
        QMenu menu{label};
        auto * save = menu.addAction("Save image...");
        if (menu.exec(label->mapToGlobal(point)) != save) {
            return;
        }
        auto clean = [] (const QString& text, int length) { return slug(text, length); };
        auto product = clean(label->window()->windowTitle(), 40);
        // a screen showing several pictures (outlook grids, dashboards) would give
        // them all the same name, so add the picture's own file name when so
        int saveable = 0;
        for (auto * other : label->window()->findChildren<QLabel *>()) {
            saveable += other->property("wxqtSourceBytes").toByteArray().isEmpty() ? 0 : 1;
        }
        URL::Meta meta;
        if (saveable > 1 && URL::metaFor(bytes, meta)) {
            auto file = QString::fromStdString(meta.url).section('?', 0, 0).section('/', -1);
            file = clean(QFileInfo{file}.completeBaseName(), 30);
            if (!file.isEmpty()) {
                product += (product.isEmpty() ? "" : "_") + file;
            }
        }
        saveWithDialog(label->window(), {}, 0, bytes, product.isEmpty() ? QString{"image"} : product);
    });
}

void UtilityAnimationExport::showInstallHelp(QWidget * parent, const QStringList& missing) {
    QString text = "<p><b>More export formats</b> need a small free helper program that is not "
                   "included, to keep this download small. Animated PNG always works without them.</p><ul>";
    if (missing.contains("cjxl")) {
        text += "<li><b>JPEG XL</b> - <code>cjxl</code>: "
                "<a href=\"https://github.com/libjxl/libjxl/releases\">libjxl releases</a></li>";
    }
    if (missing.contains("avifenc")) {
        text += "<li><b>AVIF</b> - <code>avifenc</code>: "
                "<a href=\"https://github.com/AOMediaCodec/libavif/releases\">libavif releases</a></li>";
    }
    if (missing.contains("img2webp")) {
        text += "<li><b>WebP</b> - <code>img2webp</code> (libwebp tools): "
                "<a href=\"https://developers.google.com/speed/webp/download\">WebP downloads</a></li>";
    }
    if (missing.contains("ffmpeg")) {
        text += QString{"<li><b>"} + (missing.contains("img2webp") ? "MP4 (and WebP)" : "MP4") + "</b> - <code>ffmpeg</code>: "
                "<a href=\"https://ffmpeg.org/download.html\">ffmpeg.org/download</a></li>";
    }
    text += "</ul>";
#ifdef Q_OS_WIN
    text += "<p><b>Windows:</b> for ffmpeg run <code>winget install Gyan.FFmpeg</code>; otherwise download the "
            "files from the links above. Then copy the program (and any .dll files that come with it) into "
            "the <b>tools</b> folder next to wxqt.exe - the button below opens it - or add it to your PATH.</p>";
#else
    QStringList packages;
    if (missing.contains("cjxl")) { packages << "libjxl-tools"; }
    if (missing.contains("avifenc")) { packages << "libavif-bin"; }
    if (missing.contains("img2webp")) { packages << "webp"; }
    if (missing.contains("ffmpeg")) { packages << "ffmpeg"; }
    QStringList arch;
    if (missing.contains("cjxl")) { arch << "libjxl"; }
    if (missing.contains("avifenc")) { arch << "libavif"; }
    if (missing.contains("img2webp")) { arch << "libwebp-utils"; }
    if (missing.contains("ffmpeg")) { arch << "ffmpeg"; }
    QStringList fedora;
    if (missing.contains("cjxl")) { fedora << "libjxl-utils"; }
    if (missing.contains("avifenc")) { fedora << "libavif-tools"; }
    if (missing.contains("img2webp")) { fedora << "libwebp-tools"; }
    if (missing.contains("ffmpeg")) { fedora << "ffmpeg"; }
    QStringList brew;
    if (missing.contains("cjxl")) { brew << "jpeg-xl"; }
    if (missing.contains("avifenc")) { brew << "libavif"; }
    if (missing.contains("img2webp")) { brew << "webp"; }
    if (missing.contains("ffmpeg")) { brew << "ffmpeg"; }
    text += "<p>Install with your package manager:</p><pre>"
            "Debian/Ubuntu   sudo apt install " + packages.join(" ") + "\n"
            "Arch/CachyOS    sudo pacman -S " + arch.join(" ") + "\n"
            "Fedora          sudo dnf install " + fedora.join(" ") + "\n"
            "macOS (brew)    brew install " + brew.join(" ") + "</pre>"
            "<p>They only need to be on your PATH. (Or drop the programs into the <b>tools</b> folder "
            "next to the wxqt program - the button below opens it.)</p>";
#endif
    text += "<p>Then reopen Save - the new formats appear in the file-type list.</p>";

    QMessageBox box{QMessageBox::Information, "Get more export formats", text, QMessageBox::Close, parent};
    box.setTextFormat(Qt::RichText);
    auto * openFolder = box.addButton("Open tools folder", QMessageBox::ActionRole);
    box.exec();
    if (box.clickedButton() == static_cast<QAbstractButton *>(openFolder)) {
        QDir{}.mkpath(UtilityTools::folder());
        QDesktopServices::openUrl(QUrl::fromLocalFile(UtilityTools::folder()));
    }
}

QString UtilityAnimationExport::slug(const QString& text, int length) {
    auto result = text.toLower();
    result.replace(QRegularExpression{"[^a-z0-9]+"}, "_");
    result = result.left(length);
    while (result.endsWith('_')) {
        result.chop(1);
    }
    return result;
}

namespace {
    struct ParsedRun {
        QDateTime run;
        int firstHour{0};
        int lastHour{0};
    };

    bool parseStatus(const QString& status, ParsedRun& out) {
        const auto match = QRegularExpression{R"((\d{4})-(\d{2})-(\d{2}) (\d{2})z\s+F(\d+)(?:-F(\d+))?)"}.match(status);
        if (!match.hasMatch()) {
            return false;
        }
        out.run = QDateTime{QDate{match.captured(1).toInt(), match.captured(2).toInt(), match.captured(3).toInt()},
                            QTime{match.captured(4).toInt(), 0}, QTimeZone::utc()};
        out.firstHour = match.captured(5).toInt();
        out.lastHour = match.captured(6).isEmpty() ? out.firstHour : match.captured(6).toInt();
        return out.run.isValid();
    }

    QString stamp(const QDateTime& when) {
        return when.toUTC().toString("yyyyMMdd_HHmm") + "Z";
    }
}

QByteArray UtilityAnimationExport::withHeader(const QByteArray& imageBytes, const QString& status, const QString& units) {
    QImage image = QImage::fromData(imageBytes);
    ParsedRun run;
    if (image.isNull() || !parseStatus(status, run)) {
        return imageBytes;
    }
    // "<model>  <run date> <cycle>z    F12 valid ...    <product>    <region>": the model is the text before
    // the run date, the region is the last field, the product the field before it (if any)
    const auto dateAt = QRegularExpression{R"(\d{4}-\d{2}-\d{2} \d{2}z)"}.match(status);
    const QString model = status.left(dateAt.capturedStart()).trimmed();
    const auto afterRun = status.mid(dateAt.capturedEnd());
    const auto parts = afterRun.split(QRegularExpression{R"(\s{3,})"}, Qt::SkipEmptyParts);
    // parts[0] is the hour text; then an optional product; the last part is the region
    const QString region = parts.size() >= 2 ? parts.last() : QString{};
    QString product = parts.size() >= 3 ? parts[parts.size() - 2] : model;
    if (!units.isEmpty()) {
        product += " (" + units + ")";
    }
    UtilityGrib::drawMapHeader(image, UtilityGrib::standardHeader(model, product, region, run.run, run.firstHour, run.lastHour));
    QByteArray out;
    QBuffer buffer{&out};
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    return out.isEmpty() ? imageBytes : out;
}

vector<QByteArray> UtilityAnimationExport::withHeaders(const vector<QByteArray>& frames, const vector<std::string>& statuses,
                                                       const QString& units) {
    vector<QByteArray> out;
    for (size_t i = 0; i < frames.size(); i += 1) {
        out.push_back(i < statuses.size() ? withHeader(frames[i], QString::fromStdString(statuses[i]), units) : frames[i]);
    }
    return out;
}

QString UtilityAnimationExport::modelName(const QString& firstStatus, const QString& lastStatus, const QString& product) {
    ParsedRun first;
    if (!parseStatus(firstStatus, first)) {
        return product;
    }
    ParsedRun last = first;
    if (!lastStatus.isEmpty() && !parseStatus(lastStatus, last)) {
        last = first;
    }
    const auto fh = [] (int hour) { return QString::number(hour).rightJustified(3, '0'); };
    const auto validFirst = first.run.addSecs(3600 * first.firstHour);
    const auto validLast = last.run.addSecs(3600 * last.lastHour);
    QString hours = "f" + fh(first.firstHour);
    QString valid = "v" + stamp(validFirst);
    if (first.run != last.run || last.lastHour != first.firstHour) {
        hours += "-f" + fh(last.lastHour);
        valid += "-" + stamp(validLast);
    }
    return first.run.toString("yyyyMMdd_HH") + "z_" + hours + "_" + valid + "_" + product;
}

QString UtilityAnimationExport::validName(const QDateTime& first, const QDateTime& last, const QString& product) {
    if (!first.isValid()) {
        return product;
    }
    auto name = stamp(first);
    if (last.isValid() && last != first) {
        name += "-" + stamp(last);
    }
    return name + "_" + product;
}

QString UtilityAnimationExport::datedName(const QString& product, const QByteArray& bytes) {
    URL::Meta meta;
    if (!URL::metaFor(bytes, meta)) {
        return product;
    }
    const auto when = meta.lastModified.isValid() ? meta.lastModified : meta.fetched;
    return when.toUTC().toString("yyyyMMdd_HHmm") + "Z_" + product;
}

QString UtilityAnimationExport::updatedText(const QByteArray& bytes) {
    URL::Meta meta;
    if (!URL::metaFor(bytes, meta)) {
        return {};
    }
    if (meta.lastModified.isValid()) {
        return "Updated " + meta.lastModified.toUTC().toString("yyyy-MM-dd HH:mm") + " UTC";
    }
    return "Downloaded " + meta.fetched.toUTC().toString("yyyy-MM-dd HH:mm") + " UTC";
}

bool UtilityAnimationExport::saveWithDialog(QWidget * parent, const vector<QByteArray>& frames, int frameDelayMs,
                                            const QByteArray& still, const QString& baseName,
                                            const QByteArray& metaBytes, bool datePrefix) {
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
    QStringList missingTools;
    if (!UtilityJxl::available()) {
        missingTools << "cjxl";
    }
    if (!toolAvailable("avifenc")) {
        missingTools << "avifenc";
    }
    if (!toolAvailable("img2webp")) {
        missingTools << "img2webp";
    }
    if (!toolAvailable("ffmpeg")) {
        missingTools << "ffmpeg";     // MP4 (and a WebP fallback)
    }
    // last entry in the file-type list: leads to install instructions
    const QString helpFilter = "More formats: JPEG XL, AVIF, WebP, MP4 (how to add)...";
    if (!missingTools.isEmpty()) {
        filters << helpFilter;
    }

    const auto & metaSource = !metaBytes.isEmpty() ? metaBytes
        : (!still.isEmpty() ? still : (frames.empty() ? still : frames.back()));
    const auto datedBase = datePrefix ? datedName(baseName, metaSource) : baseName;
    const auto picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto stem = picturesDir.isEmpty() ? datedBase : picturesDir + "/" + datedBase;
    QString selectedFilter = filters.first();
    const auto fileName = QFileDialog::getSaveFileName(parent, caption, stem + "." + ordered.front().ext,
                                                       filters.join(";;"), &selectedFilter);
    if (fileName.isEmpty()) {
        return false;   // the user cancelled
    }
    if (selectedFilter == helpFilter) {
        showInstallHelp(parent, missingTools);
        return false;
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

    if (chosen.id == "webp") {
        WebpOptions options;
        if (!webpOptionsDialog(parent, animated, frameDelayMs, options)) {
            return false;   // the user cancelled
        }
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
