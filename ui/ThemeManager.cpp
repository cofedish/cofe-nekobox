#include <QStyle>
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QFont>
#include <QCoreApplication>
#include <QEvent>
#include <QScopedValueRollback>
#include <QTimer>

#include "ThemeManager.hpp"
#include "Typography.hpp"

ThemeManager *themeManager = new ThemeManager;

extern QString ReadFileText(const QString &path);

namespace {
    struct ThemeTokens {
        QString window;
        QString surface;
        QString surface_alt;
        QString border;
        QString text;
        QString text_muted;
        QString accent;
        QString accent_hover;
        QString accent_press;
        QString accent_soft;
        QString text_on_accent;
        QString success;
        QString success_text;
        QString warning;
        QString warning_text;
        QString error;
        QString error_text;
        QString font_family;
    };

    QString NormalizeThemeKey(const QString &theme) {
        const auto key = theme.trimmed().toLower();
        if (key == "0" || key == "system") return "system";
        if (key == "1" || key == "light") return "light";
        if (key == "2" || key == "dark") return "dark";
        if (key == "bad600light" || key == "bad600-light") return "bad600-light";
        if (key == "bad600dark" || key == "bad600-dark") return "bad600-dark";
        return key;
    }

    QString ToRgba(const QColor &color, int alpha) {
        return QString("rgba(%1,%2,%3,%4)")
            .arg(color.red())
            .arg(color.green())
            .arg(color.blue())
            .arg(alpha);
    }

    ThemeTokens TokensLight() {
        ThemeTokens t;
        t.window = "#F8F2FF";
        t.surface = "#FFFFFF";
        t.surface_alt = "#F3E8FF";
        t.border = "#DCC9F2";
        t.text = "#20142E";
        t.text_muted = "#725E89";
        t.accent = "#D946EF";
        t.accent_hover = "#C026D3";
        t.accent_press = "#A21CAF";
        t.accent_soft = "#F5D9FA";
        t.text_on_accent = "#FFFFFF";
        t.success = "#B832E2";
        t.success_text = "#FFFFFF";
        t.warning = "#C17A2B";
        t.warning_text = "#20142E";
        t.error = "#D14B7E";
        t.error_text = "#FFFFFF";
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    ThemeTokens TokensDark() {
        ThemeTokens t;
        t.window = "#090612";
        t.surface = "#130B22";
        t.surface_alt = "#1B1030";
        t.border = "#3E2561";
        t.text = "#F7F1FF";
        t.text_muted = "#BCAAD3";
        t.accent = "#FF4CCF";
        t.accent_hover = "#FF6AD8";
        t.accent_press = "#E53FB8";
        t.accent_soft = "#3A184E";
        t.text_on_accent = "#FFF6FF";
        t.success = "#FF4CCF";
        t.success_text = "#120816";
        t.warning = "#F2AB4B";
        t.warning_text = "#120816";
        t.error = "#FF5D96";
        t.error_text = "#120816";
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    ThemeTokens TokensLucifer() {
        ThemeTokens t;
        t.window = "#0A0510";
        t.surface = "#160A1F";
        t.surface_alt = "#200E2E";
        t.border = "#4A1D53";
        t.text = "#F9F1FF";
        t.text_muted = "#C7A8CE";
        t.accent = "#FF2F8F";
        t.accent_hover = "#FF53A5";
        t.accent_press = "#DE2A7D";
        t.accent_soft = "#3A1239";
        t.text_on_accent = "#FFF0F8";
        t.success = "#DB3CB7";
        t.success_text = "#FFF2FA";
        t.warning = "#D59D46";
        t.warning_text = "#0B0B0E";
        t.error = "#D25580";
        t.error_text = "#FCEFF1";
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    ThemeTokens TokensBad600Light() {
        ThemeTokens t;
        t.window = "#F9FBEF";
        t.surface = "#FFFFFF";
        t.surface_alt = "#EEF3D7";
        t.border = "#CDD9A7";
        t.text = "#1A190F";
        t.text_muted = "#66624F";
        t.accent = "#BAD600";
        t.accent_hover = "#A4BE00";
        t.accent_press = "#8FA700";
        t.accent_soft = "#EAF1B7";
        t.text_on_accent = "#111207";
        t.success = "#7D9800";
        t.success_text = "#111207";
        t.warning = "#C89A2A";
        t.warning_text = "#111207";
        t.error = "#C4563E";
        t.error_text = "#FFFFFF";
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    ThemeTokens TokensBad600Dark() {
        ThemeTokens t;
        t.window = "#0B1107";
        t.surface = "#121A0D";
        t.surface_alt = "#1A2613";
        t.border = "#324A1C";
        t.text = "#EAF2CF";
        t.text_muted = "#B0B997";
        t.accent = "#BAD600";
        t.accent_hover = "#A4BE00";
        t.accent_press = "#8FA700";
        t.accent_soft = "#283612";
        t.text_on_accent = "#101207";
        t.success = "#BAD600";
        t.success_text = "#101207";
        t.warning = "#D4A43B";
        t.warning_text = "#101207";
        t.error = "#D46A55";
        t.error_text = "#101207";
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    ThemeTokens TokensFromPalette(const QPalette &pal) {
        ThemeTokens t;
        auto window = pal.color(QPalette::Window);
        auto text = pal.color(QPalette::WindowText);
        auto muted = pal.color(QPalette::Disabled, QPalette::WindowText);
        auto accent = pal.color(QPalette::Highlight);
        t.window = window.name();
        t.surface = pal.color(QPalette::Base).name();
        t.surface_alt = pal.color(QPalette::AlternateBase).name();
        t.border = window.lightness() < 128 ? "#2A3036" : "#E0DACD";
        t.text = text.name();
        t.text_muted = muted.name();
        t.accent = accent.name();
        t.accent_hover = accent.lighter(110).name();
        t.accent_press = accent.darker(115).name();
        t.accent_soft = ToRgba(accent, 36);
        t.text_on_accent = pal.color(QPalette::HighlightedText).name();
        t.success = accent.name();
        t.success_text = pal.color(QPalette::HighlightedText).name();
        t.warning = accent.lighter(120).name();
        t.warning_text = pal.color(QPalette::WindowText).name();
        t.error = accent.darker(120).name();
        t.error_text = pal.color(QPalette::HighlightedText).name();
        t.font_family = Typography::QssFamilyChain();
        return t;
    }

    QPalette BuildPalette(const ThemeTokens &t) {
        QPalette pal;
        pal.setColor(QPalette::Window, QColor(t.window));
        pal.setColor(QPalette::WindowText, QColor(t.text));
        pal.setColor(QPalette::Base, QColor(t.surface));
        pal.setColor(QPalette::AlternateBase, QColor(t.surface_alt));
        pal.setColor(QPalette::Text, QColor(t.text));
        pal.setColor(QPalette::Button, QColor(t.surface_alt));
        pal.setColor(QPalette::ButtonText, QColor(t.text));
        pal.setColor(QPalette::Highlight, QColor(t.accent));
        pal.setColor(QPalette::HighlightedText, QColor(t.text_on_accent));
        return pal;
    }

    QString BuildThemeQss(const ThemeTokens &t) {
        const int body_font_px = Typography::ScalePx(13);
        const int nav_font_px = Typography::ScalePx(14);
        const int drawer_toggle_font_px = Typography::ScalePx(18);
        const int drawer_title_font_px = Typography::ScalePx(16);
        const int home_title_font_px = Typography::ScalePx(20);
        const int home_status_font_px = Typography::ScalePx(16);
        const int theme_font_px = Typography::ScalePx(12);
        const int regular_weight = QFont::Normal;
        const int medium_weight = QFont::Medium;
        const int semibold_weight = QFont::DemiBold;

        QString qss = QString(
                   "QWidget { font-family: %1; color: %2; font-size: %BODY_FONT_PX%px; font-weight: %WEIGHT_REGULAR%; }"
                   "QWidget#centralwidget { background: %3; }"
                   "QFrame#drawer_container { background: %4; border-right: 1px solid %5; }"
                   "QWidget#drawer_scrim { background: rgba(0,0,0,90); }"
                   "QListWidget#drawer_nav { background: transparent; border: none; }"
                   "QListWidget#drawer_nav::item { padding: 10px 14px; margin: 2px 0; min-height: 42px; border-radius: 12px; color: %6; font-size: %NAV_FONT_PX%px; background: %9; border: 1px solid %5; }"
                   "QListWidget#drawer_nav::item:hover { background: %7; border-color: %11; }"
                   "QListWidget#drawer_nav::item:pressed { background: %13; border-color: %11; }"
                   "QListWidget#drawer_nav::item:selected { background: %7; color: %6; border: 1px solid %11; border-left: 3px solid %11; padding-left: 11px; font-weight: %WEIGHT_SEMIBOLD%; }"
                   "QListWidget#drawer_nav QScrollBar:vertical { background: transparent; width: 10px; margin: 6px 0 6px 0; }"
                   "QListWidget#drawer_nav QScrollBar::handle:vertical { background: %5; min-height: 28px; border-radius: 5px; }"
                   "QListWidget#drawer_nav QScrollBar::handle:vertical:hover { background: %10; }"
                   "QListWidget#drawer_nav QScrollBar::add-line:vertical, QListWidget#drawer_nav QScrollBar::sub-line:vertical { height: 0px; width: 0px; border: none; background: transparent; }"
                   "QListWidget#drawer_nav QScrollBar::add-page:vertical, QListWidget#drawer_nav QScrollBar::sub-page:vertical { background: transparent; }"
                   "QFrame#topbar { background: %9; border: 1px solid %5; border-radius: 12px; padding: 6px; }"
                   "QToolButton#drawer_toggle { background: %9; border: 1px solid %5; border-radius: 12px; font-size: %DRAWER_TOGGLE_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; }"
                   "QToolButton#drawer_toggle:hover { background: %7; }"
                   "QToolButton#drawer_toggle:pressed { background: %13; }"
                   "QLabel#drawer_app_name { font-size: %DRAWER_TITLE_FONT_PX%px; font-weight: %WEIGHT_SEMIBOLD%; color: %2; }"
                   "QLabel#drawer_status, QLabel#drawer_profile { color: %10; }"
                   "QLabel#drawer_status { border-radius: 10px; padding: 2px 8px; }"
                   "QLabel#drawer_status[state=\"connected\"] { background: %7; color: %2; }"
                   "QLabel#drawer_status[state=\"disconnected\"] { background: %9; color: %10; border: 1px solid %5; }"
                   "QLabel#home_title { font-size: %HOME_TITLE_FONT_PX%px; font-weight: %WEIGHT_SEMIBOLD%; }"
                   "QLabel#label_running { font-size: %HOME_STATUS_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; }"
                   "QLabel#label_inbound, QLabel#label_speed { color: %10; }"
                   "QPushButton#home_connect_button { background: %11; color: %12; border: none; border-radius: 18px; padding: 14px 20px; font-size: %HOME_STATUS_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; }"
                   "QPushButton#home_connect_button:hover { background: %13; }"
                   "QPushButton#home_connect_button:pressed { background: %14; }"
                   "QCheckBox { spacing: 8px; }"
                   "QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid %5; border-radius: 4px; background: %9; }"
                   "QCheckBox::indicator:hover { border-color: %11; }"
                   "QCheckBox::indicator:checked { background: %11; border-color: %11; }"
                   "QCheckBox::indicator:checked:hover { background: %13; border-color: %13; }"
                   "QCheckBox::indicator:disabled { background: %4; border-color: %5; }"
                   "QLineEdit#home_sub_url { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; min-height: 36px; }"
                   "QPushButton#home_sub_add { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 14px; min-height: 36px; min-width: 72px; }"
                   "QPushButton#home_sub_add:hover { background: %7; }"
                   "QWidget#page_servers QLineEdit { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; }"
                   "QTabWidget::pane { border: 1px solid %5; border-radius: 12px; }"
                   "QTabBar::tab { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; margin-right: 6px; }"
                   "QTabBar::tab:selected { background: %7; color: %8; }"
                   "QTextBrowser, QTableWidget { background: %9; border: 1px solid %5; border-radius: 12px; }"
                   "QMenu { background: %4; color: %2; border: 1px solid %5; border-radius: 10px; padding: 4px; }"
                   "QMenu::item { padding: 6px 14px; min-height: 32px; border-radius: 6px; }"
                   "QMenu::item:selected { background: %7; color: %2; }"
                   "QMenu::item:checked { background: %11; color: %12; }"
                   "QMenu::item:disabled { color: %10; }"
                   "QMenu::separator { height: 1px; background: %5; margin: 4px 8px; }"
                   "QComboBox { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 28px 6px 10px; min-height: 32px; }"
                   "QComboBox:hover { background: %7; }"
                   "QComboBox:focus { border: 1px solid %11; outline: none; }"
                   "QComboBox:disabled { color: %10; background: %4; }"
                   "QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border-left: 1px solid %5; }"
                   "QComboBox QAbstractItemView { background: %4; border: 1px solid %5; border-radius: 10px; padding: 4px; outline: none; selection-background-color: %11; selection-color: %12; }"
                   "QComboBox QAbstractItemView::item { min-height: 32px; padding: 6px 10px; border-radius: 6px; }"
                   "QComboBox QAbstractItemView::item:hover { background: %7; color: %2; }"
                   "QComboBox QAbstractItemView::item:selected { background: %11; color: %12; }"
                   "QComboBox QAbstractItemView::item:disabled { color: %10; }"
                   "QFrame#toast_widget { background: %4; border: 1px solid %5; border-radius: 10px; }"
                   "QFrame#toast_widget QLabel { color: %2; }"
                   "QFrame#toast_widget[level=\"success\"] { background: %15; color: %16; border-color: %15; }"
                   "QFrame#toast_widget[level=\"warning\"] { background: %17; color: %18; border-color: %17; }"
                   "QFrame#toast_widget[level=\"error\"] { background: %19; color: %20; border-color: %19; }"
                   "QLabel#drawer_theme_label { color: %10; font-size: %THEME_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; }"
                   "QToolButton#drawer_theme_button { background: %9; border: 1px solid %5; border-radius: 10px; padding: 4px 12px; min-height: 32px; font-size: %THEME_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; }"
                   "QToolButton#drawer_theme_button:hover { background: %7; }"
                   "QToolButton#drawer_theme_button:pressed { background: %13; }"
                   "QToolButton#drawer_theme_button:focus { border: 1px solid %11; outline: none; }"
                   "QToolButton#drawer_theme_button::menu-indicator { width: 0px; }"
                   "QMenu#drawer_theme_menu { border-radius: 12px; padding: 6px; }"
                   "QMenu#drawer_theme_menu::item { padding: 0px; margin: 0px; }"
                   "QToolButton#drawer_theme_swatch { background: %4; border: 1px solid %5; border-radius: 14px; padding: 3px; min-width: 28px; min-height: 28px; }"
                   "QToolButton#drawer_theme_swatch:hover { background: %9; border-color: %11; }"
                   "QToolButton#drawer_theme_swatch:pressed { background: %7; }"
                   "QToolButton#drawer_theme_swatch:checked { background: %9; border: 2px solid %11; }"
                   "QToolButton#drawer_theme_swatch:focus { outline: none; }"
                   "QToolButton#home_select_server, QToolButton#home_select_profile, QToolButton#home_open_logs, "
                   "QToolButton#servers_add_button, QToolButton#servers_add_paste {"
                   "background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 12px; min-height: 36px; }"
                   "QToolButton#home_select_server:hover, QToolButton#home_select_profile:hover, QToolButton#home_open_logs:hover, "
                   "QToolButton#servers_add_button:hover, QToolButton#servers_add_paste:hover {"
                   "background: %7; }"
                   "QWidget#page_profiles QPushButton, QWidget#page_subscriptions QPushButton, QWidget#page_rules QPushButton, "
                   "QWidget#page_settings QPushButton, QWidget#page_about QPushButton {"
                   "background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; }"
                   "QWidget#page_profiles QPushButton:hover, QWidget#page_subscriptions QPushButton:hover, QWidget#page_rules QPushButton:hover, "
                   "QWidget#page_settings QPushButton:hover, QWidget#page_about QPushButton:hover {"
                   "background: %7; }");
        qss = qss.arg(t.font_family);
        qss = qss.arg(t.text);
        qss = qss.arg(t.window);
        qss = qss.arg(t.surface);
        qss = qss.arg(t.border);
        qss = qss.arg(t.text);
        qss = qss.arg(t.accent_soft);
        qss = qss.arg(t.text);
        qss = qss.arg(t.surface_alt);
        qss = qss.arg(t.text_muted);
        qss = qss.arg(t.accent);
        qss = qss.arg(t.text_on_accent);
        qss = qss.arg(t.accent_hover);
        qss = qss.arg(t.accent_press);
        qss = qss.arg(t.success);
        qss = qss.arg(t.success_text);
        qss = qss.arg(t.warning);
        qss = qss.arg(t.warning_text);
        qss = qss.arg(t.error);
        qss = qss.arg(t.error_text);
        qss.replace("%BODY_FONT_PX%", QString::number(body_font_px));
        qss.replace("%NAV_FONT_PX%", QString::number(nav_font_px));
        qss.replace("%DRAWER_TOGGLE_FONT_PX%", QString::number(drawer_toggle_font_px));
        qss.replace("%DRAWER_TITLE_FONT_PX%", QString::number(drawer_title_font_px));
        qss.replace("%HOME_TITLE_FONT_PX%", QString::number(home_title_font_px));
        qss.replace("%HOME_STATUS_FONT_PX%", QString::number(home_status_font_px));
        qss.replace("%THEME_FONT_PX%", QString::number(theme_font_px));
        qss.replace("%WEIGHT_REGULAR%", QString::number(regular_weight));
        qss.replace("%WEIGHT_MEDIUM%", QString::number(medium_weight));
        qss.replace("%WEIGHT_SEMIBOLD%", QString::number(semibold_weight));
        const auto glassSurface      = ToRgba(QColor(t.surface), 128);       // ~50% transparent — shows waves
        const auto glassSurfaceSoft  = ToRgba(QColor(t.surface_alt), 108);
        const auto glassBorder       = ToRgba(QColor(t.border), 190);
        const auto accentBorderHover = ToRgba(QColor(t.accent), 200);
        const auto mutedSoft         = ToRgba(QColor(t.text_muted), 112);
        const auto shadowColor       = ToRgba(QColor(t.window).darker(170), 155);
        const auto navItemHover      = ToRgba(QColor(t.accent), 32);
        const auto navItemSelected   = ToRgba(QColor(t.accent), 55);
        const auto sidebarBg         = ToRgba(QColor(t.surface).darker(108), 188);
        const auto cardHoverBg       = ToRgba(QColor(t.surface).lighter(110), 160);
        const auto topbarBg          = ToRgba(QColor(t.surface).darker(110), 200);
        qss += QString(
            "QWidget#centralwidget { background: transparent; }"
            "QWidget#main_container { background: transparent; }"

            // ===== Sidebar =====
            "QFrame#drawer_container {"
            "background: %1;"
            "border-right: 1px solid %2; }"

            // Nav items — compact icon-above-label mode
            "QListWidget#drawer_nav { background: transparent; border: none; padding: 4px 0; outline: none; }"
            "QListWidget#drawer_nav::item {"
            "color: %3; border-radius: 12px; border: 1px solid transparent; }"
            "QListWidget#drawer_nav::item:hover { background: %4; border-color: %5; color: %11; }"
            "QListWidget#drawer_nav::item:selected {"
            "background: %6; border: 1px solid %7; color: %8; }"
            "QListWidget#drawer_nav::item:focus { outline: none; border: 1px solid transparent; }"

            // Theme picker button
            "QToolButton#drawer_theme_button { min-height: 26px; padding: 2px 5px; }"

            // ===== Top bar =====
            "QFrame#topbar {"
            "background: %17;"
            "border: 1px solid %2;"
            "border-radius: 16px;"
            "padding: 6px 14px; }"
            "QLabel#topbar_title {"
            "font-size: %HOME_TITLE_FONT_PX%px; font-weight: %WEIGHT_SEMIBOLD%; letter-spacing: 0.3px; }"
            "QToolButton#drawer_toggle { border-radius: 14px; }"

            // ===== Home page =====
            "QWidget#page_home { background: transparent; }"

            // Center panel: semi-transparent glass showing waves through
            "QFrame#home_center_panel {"
            "background: qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 %10,stop:0.6 %9,stop:1 %18);"
            "border: 1px solid %2; border-radius: 28px; }"

            // Side panel: slightly darker glass
            "QFrame#home_side_panel {"
            "background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 %10,stop:1 %9);"
            "border: 1px solid %2; border-radius: 24px; }"

            // Running label (below connect button)
            "QLabel#label_running {"
            "font-size: %HOME_STATUS_FONT_PX%px; font-weight: %WEIGHT_MEDIUM%; color: %11; }"

            // Side panel section headings
            "QLabel#home_modes_title, QLabel#home_stats_title, QLabel#home_inbound_title {"
            "font-size: %NAV_FONT_PX%px; font-weight: %WEIGHT_SEMIBOLD%; color: %11;"
            "padding-bottom: 2px; }"

            // Side panel body text
            "QLabel#label_speed, QLabel#label_inbound, QLabel#home_modes_text {"
            "color: %3; }"

            // Separators
            "QFrame#home_side_sep_1, QFrame#home_side_sep_2 { color: %2; background: %2; max-height: 1px; }"

            // ===== Server / Profile cards =====
            "QToolButton#home_select_server, QToolButton#home_select_profile {"
            "text-align: left; padding: 12px 16px; min-height: 58px;"
            "background: %9;"
            "border: 1px solid %2;"
            "border-radius: 18px; color: %11; }"
            "QToolButton#home_select_server:hover, QToolButton#home_select_profile:hover {"
            "background: %19;"
            "border: 1px solid %20; }"
            "QToolButton#home_select_server:pressed, QToolButton#home_select_profile:pressed {"
            "background: %6; border-color: %12; }"

            // Logs button (hidden but styled just in case)
            "QToolButton#home_open_logs {"
            "text-align: center; min-width: 66px; min-height: 40px;"
            "background: %9; border: 1px solid %2; border-radius: 12px; }"
            "QToolButton#home_open_logs:hover { background: %4; border-color: %12; }"

            // ===== Subscription row =====
            "QLineEdit#home_sub_url {"
            "background: %9; border: 1px solid %2; border-radius: 14px;"
            "min-height: 42px; padding: 7px 12px; }"
            "QLineEdit#home_sub_url:focus { border-color: %12; }"
            "QPushButton#home_sub_add {"
            "background: %12; color: %13; border: none; border-radius: 14px;"
            "min-height: 42px; min-width: 100px; padding: 7px 18px;"
            "font-weight: %WEIGHT_SEMIBOLD%; letter-spacing: 0.2px; }"
            "QPushButton#home_sub_add:hover { background: %14; }"
            "QPushButton#home_sub_add:pressed { background: %15; }"

            // ===== Toggle switches =====
            "QCheckBox#checkBox_VPN, QCheckBox#checkBox_SystemProxy {"
            "color: %11; padding: 0 6px; spacing: 7px; font-size: %NAV_FONT_PX%px; }"
            "QCheckBox#checkBox_VPN::indicator, QCheckBox#checkBox_SystemProxy::indicator {"
            "width: 38px; height: 22px; border-radius: 11px; border: 1px solid %2; background: %10; }"
            "QCheckBox#checkBox_VPN::indicator:checked, QCheckBox#checkBox_SystemProxy::indicator:checked {"
            "background: %12; border-color: %12; }"
            "QCheckBox#checkBox_VPN::indicator:hover, QCheckBox#checkBox_SystemProxy::indicator:hover {"
            "border-color: %14; }"

            // ===== Other page widgets =====
            "QTabWidget::pane, QTextBrowser, QTableWidget {"
            "background: %9; border: 1px solid %2; border-radius: 14px; }"
            "QPushButton, QToolButton, QComboBox, QLineEdit, QTextEdit, QPlainTextEdit {"
            "selection-background-color: %12; selection-color: %13; }"
            "QMenu { background: %16; border: 1px solid %2; border-radius: 12px; }"
            "QFrame#toast_widget { border-color: %2; }")
                   .arg(sidebarBg)           // %1   sidebar bg
                   .arg(glassBorder)         // %2   border
                   .arg(t.text_muted)        // %3   muted text
                   .arg(navItemHover)        // %4   nav hover bg
                   .arg(mutedSoft)           // %5   nav hover border
                   .arg(navItemSelected)     // %6   nav selected bg
                   .arg(t.accent)            // %7   nav selected border
                   .arg(t.text)              // %8   nav selected text
                   .arg(glassSurface)        // %9   glass surface (50% transparent)
                   .arg(shadowColor)         // %10  deep shadow / gradient dark
                   .arg(t.text)              // %11  primary text
                   .arg(t.accent)            // %12  accent color
                   .arg(t.text_on_accent)    // %13  text on accent
                   .arg(t.accent_hover)      // %14  accent hover
                   .arg(t.accent_press)      // %15  accent press
                   .arg(t.surface)           // %16  menu / popup bg
                   .arg(topbarBg)            // %17  topbar glass bg
                   .arg(glassSurfaceSoft)    // %18  gradient end (soft)
                   .arg(cardHoverBg)         // %19  card hover bg
                   .arg(accentBorderHover);  // %20  card hover border (accent)
        qss.replace("%HOME_TITLE_FONT_PX%", QString::number(home_title_font_px));
        qss.replace("%HOME_STATUS_FONT_PX%", QString::number(home_status_font_px));
        qss.replace("%NAV_FONT_PX%", QString::number(nav_font_px));
        qss.replace("%WEIGHT_SEMIBOLD%", QString::number(semibold_weight));
        qss.replace("%WEIGHT_MEDIUM%", QString::number(medium_weight));
        return qss;
    }
} // namespace

void ThemeManager::ApplyTheme(const QString &theme) {
    if (!event_filter_installed_ && qApp != nullptr) {
        qApp->installEventFilter(this);
        event_filter_installed_ = true;
    }

    if (applying_theme_) return;
    QScopedValueRollback<bool> guard(applying_theme_, true);

    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->objectName();
    }

    auto normalized = NormalizeThemeKey(theme);
    if (this->current_theme == normalized && normalized != "system") {
        return;
    }

    bool handled = false;
    QString base_qss;

    if (normalized == "system" || normalized == "light" || normalized == "dark" || normalized == "lucifer" ||
        normalized == "bad600-light" || normalized == "bad600-dark") {
        auto system_style = QStyleFactory::create(this->system_style_name);
        if (system_style != nullptr) {
            qApp->setStyle(system_style);
        }
        ThemeTokens tokens = TokensLight();
        if (normalized == "system") {
            auto palette = system_style != nullptr ? system_style->standardPalette() : qApp->palette();
            qApp->setPalette(palette);
            tokens = TokensFromPalette(palette);
        } else if (normalized == "lucifer") {
            tokens = TokensLucifer();
            qApp->setPalette(BuildPalette(tokens));
        } else if (normalized == "bad600-dark") {
            tokens = TokensBad600Dark();
            qApp->setPalette(BuildPalette(tokens));
        } else if (normalized == "bad600-light") {
            tokens = TokensBad600Light();
            qApp->setPalette(BuildPalette(tokens));
        } else if (normalized == "dark") {
            tokens = TokensDark();
            qApp->setPalette(BuildPalette(tokens));
        } else {
            tokens = TokensLight();
            qApp->setPalette(BuildPalette(tokens));
        }
        base_qss = BuildThemeQss(tokens);
        handled = true;
    } else {
        bool ok;
        auto themeId = theme.toInt(&ok);
        if (ok) {
            QString qss;
            if (themeId != 0) {
                QString path;
                std::map<QString, QString> replace;
                switch (themeId) {
                    case 1:
                        path = ":/themes/feiyangqingyun/qss/flatgray.css";
                        replace[":/qss/"] = ":/themes/feiyangqingyun/qss/";
                        break;
                    case 2:
                        path = ":/themes/feiyangqingyun/qss/lightblue.css";
                        replace[":/qss/"] = ":/themes/feiyangqingyun/qss/";
                        break;
                    case 3:
                        path = ":/themes/feiyangqingyun/qss/blacksoft.css";
                        replace[":/qss/"] = ":/themes/feiyangqingyun/qss/";
                        break;
                    default:
                        return;
                }
                qss = ReadFileText(path);
                for (auto const &[a, b]: replace) {
                    qss = qss.replace(a, b);
                }
            }
            auto system_style = QStyleFactory::create(this->system_style_name);
            if (themeId == 0) {
                if (system_style != nullptr) {
                    qApp->setPalette(system_style->standardPalette());
                    qApp->setStyle(system_style);
                }
                qApp->setStyleSheet("");
            } else {
                if (themeId == 1 || themeId == 2 || themeId == 3) {
                    QString paletteColor = qss.mid(20, 7);
                    qApp->setPalette(QPalette(paletteColor));
                } else if (system_style != nullptr) {
                    qApp->setPalette(system_style->standardPalette());
                }
                qApp->setStyleSheet(qss);
            }
        } else {
            const auto &_style = QStyleFactory::create(theme);
            if (_style != nullptr) {
                qApp->setPalette(_style->standardPalette());
                qApp->setStyle(_style);
                qApp->setStyleSheet("");
            }
        }
    }

    current_theme = normalized;
    emit themeChanged(normalized);

    auto cofebox_css = ReadFileText(":/cofebox/cofebox.css");
    if (handled) {
        qApp->setStyleSheet(base_qss.append("\n").append(cofebox_css));
    } else {
        qApp->setStyleSheet(qApp->styleSheet().append("\n").append(cofebox_css));
    }
}

bool ThemeManager::eventFilter(QObject *watched, QEvent *event) {
    if (watched == qApp && !applying_theme_ && NormalizeThemeKey(current_theme) == "system") {
        const auto type = event->type();
        if (type == QEvent::ApplicationPaletteChange || type == QEvent::ThemeChange || type == QEvent::StyleChange) {
            QTimer::singleShot(0, this, [this] {
                if (NormalizeThemeKey(current_theme) == "system") {
                    ApplyTheme("system");
                }
            });
        }
    }
    return QObject::eventFilter(watched, event);
}

ThemeOption ThemeManager::ThemeOptionFor(const QString &theme) const {
    const auto normalized = NormalizeThemeKey(theme);
    ThemeTokens tokens = TokensLight();
    if (normalized == "system") {
        auto palette = qApp->style() ? qApp->style()->standardPalette() : qApp->palette();
        tokens = TokensFromPalette(palette);
    } else if (normalized == "dark") {
        tokens = TokensDark();
    } else if (normalized == "lucifer") {
        tokens = TokensLucifer();
    } else if (normalized == "bad600-light") {
        tokens = TokensBad600Light();
    } else if (normalized == "bad600-dark") {
        tokens = TokensBad600Dark();
    } else {
        tokens = TokensLight();
    }

    ThemeOption option;
    option.id = normalized;
    if (normalized == "system") {
        option.displayName = QCoreApplication::translate("MainWindow", "System");
    } else if (normalized == "light") {
        option.displayName = QCoreApplication::translate("MainWindow", "Light");
    } else if (normalized == "dark") {
        option.displayName = QCoreApplication::translate("MainWindow", "Dark");
    } else if (normalized == "lucifer") {
        option.displayName = QCoreApplication::translate("MainWindow", "Lucifer");
    } else if (normalized == "bad600-light") {
        option.displayName = QCoreApplication::translate("MainWindow", "BAD600 Light");
    } else if (normalized == "bad600-dark") {
        option.displayName = QCoreApplication::translate("MainWindow", "BAD600 Dark");
    } else {
        option.displayName = normalized;
    }
    option.window = QColor(tokens.window);
    option.surface = QColor(tokens.surface);
    option.accent = QColor(tokens.accent);
    option.text = QColor(tokens.text);
    return option;
}

QVector<ThemeOption> ThemeManager::AvailableThemes() const {
    return {
        ThemeOptionFor("system"),
        ThemeOptionFor("light"),
        ThemeOptionFor("dark"),
        ThemeOptionFor("lucifer"),
        ThemeOptionFor("bad600-light"),
        ThemeOptionFor("bad600-dark")
    };
}
