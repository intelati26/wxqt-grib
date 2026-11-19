// *****************************************************************************
// * This file is part of wxqt.  Licensed under the GNU General Public License v3.
// * See the COPYING file for the full license text.
// *****************************************************************************

#include "util/UtilityTheme.h"
#include <QApplication>
#include <QColor>
#include <QGuiApplication>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include "util/UtilityList.h"
#include "util/Utility.h"

namespace {
    bool platformCaptured{false};
    bool platformDark{false};   // the palette the platform gave the application before any theme was applied (the fallback for the OS setting)
    bool followingSystem{false};

    void capturePlatform() {
        if (!platformCaptured) {
            platformDark = QApplication::palette().color(QPalette::Window).lightness() < 128;
            platformCaptured = true;
        }
    }

    // does the operating system ask for a dark look? The color scheme the OS reports, else the platform's own palette
    bool systemWantsDark() {
        if (const auto hints = QGuiApplication::styleHints()) {
            switch (hints->colorScheme()) {
                case Qt::ColorScheme::Dark: return true;
                case Qt::ColorScheme::Light: return false;
                default: break;
            }
        }
        return platformDark;
    }

    QPalette darkPalette() {
        const QColor window{53, 53, 53};
        const QColor base{35, 35, 35};
        const QColor text{221, 221, 221};
        const QColor dim{130, 130, 130};
        const QColor highlight{42, 130, 218};
        QPalette p;
        p.setColor(QPalette::Window, window);
        p.setColor(QPalette::WindowText, text);
        p.setColor(QPalette::Base, base);
        p.setColor(QPalette::AlternateBase, window);
        p.setColor(QPalette::ToolTipBase, window);
        p.setColor(QPalette::ToolTipText, text);
        p.setColor(QPalette::Text, text);
        p.setColor(QPalette::Button, window);
        p.setColor(QPalette::ButtonText, text);
        p.setColor(QPalette::BrightText, QColor{255, 80, 80});
        p.setColor(QPalette::Link, QColor{97, 175, 239});
        p.setColor(QPalette::Highlight, highlight);
        p.setColor(QPalette::HighlightedText, Qt::black);
        p.setColor(QPalette::PlaceholderText, dim);
        p.setColor(QPalette::Disabled, QPalette::Text, dim);
        p.setColor(QPalette::Disabled, QPalette::WindowText, dim);
        p.setColor(QPalette::Disabled, QPalette::ButtonText, dim);
        return p;
    }

    QPalette lightPalette() {
        QPalette p;   // Fusion default is a clean light palette
        p.setColor(QPalette::Link, QColor{38, 97, 139});
        return p;
    }

    // what the program asks of the style hints (Qt::Unknown: follow the system)
    enum class ColorSchemeChoice { Unknown, Light, Dark };

    void setColorScheme(ColorSchemeChoice scheme) {
        if (const auto hints = QGuiApplication::styleHints()) {
            switch (scheme) {
                case ColorSchemeChoice::Dark: hints->setColorScheme(Qt::ColorScheme::Dark); break;
                case ColorSchemeChoice::Light: hints->setColorScheme(Qt::ColorScheme::Light); break;
                default: hints->setColorScheme(Qt::ColorScheme::Unknown); break;
            }
        }
    }
}

const string UtilityTheme::pref{"THEME"};
const vector<string> UtilityTheme::labels{"System", "Light", "Dark"};
const vector<string> UtilityTheme::values{"system", "light", "dark"};

void UtilityTheme::apply() {
    applyTheme(Utility::readPref(pref, "system"));
}

void UtilityTheme::applyTheme(const string& theme) {
    capturePlatform();
    followingSystem = theme != "dark" && theme != "light";
    if (theme == "dark") {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(darkPalette());
        setColorScheme(ColorSchemeChoice::Dark);
    } else if (theme == "light") {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(lightPalette());
        setColorScheme(ColorSchemeChoice::Light);
    } else {
        // follow the system: the same Fusion look on every platform, light or dark as the operating system asks
        setColorScheme(ColorSchemeChoice::Unknown);
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(systemWantsDark() ? darkPalette() : lightPalette());
        static bool connected{false};   // switch live when the operating system changes between light and dark
        if (!connected) {
            if (const auto hints = QGuiApplication::styleHints()) {
                connected = true;
                QObject::connect(hints, &QStyleHints::colorSchemeChanged, qApp, [] {
                    if (followingSystem) {
                        QApplication::setPalette(systemWantsDark() ? darkPalette() : lightPalette());
                    }
                });
            }
        }
    }
}

int UtilityTheme::prefIndex() {
    const auto current = Utility::readPref(pref, "system");
    const auto index = indexOf(values, current);
    return index < 0 ? 0 : index;
}

bool UtilityTheme::isDark() {
    const auto theme = Utility::readPref(pref, "system");
    if (theme == "dark") {
        return true;
    }
    if (theme == "light") {
        return false;
    }
    if (const auto hints = QGuiApplication::styleHints()) {
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}
