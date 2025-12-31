#include <QStyle>
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>

#include "ThemeManager.hpp"

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
        if (theme == "0") return "system";
        if (theme == "1") return "light";
        if (theme == "2") return "dark";
        return theme;
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
        t.window = "#F6F4EF";
        t.surface = "#FFFFFF";
        t.surface_alt = "#F0ECE5";
        t.border = "#E0DACD";
        t.text = "#1C1914";
        t.text_muted = "#6D655B";
        t.accent = "#2F7D70";
        t.accent_hover = "#296D62";
        t.accent_press = "#235E55";
        t.accent_soft = "#D7EAE6";
        t.text_on_accent = "#FFFFFF";
        t.success = "#2F7D70";
        t.success_text = "#FFFFFF";
        t.warning = "#C08B2E";
        t.warning_text = "#1C1914";
        t.error = "#C44C4C";
        t.error_text = "#FFFFFF";
        t.font_family = "\"Segoe UI Variable\",\"Segoe UI\",\"Inter\",\"Noto Sans\",\"Arial\"";
        return t;
    }

    ThemeTokens TokensDark() {
        ThemeTokens t;
        t.window = "#101214";
        t.surface = "#15181B";
        t.surface_alt = "#1B1F23";
        t.border = "#2A3036";
        t.text = "#E9ECEF";
        t.text_muted = "#9AA3AB";
        t.accent = "#4CC2B2";
        t.accent_hover = "#3BA89A";
        t.accent_press = "#2F8F83";
        t.accent_soft = "#1C3A38";
        t.text_on_accent = "#0C1114";
        t.success = "#4CC2B2";
        t.success_text = "#0C1114";
        t.warning = "#D1A14A";
        t.warning_text = "#0C1114";
        t.error = "#D16060";
        t.error_text = "#0C1114";
        t.font_family = "\"Segoe UI Variable\",\"Segoe UI\",\"Inter\",\"Noto Sans\",\"Arial\"";
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
        t.font_family = "\"Segoe UI Variable\",\"Segoe UI\",\"Inter\",\"Noto Sans\",\"Arial\"";
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
        QString qss = QString(
                   "QWidget { font-family: %1; color: %2; }"
                   "QWidget#centralwidget { background: %3; }"
                   "QFrame#drawer_container { background: %4; border-right: 1px solid %5; }"
                   "QWidget#drawer_scrim { background: rgba(0,0,0,90); }"
                   "QListWidget#drawer_nav { background: transparent; border: none; }"
                   "QListWidget#drawer_nav::item { padding: 8px 12px; min-height: 40px; border-radius: 10px; color: %6; }"
                   "QListWidget#drawer_nav::item:hover { background: %9; }"
                   "QListWidget#drawer_nav::item:selected { background: %7; color: %8; border-left: 3px solid %11; padding-left: 9px; }"
                   "QFrame#topbar { background: %9; border: 1px solid %5; border-radius: 12px; padding: 6px; }"
                   "QToolButton#drawer_toggle { background: %9; border: 1px solid %5; border-radius: 12px; font-size: 18px; font-weight: 600; }"
                   "QToolButton#drawer_toggle:hover { background: %7; }"
                   "QToolButton#drawer_toggle:pressed { background: %13; }"
                   "QLabel#drawer_app_name { font-size: 16px; font-weight: 600; color: %2; }"
                   "QLabel#drawer_status, QLabel#drawer_profile { color: %10; }"
                   "QLabel#drawer_status { border-radius: 10px; padding: 2px 8px; }"
                   "QLabel#drawer_status[state=\"connected\"] { background: %7; color: %2; }"
                   "QLabel#drawer_status[state=\"disconnected\"] { background: %9; color: %10; border: 1px solid %5; }"
                   "QLabel#home_title { font-size: 20px; font-weight: 600; }"
                   "QLabel#label_running { font-size: 16px; font-weight: 600; }"
                   "QLabel#label_inbound, QLabel#label_speed { color: %10; }"
                   "QPushButton#home_connect_button { background: %11; color: %12; border: none; border-radius: 18px; padding: 14px 20px; font-size: 16px; font-weight: 600; }"
                   "QPushButton#home_connect_button:hover { background: %13; }"
                   "QPushButton#home_connect_button:pressed { background: %14; }"
                   "QLineEdit#home_sub_url { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; min-height: 36px; }"
                   "QPushButton#home_sub_add { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 14px; min-height: 36px; min-width: 72px; }"
                   "QPushButton#home_sub_add:hover { background: %7; }"
                   "QWidget#page_servers QLineEdit { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; }"
                   "QTabWidget::pane { border: 1px solid %5; border-radius: 12px; }"
                   "QTabBar::tab { background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 10px; margin-right: 6px; }"
                   "QTabBar::tab:selected { background: %7; color: %8; }"
                   "QTextBrowser, QTableWidget { background: %9; border: 1px solid %5; border-radius: 12px; }"
                   "QMenu { background: %4; color: %2; border: 1px solid %5; padding: 4px; }"
                   "QMenu::item { padding: 6px 16px; border-radius: 6px; }"
                   "QMenu::item:selected { background: %7; color: %8; }"
                   "QMenu::separator { height: 1px; background: %5; margin: 4px 8px; }"
                   "QFrame#toast_widget { background: %4; border: 1px solid %5; border-radius: 10px; }"
                   "QFrame#toast_widget QLabel { color: %2; }"
                   "QFrame#toast_widget[level=\"success\"] { background: %15; color: %16; border-color: %15; }"
                   "QFrame#toast_widget[level=\"warning\"] { background: %17; color: %18; border-color: %17; }"
                   "QFrame#toast_widget[level=\"error\"] { background: %19; color: %20; border-color: %19; }"
                   "QToolButton#drawer_quick_logs, QToolButton#drawer_quick_settings, "
                   "QToolButton#home_select_server, QToolButton#home_select_profile, QToolButton#home_open_logs {"
                   "background: %9; border: 1px solid %5; border-radius: 10px; padding: 6px 12px; min-height: 36px; }"
                   "QToolButton#drawer_quick_logs:hover, QToolButton#drawer_quick_settings:hover, "
                   "QToolButton#home_select_server:hover, QToolButton#home_select_profile:hover, QToolButton#home_open_logs:hover {"
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
        return qss;
    }
} // namespace

void ThemeManager::ApplyTheme(const QString &theme) {
    if (this->system_style_name.isEmpty()) {
        this->system_style_name = qApp->style()->objectName();
    }

    auto normalized = NormalizeThemeKey(theme);
    if (this->current_theme == normalized) {
        return;
    }

    bool handled = false;
    QString base_qss;

    if (normalized == "system" || normalized == "light" || normalized == "dark") {
        auto system_style = QStyleFactory::create(this->system_style_name);
        if (system_style != nullptr) {
            qApp->setStyle(system_style);
        }
        ThemeTokens tokens = TokensLight();
        if (normalized == "system") {
            auto palette = system_style != nullptr ? system_style->standardPalette() : qApp->palette();
            tokens = TokensFromPalette(palette);
            qApp->setPalette(palette);
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

    auto nekoray_css = ReadFileText(":/neko/neko.css");
    if (handled) {
        qApp->setStyleSheet(base_qss.append("\n").append(nekoray_css));
    } else {
        qApp->setStyleSheet(qApp->styleSheet().append("\n").append(nekoray_css));
    }
}
