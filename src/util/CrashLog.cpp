// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/CrashLog.h"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <mutex>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QtLogging>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {
    std::mutex logMutex;
    QString logPath;

    QString chooseLogPath() {
        const auto portable = QCoreApplication::applicationDirPath();
        if (QFileInfo{portable}.isWritable()) {
            return portable + "/wxqt.log";
        }
        const auto local = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        QDir{}.mkpath(local);
        return local + "/wxqt.log";
    }

    void append(const QString& line) {
        std::lock_guard<std::mutex> lock{logMutex};
        if (logPath.isEmpty()) {
            return;
        }
        if (QFileInfo{logPath}.size() > 1024 * 1024) {
            QFile::remove(logPath + ".old");
            QFile::rename(logPath, logPath + ".old");
        }
        QFile file{logPath};
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            file.write((line + "\n").toUtf8());
        }
    }

    void messageHandler(QtMsgType type, const QMessageLogContext&, const QString& message) {
        const char * kind = type == QtDebugMsg ? "debug" : type == QtInfoMsg ? "info" : type == QtWarningMsg ? "warning"
            : type == QtCriticalMsg ? "CRITICAL" : "FATAL";
        append(QString{"Qt %1: %2"}.arg(kind, message));
        std::fprintf(stderr, "%s\n", message.toLocal8Bit().constData());
    }

    void onTerminate() {
        QString what = "terminate() called with no active exception";
        if (const auto current = std::current_exception()) {
            try {
                std::rethrow_exception(current);
            } catch (const std::exception& e) {
                what = QString{"uncaught C++ exception: %1"}.arg(e.what());
            } catch (...) {
                what = "uncaught exception of an unknown type";
            }
        }
        append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + "FATAL " + what);
        std::abort();
    }

#ifdef Q_OS_WIN
    LONG WINAPI onCrash(EXCEPTION_POINTERS * info) {
        const auto * record = info->ExceptionRecord;
        const auto describe = [] (void * address) {
            HMODULE module = nullptr;
            char name[MAX_PATH] = "?";
            if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                   static_cast<LPCSTR>(address), &module)) {
                GetModuleFileNameA(module, name, MAX_PATH);
            }
            const auto offset = static_cast<unsigned long long>(reinterpret_cast<ULONG_PTR>(address) - reinterpret_cast<ULONG_PTR>(module));
            return QString{"%1+0x%2"}.arg(QFileInfo{QString::fromLocal8Bit(name)}.fileName()).arg(offset, 0, 16);
        };
        QString text = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") +
            QString{"CRASH code 0x%1 at %2"}.arg(static_cast<unsigned long>(record->ExceptionCode), 0, 16).arg(describe(record->ExceptionAddress));
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2) {
            text += QString{" (%1 address 0x%2)"}.arg(record->ExceptionInformation[0] == 0 ? "read of" : record->ExceptionInformation[0] == 1 ? "write to" : "execute at")
                .arg(static_cast<unsigned long long>(record->ExceptionInformation[1]), 0, 16);
        }
        void * frames[32];
        const auto count = CaptureStackBackTrace(0, 32, frames, nullptr);
        for (USHORT i = 0; i < count; i += 1) {
            text += "\n    " + describe(frames[i]);
        }
        append(text);
        return EXCEPTION_CONTINUE_SEARCH;   // let Windows show / record the crash as usual
    }
#endif
}

void CrashLog::install() {
    logPath = chooseLogPath();
    qInstallMessageHandler(messageHandler);
    std::set_terminate(onTerminate);
#ifdef Q_OS_WIN
    SetUnhandledExceptionFilter(onCrash);
#endif
    append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + "---- wxqt started ----");
}

void CrashLog::write(const std::string& line) {
    append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ") + QString::fromStdString(line));
}

std::string CrashLog::path() {
    return logPath.toStdString();
}
