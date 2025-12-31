#pragma once

#include <QColor>
#include <QString>
#include <QMap>

/**
 * NekoBox Premium Design System - Color Tokens
 * Modern, minimalist, premium aesthetic inspired by high-end control panels
 * Supports Light and Dark themes with smooth transitions
 */

enum class ThemeMode {
    Light,
    Dark,
    System
};

struct ColorTokens {
    // Primary Brand Colors
    QColor primary;
    QColor primaryHover;
    QColor primaryPressed;
    QColor primaryDisabled;

    // Accent Colors
    QColor accent;
    QColor accentHover;
    QColor accentPressed;

    // Semantic Colors
    QColor success;
    QColor warning;
    QColor error;
    QColor info;

    // Neutral Colors - Surfaces
    QColor surface;
    QColor surfaceElevated;
    QColor surfaceHover;
    QColor surfacePressed;
    QColor surfaceDisabled;

    // Neutral Colors - Backgrounds
    QColor background;
    QColor backgroundSecondary;
    QColor backgroundTertiary;

    // Borders
    QColor border;
    QColor borderFocus;
    QColor borderHover;
    QColor borderDisabled;

    // Text
    QColor textPrimary;
    QColor textSecondary;
    QColor textTertiary;
    QColor textDisabled;
    QColor textOnPrimary;
    QColor textOnAccent;

    // Overlays & Shadows
    QColor overlay;
    QColor shadow;
    QColor shadowElevated;

    // Special States
    QColor selectedBackground;
    QColor selectedText;
    QColor hoverBackground;
};

struct SpacingTokens {
    int xs = 4;
    int sm = 8;
    int md = 16;
    int lg = 24;
    int xl = 32;
    int xxl = 48;
};

struct RadiusTokens {
    int none = 0;
    int sm = 4;
    int md = 8;
    int lg = 12;
    int xl = 16;
    int full = 9999;
};

struct TypographyTokens {
    struct FontStyle {
        QString family;
        int size;
        int weight;
        int lineHeight;
    };

    FontStyle displayLarge;   // Page titles
    FontStyle displayMedium;  // Section headers
    FontStyle headlineLarge;  // Card headers
    FontStyle headlineMedium; // Dialog titles
    FontStyle titleLarge;     // List headers
    FontStyle titleMedium;    // Subheaders
    FontStyle bodyLarge;      // Primary content
    FontStyle bodyMedium;     // Secondary content
    FontStyle labelLarge;     // Buttons, tabs
    FontStyle labelMedium;    // Form labels
    FontStyle labelSmall;     // Captions, hints
};

struct ShadowTokens {
    struct Shadow {
        int offsetX;
        int offsetY;
        int blur;
        int spread;
        QColor color;
    };

    Shadow none;
    Shadow sm;
    Shadow md;
    Shadow lg;
    Shadow xl;
};

class DesignTokens {
public:
    static DesignTokens& instance();

    ColorTokens colors() const { return m_colors; }
    SpacingTokens spacing() const { return m_spacing; }
    RadiusTokens radius() const { return m_radius; }
    TypographyTokens typography() const { return m_typography; }
    ShadowTokens shadows() const { return m_shadows; }

    ThemeMode currentMode() const { return m_currentMode; }
    void setThemeMode(ThemeMode mode);

    // Generate complete stylesheet from tokens
    QString generateStyleSheet() const;

private:
    DesignTokens();
    void applyLightTheme();
    void applyDarkTheme();
    void initializeTypography();
    void initializeShadows();

    ColorTokens m_colors;
    SpacingTokens m_spacing;
    RadiusTokens m_radius;
    TypographyTokens m_typography;
    ShadowTokens m_shadows;
    ThemeMode m_currentMode = ThemeMode::System;
};
