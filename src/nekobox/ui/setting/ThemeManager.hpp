#pragma once

#include <QObject>

enum class AppThemeMode {
    System = 0,
    Light = 1,
    Dark = 2,
    QDarkStyle = 3,  // Legacy support
    CustomQt = 99    // QStyleFactory custom styles
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    QString system_style_name = "";
    QString current_theme = "0"; // int: 0:system 1:light 2:dark 3:qdarkstyle string: QStyleFactory

    // Legacy method - maintained for compatibility
    void ApplyTheme(const QString &theme, bool force = false);

    // New premium design system methods
    void applyPremiumTheme(AppThemeMode mode, bool force = false);
    AppThemeMode getCurrentThemeMode() const;
    bool isPremiumThemeActive() const { return m_usePremiumTheme; }
    void setPremiumThemeEnabled(bool enabled);

signals:
    void themeChanged(QString themeName);
    void premiumThemeChanged(AppThemeMode mode);

private:
    bool m_usePremiumTheme = true; // Default to new premium design
    AppThemeMode m_currentMode = AppThemeMode::Dark;
};

extern ThemeManager *themeManager;
