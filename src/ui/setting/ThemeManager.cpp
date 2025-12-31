#include <QStyle>
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>

#include "nekobox/ui/setting/ThemeManager.hpp"
#include "nekobox/ui/design/DesignTokens.hpp"
#include "iostream"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

void ThemeManager::ApplyTheme(const QString &theme, bool force) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    if (this->current_theme == theme && !force) {
        return;
    }

    // If using premium theme system, convert to new mode
    if (m_usePremiumTheme) {
        auto lowerTheme = theme.toLower();
        if (lowerTheme == "system" || theme == "0") {
            applyPremiumTheme(AppThemeMode::System, force);
        } else if (lowerTheme == "light" || theme == "1") {
            applyPremiumTheme(AppThemeMode::Light, force);
        } else if (lowerTheme == "dark" || theme == "2") {
            applyPremiumTheme(AppThemeMode::Dark, force);
        } else if (lowerTheme == "qdarkstyle" || theme == "3") {
            applyPremiumTheme(AppThemeMode::QDarkStyle, force);
        }
        return;
    }

    // Legacy theme application
    auto lowerTheme = theme.toLower();
    if (lowerTheme == "system") {
        qApp->setStyleSheet("");
        qApp->setStyle(system_style_name);
    } else if (lowerTheme == "qdarkstyle") {
        QFile f(":qdarkstyle/dark/darkstyle.qss");
        if (f.open(QFile::ReadOnly | QFile::Text)){
            QTextStream ts(&f);
            qApp->setStyleSheet(ts.readAll());
        }
    } else {
        qApp->setStyleSheet("");
        qApp->setStyle(theme);
    }

    current_theme = theme;
    emit themeChanged(theme);
}

void ThemeManager::applyPremiumTheme(AppThemeMode mode, bool force) {
    if (!force && m_currentMode == mode) {
        return;
    }

    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->name();
    }

    // Get design tokens instance
    auto& tokens = DesignTokens::instance();

    switch (mode) {
        case AppThemeMode::System: {
            // Detect system theme (simplified - defaults to dark for now)
            // TODO: Add proper system theme detection for Windows/Linux
            tokens.setThemeMode(ThemeMode::Dark);
            qApp->setStyle(system_style_name);
            qApp->setStyleSheet(tokens.generateStyleSheet());
            current_theme = "0";
            break;
        }

        case AppThemeMode::Light: {
            tokens.setThemeMode(ThemeMode::Light);
            // Use Fusion style as base for best cross-platform consistency
            if (QStyleFactory::keys().contains("Fusion")) {
                qApp->setStyle("Fusion");
            }
            qApp->setStyleSheet(tokens.generateStyleSheet());
            current_theme = "1";
            break;
        }

        case AppThemeMode::Dark: {
            tokens.setThemeMode(ThemeMode::Dark);
            // Use Fusion style as base for best cross-platform consistency
            if (QStyleFactory::keys().contains("Fusion")) {
                qApp->setStyle("Fusion");
            }
            qApp->setStyleSheet(tokens.generateStyleSheet());
            current_theme = "2";
            break;
        }

        case AppThemeMode::QDarkStyle: {
            // Legacy QDarkStyle support
            QFile f(":qdarkstyle/dark/darkstyle.qss");
            if (f.open(QFile::ReadOnly | QFile::Text)){
                QTextStream ts(&f);
                qApp->setStyleSheet(ts.readAll());
            }
            current_theme = "3";
            break;
        }

        case AppThemeMode::CustomQt: {
            // Custom Qt style - use system default
            qApp->setStyleSheet("");
            qApp->setStyle(system_style_name);
            current_theme = "99";
            break;
        }
    }

    m_currentMode = mode;
    emit themeChanged(current_theme);
    emit premiumThemeChanged(mode);
}

AppThemeMode ThemeManager::getCurrentThemeMode() const {
    return m_currentMode;
}

void ThemeManager::setPremiumThemeEnabled(bool enabled) {
    if (m_usePremiumTheme == enabled) {
        return;
    }

    m_usePremiumTheme = enabled;

    // Reapply current theme
    if (enabled) {
        applyPremiumTheme(m_currentMode, true);
    } else {
        ApplyTheme(current_theme, true);
    }
}
