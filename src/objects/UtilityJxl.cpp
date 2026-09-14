// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "objects/UtilityJxl.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>

namespace {
    // same magic-byte sniff ImageViewer used to use locally; centralised here
    // since every save site needs it for the "cjxl unavailable" fallback
    string sniffExtension(const QByteArray& bytes) {
        if (bytes.startsWith("\x89PNG")) {
            return ".png";
        }
        if (bytes.startsWith("GIF8")) {
            return ".gif";
        }
        if (bytes.size() > 2 && static_cast<unsigned char>(bytes[0]) == 0xFF
                && static_cast<unsigned char>(bytes[1]) == 0xD8) {
            return ".jpg";
        }
        return ".png";
    }
}

string UtilityJxl::cjxlPath() {
    return QStandardPaths::findExecutable("cjxl").toStdString();
}

bool UtilityJxl::available() {
    return !cjxlPath().empty();
}

string UtilityJxl::cacheDir() {
    auto path = QDir::tempPath() + "/wxqt_jxl";
    QDir{}.mkpath(path);
    return path.toStdString();
}

bool UtilityJxl::encode(const QByteArray& bytes, const QString& outPath, bool lossless) {
    const auto cjxl = cjxlPath();
    if (cjxl.empty() || bytes.isEmpty()) {
        return false;
    }
    const auto tempPath = QString::fromStdString(cacheDir()) + "/in_" +
        QString::number(QDateTime::currentMSecsSinceEpoch()) + ".png";
    {
        QFile tempFile{tempPath};
        if (!tempFile.open(QIODevice::WriteOnly)) {
            return false;
        }
        tempFile.write(bytes);
        tempFile.close();
    }

    QFile::remove(outPath);
    QStringList args{tempPath, outPath, "--effort", "7"};
    if (lossless) {
        args << "-d" << "0";
    } else {
        args << "-q" << "90";
    }
    QProcess process;
    process.start(QString::fromStdString(cjxl), args);
    process.waitForFinished(30000);
    const auto ok = process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    QFile::remove(tempPath);
    return ok && QFile::exists(outPath);
}

string UtilityJxl::preferredExtension(const QByteArray& fallbackBytes) {
    return available() ? string{".jxl"} : sniffExtension(fallbackBytes);
}

QString UtilityJxl::save(const QByteArray& bytes, QString path) {
    if (available() && encode(bytes, path)) {
        return path;
    }
    if (path.endsWith(".jxl", Qt::CaseInsensitive)) {
        path.chop(4);
        path += QString::fromStdString(sniffExtension(bytes));
    }
    QFile file{path};
    if (file.open(QIODevice::WriteOnly)) {
        file.write(bytes);
        file.close();
    }
    return path;
}
