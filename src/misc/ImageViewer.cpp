// *****************************************************************************
// * Copyright (c) 2020, 2021, 2022, 2023, 2024 joshua.tee@gmail.com. All rights reserved.
// *
// * Refer to the COPYING file of the official project for license.
// *****************************************************************************

#include "ImageViewer.h"
#include <algorithm>
#include <QBuffer>
#include <QFile>
#include <QFileDialog>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QStandardPaths>
#include "objects/FutureBytes.h"
#include "objects/UtilityJxl.h"

namespace {
    // add a titled header strip above the image (for saved copies)
    QImage withHeader(const QImage& source, const QString& text) {
        QImage probe{1, 1, QImage::Format_ARGB32};
        QPainter measure{&probe};
        auto font = measure.font();
        font.setPixelSize(std::max(16, source.height() / 42));
        font.setBold(true);
        measure.setFont(font);
        const auto pad = font.pixelSize() / 2 + 4;
        const auto headerHeight = measure.fontMetrics().height() + pad * 2;
        measure.end();

        QImage out{source.width(), source.height() + headerHeight, QImage::Format_ARGB32};
        out.fill(QColor{28, 28, 30});
        QPainter painter{&out};
        painter.setFont(font);
        painter.setPen(QColor{240, 240, 240});
        painter.drawText(QRect{pad, 0, out.width() - pad * 2, headerHeight},
                         Qt::AlignVCenter | Qt::AlignLeft, text);
        painter.drawImage(0, headerHeight, source);
        painter.end();
        return out;
    }

    string sanitize(string name) {
        for (auto& c : name) {
            if (c == '/' || c == '\\' || c == ':' || c == '?' || c == '*' || c == '"' || c == '<' || c == '>' || c == '|') {
                c = '_';
            }
        }
        return name;
    }
}

ImageViewer::ImageViewer(Window * parent, const QByteArray& data, string title)
    : Window{parent}
    , image{this}
    , saveButton{this, None, "Save Image"}
    , saveShortcut{QKeySequence{"S"}, this}
    , titleText{title}
{
    setTitle(title);
    saveButton.connect([this] { save(); });
    saveShortcut.connect([this] { save(); });
    boxTop.addWidget(saveButton);
    boxTop.addStretch();
    box.addLayout(boxTop);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    setBytes(data);
}

ImageViewer::ImageViewer(Window * parent, const string& url, string title)
    : Window{parent}
    , image{this}
    , saveButton{this, None, "Save Image"}
    , saveShortcut{QKeySequence{"S"}, this}
    , titleText{title}
    , sourceUrl{url}
{
    setTitle(title);
    saveButton.connect([this] { save(); });
    saveShortcut.connect([this] { save(); });
    boxTop.addWidget(saveButton);
    boxTop.addStretch();
    box.addLayout(boxTop);
    box.addWidgetReal(&image, 1, Qt::Alignment{});
    box.getAndShow(this);
    new FutureBytes{this, url, [this] (const auto& ba) { setBytes(ba); }};
}

void ImageViewer::setBytes(const QByteArray& data) {
    imageBytes = data;
    image.setBytes(data);
}

void ImageViewer::resizeEventCustom() {
    // ZoomImage re-fits itself on resize while the user has not zoomed
}

string ImageViewer::suggestedFileName(const QByteArray& savedBytes) const {
    string base;
    if (!sourceUrl.empty()) {
        base = sourceUrl.substr(sourceUrl.find_last_of('/') + 1);
        const auto queryStart = base.find('?');
        if (queryStart != string::npos) {
            base = base.substr(0, queryStart);
        }
    }
    if (base.empty() && !titleText.empty()) {
        base = titleText;
    }
    if (base.empty()) {
        base = "wxqt-image";
    }
    base = sanitize(base);
    const auto dot = base.find_last_of('.');
    if (dot != string::npos) {
        base = base.substr(0, dot);   // strip any existing extension - the real save format is decided below
    }
    return base + UtilityJxl::preferredExtension(savedBytes);
}

void ImageViewer::save() {
    if (imageBytes.isEmpty()) {
        return;
    }
    // apply the title header (if any) first, so the JXL-vs-fallback decision
    // below sees the bytes that will actually be written
    QByteArray sourceBytes = imageBytes;
    if (!titleText.empty()) {
        const auto decoded = QImage::fromData(imageBytes);
        if (!decoded.isNull()) {
            QByteArray headered;
            QBuffer buffer{&headered};
            buffer.open(QIODevice::WriteOnly);
            withHeader(decoded, QString::fromStdString(titleText)).save(&buffer, "PNG");
            buffer.close();
            if (!headered.isEmpty()) {
                sourceBytes = headered;
            }
        }
    }

    const auto picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const auto suggested = QString::fromStdString(suggestedFileName(sourceBytes));
    const auto defaultPath = picturesDir.isEmpty() ? suggested : picturesDir + "/" + suggested;
    const auto filter = UtilityJxl::available()
        ? QString{"JPEG XL Image (*.jxl);;All Files (*)"}
        : QString{"Images (*.png *.gif *.jpg *.jpeg);;All Files (*)"};
    const auto fileName = QFileDialog::getSaveFileName(this, "Save Image", defaultPath, filter);
    if (fileName.isEmpty()) {
        return;
    }
    UtilityJxl::save(sourceBytes, fileName);
}
