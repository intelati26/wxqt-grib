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
    QString defaultStyleName;
    bool defaultStyleCaptured{false};

    void captureDefaultStyle() {
        if (!defaultStyleCaptured) {
            if (const auto style = QApplication::style()) {
                defaultStyleName = style->name();
            }
            defaultStyleCaptured = true;
        }
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

    // Qt::ColorScheme itself is a 6.5+ enum (QStyleHints::setColorScheme(),
    // called below, is 6.8+) - this app also targets Debian bookworm's Qt
    // 6.4.2 (see README_OS.md), which has neither, so callers pass this
    // version-independent stand-in instead of referencing Qt::ColorScheme
    // directly at every call site.
    enum class ColorSchemeChoice { Unknown, Light, Dark };

    void setColorScheme(ColorSchemeChoice scheme) {
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        if (const auto hints = QGuiApplication::styleHints()) {
            switch (scheme) {
                case ColorSchemeChoice::Dark: hints->setColorScheme(Qt::ColorScheme::Dark); break;
                case ColorSchemeChoice::Light: hints->setColorScheme(Qt::ColorScheme::Light); break;
                default: hints->setColorScheme(Qt::ColorScheme::Unknown); break;
            }
        }
#else
        (void) scheme;
#endif
    }
}

const string UtilityTheme::pref{"THEME"};
const vector<string> UtilityTheme::labels{"System", "Light", "Dark"};
const vector<string> UtilityTheme::values{"system", "light", "dark"};

void UtilityTheme::apply() {
    applyTheme(Utility::readPref(pref, "system"));
}

void UtilityTheme::applyTheme(const string& theme) {
    captureDefaultStyle();
    if (theme == "dark") {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(darkPalette());
        setColorScheme(ColorSchemeChoice::Dark);
    } else if (theme == "light") {
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QApplication::setPalette(lightPalette());
        setColorScheme(ColorSchemeChoice::Light);
    } else {
        const auto name = defaultStyleName.isEmpty() ? QStringLiteral("Fusion") : defaultStyleName;
        QApplication::setStyle(QStyleFactory::create(name));
        QApplication::setPalette(QApplication::style()->standardPalette());
        setColorScheme(ColorSchemeChoice::Unknown);
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (const auto hints = QGuiApplication::styleHints()) {
        return hints->colorScheme() == Qt::ColorScheme::Dark;
    }
#endif
    return QApplication::palette().color(QPalette::Window).lightness() < 128;
}
