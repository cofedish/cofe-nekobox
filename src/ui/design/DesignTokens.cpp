#include "nekobox/ui/design/DesignTokens.hpp"
#include <QApplication>
#include <QFont>
#include <QFontDatabase>

DesignTokens& DesignTokens::instance() {
    static DesignTokens instance;
    return instance;
}

DesignTokens::DesignTokens() {
    initializeTypography();
    initializeShadows();
    applyDarkTheme(); // Default to dark theme
}

void DesignTokens::setThemeMode(ThemeMode mode) {
    m_currentMode = mode;

    if (mode == ThemeMode::Light) {
        applyLightTheme();
    } else if (mode == ThemeMode::Dark) {
        applyDarkTheme();
    } else {
        // System mode - detect OS preference
        // For now, default to dark
        applyDarkTheme();
    }
}

void DesignTokens::applyLightTheme() {
    // Premium Light Theme - Clean, modern, spacious

    // Primary - Deep blue with sophistication
    m_colors.primary = QColor("#0066CC");
    m_colors.primaryHover = QColor("#0052A3");
    m_colors.primaryPressed = QColor("#004080");
    m_colors.primaryDisabled = QColor("#B3D9FF");

    // Accent - Vibrant but tasteful
    m_colors.accent = QColor("#FF6B35");
    m_colors.accentHover = QColor("#E55A28");
    m_colors.accentPressed = QColor("#CC4D1F");

    // Semantic
    m_colors.success = QColor("#00C853");
    m_colors.warning = QColor("#FFB300");
    m_colors.error = QColor("#D32F2F");
    m_colors.info = QColor("#0288D1");

    // Surfaces - Crisp whites with subtle elevations
    m_colors.surface = QColor("#FFFFFF");
    m_colors.surfaceElevated = QColor("#FAFAFA");
    m_colors.surfaceHover = QColor("#F5F5F5");
    m_colors.surfacePressed = QColor("#EEEEEE");
    m_colors.surfaceDisabled = QColor("#E0E0E0");

    // Backgrounds - Soft neutrals
    m_colors.background = QColor("#F8F9FA");
    m_colors.backgroundSecondary = QColor("#ECEFF1");
    m_colors.backgroundTertiary = QColor("#E0E3E5");

    // Borders - Subtle definition
    m_colors.border = QColor("#E0E0E0");
    m_colors.borderFocus = QColor("#0066CC");
    m_colors.borderHover = QColor("#BDBDBD");
    m_colors.borderDisabled = QColor("#F0F0F0");

    // Text - Clear hierarchy
    m_colors.textPrimary = QColor("#1A1A1A");
    m_colors.textSecondary = QColor("#666666");
    m_colors.textTertiary = QColor("#999999");
    m_colors.textDisabled = QColor("#CCCCCC");
    m_colors.textOnPrimary = QColor("#FFFFFF");
    m_colors.textOnAccent = QColor("#FFFFFF");

    // Overlays & Shadows
    m_colors.overlay = QColor(0, 0, 0, 100); // 40% opacity
    m_colors.shadow = QColor(0, 0, 0, 25);   // 10% opacity
    m_colors.shadowElevated = QColor(0, 0, 0, 40); // 16% opacity

    // Special States
    m_colors.selectedBackground = QColor("#E3F2FD");
    m_colors.selectedText = QColor("#0066CC");
    m_colors.hoverBackground = QColor("#F5F5F5");
}

void DesignTokens::applyDarkTheme() {
    // Premium Dark Theme - Deep, elegant, premium feel

    // Primary - Bright accent that pops on dark
    m_colors.primary = QColor("#3B82F6");
    m_colors.primaryHover = QColor("#60A5FA");
    m_colors.primaryPressed = QColor("#2563EB");
    m_colors.primaryDisabled = QColor("#1E3A5F");

    // Accent - Warm complement
    m_colors.accent = QColor("#F59E0B");
    m_colors.accentHover = QColor("#FBBF24");
    m_colors.accentPressed = QColor("#D97706");

    // Semantic
    m_colors.success = QColor("#10B981");
    m_colors.warning = QColor("#F59E0B");
    m_colors.error = QColor("#EF4444");
    m_colors.info = QColor("#3B82F6");

    // Surfaces - Layered depth
    m_colors.surface = QColor("#1E1E2E");
    m_colors.surfaceElevated = QColor("#252535");
    m_colors.surfaceHover = QColor("#2C2C3C");
    m_colors.surfacePressed = QColor("#323242");
    m_colors.surfaceDisabled = QColor("#191927");

    // Backgrounds - Deep foundation
    m_colors.background = QColor("#13131A");
    m_colors.backgroundSecondary = QColor("#1A1A24");
    m_colors.backgroundTertiary = QColor("#1E1E2E");

    // Borders - Subtle but defined
    m_colors.border = QColor("#2E2E3E");
    m_colors.borderFocus = QColor("#3B82F6");
    m_colors.borderHover = QColor("#3A3A4A");
    m_colors.borderDisabled = QColor("#262632");

    // Text - Comfortable reading
    m_colors.textPrimary = QColor("#E5E7EB");
    m_colors.textSecondary = QColor("#9CA3AF");
    m_colors.textTertiary = QColor("#6B7280");
    m_colors.textDisabled = QColor("#4B5563");
    m_colors.textOnPrimary = QColor("#FFFFFF");
    m_colors.textOnAccent = QColor("#000000");

    // Overlays & Shadows
    m_colors.overlay = QColor(0, 0, 0, 150); // 60% opacity
    m_colors.shadow = QColor(0, 0, 0, 80);   // 31% opacity
    m_colors.shadowElevated = QColor(0, 0, 0, 100); // 40% opacity

    // Special States
    m_colors.selectedBackground = QColor("#1E3A5F");
    m_colors.selectedText = QColor("#60A5FA");
    m_colors.hoverBackground = QColor("#252535");
}

void DesignTokens::initializeTypography() {
    // Use system fonts for better integration and performance
    QString systemFont = QApplication::font().family();

    // Display styles
    m_typography.displayLarge = {systemFont, 32, QFont::Bold, 40};
    m_typography.displayMedium = {systemFont, 24, QFont::Bold, 32};

    // Headline styles
    m_typography.headlineLarge = {systemFont, 20, QFont::DemiBold, 28};
    m_typography.headlineMedium = {systemFont, 18, QFont::DemiBold, 24};

    // Title styles
    m_typography.titleLarge = {systemFont, 16, QFont::DemiBold, 24};
    m_typography.titleMedium = {systemFont, 14, QFont::DemiBold, 20};

    // Body styles
    m_typography.bodyLarge = {systemFont, 14, QFont::Normal, 22};
    m_typography.bodyMedium = {systemFont, 13, QFont::Normal, 20};

    // Label styles
    m_typography.labelLarge = {systemFont, 14, QFont::Medium, 20};
    m_typography.labelMedium = {systemFont, 12, QFont::Medium, 16};
    m_typography.labelSmall = {systemFont, 11, QFont::Normal, 16};
}

void DesignTokens::initializeShadows() {
    m_shadows.none = {0, 0, 0, 0, QColor(0, 0, 0, 0)};
    m_shadows.sm = {0, 1, 2, 0, m_colors.shadow};
    m_shadows.md = {0, 2, 8, 0, m_colors.shadow};
    m_shadows.lg = {0, 4, 16, 0, m_colors.shadowElevated};
    m_shadows.xl = {0, 8, 32, 0, m_colors.shadowElevated};
}

QString DesignTokens::generateStyleSheet() const {
    QString qss;

    // Global resets and defaults
    qss += R"(
* {
    padding: 0px;
    margin: 0px;
    border: 0px;
    border-style: none;
    outline: 0;
}

QWidget {
    background-color: )" + m_colors.background.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
    font-family: ')" + m_typography.bodyMedium.family + R"(';
    font-size: )" + QString::number(m_typography.bodyMedium.size) + R"(px;
    selection-background-color: )" + m_colors.selectedBackground.name() + R"(;
    selection-color: )" + m_colors.selectedText.name() + R"(;
}

/* Cards and elevated surfaces */
QFrame[frameShape="0"][frameShadow="16"],
QFrame[frameShape="1"] {
    background-color: )" + m_colors.surface.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
}

/* Push Buttons - Premium style */
QPushButton {
    background-color: )" + m_colors.surface.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
    padding: )" + QString::number(m_spacing.sm) + R"(px )" + QString::number(m_spacing.md) + R"(px;
    font-weight: 500;
    min-height: 32px;
}

QPushButton:hover {
    background-color: )" + m_colors.surfaceHover.name() + R"(;
    border-color: )" + m_colors.borderHover.name() + R"(;
}

QPushButton:pressed {
    background-color: )" + m_colors.surfacePressed.name() + R"(;
}

QPushButton:disabled {
    background-color: )" + m_colors.surfaceDisabled.name() + R"(;
    color: )" + m_colors.textDisabled.name() + R"(;
    border-color: )" + m_colors.borderDisabled.name() + R"(;
}

QPushButton:default,
QPushButton[primary="true"] {
    background-color: )" + m_colors.primary.name() + R"(;
    color: )" + m_colors.textOnPrimary.name() + R"(;
    border: none;
}

QPushButton:default:hover,
QPushButton[primary="true"]:hover {
    background-color: )" + m_colors.primaryHover.name() + R"(;
}

QPushButton:default:pressed,
QPushButton[primary="true"]:pressed {
    background-color: )" + m_colors.primaryPressed.name() + R"(;
}

/* Line Edits and Text Inputs */
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
    background-color: )" + m_colors.surface.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
    padding: )" + QString::number(m_spacing.sm) + R"(px;
    min-height: 28px;
}

QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus {
    border-color: )" + m_colors.borderFocus.name() + R"(;
    border-width: 2px;
    padding: )" + QString::number(m_spacing.sm - 1) + R"(px;
}

QLineEdit:disabled, QTextEdit:disabled, QPlainTextEdit:disabled {
    background-color: )" + m_colors.surfaceDisabled.name() + R"(;
    color: )" + m_colors.textDisabled.name() + R"(;
}

/* Combo Boxes */
QComboBox {
    background-color: )" + m_colors.surface.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
    padding: )" + QString::number(m_spacing.sm) + R"(px;
    min-height: 28px;
}

QComboBox:hover {
    border-color: )" + m_colors.borderHover.name() + R"(;
}

QComboBox:focus {
    border-color: )" + m_colors.borderFocus.name() + R"(;
}

QComboBox::drop-down {
    border: none;
    padding-right: )" + QString::number(m_spacing.sm) + R"(px;
}

/* Checkboxes and Radio Buttons */
QCheckBox, QRadioButton {
    spacing: )" + QString::number(m_spacing.sm) + R"(px;
    color: )" + m_colors.textPrimary.name() + R"(;
}

QCheckBox::indicator, QRadioButton::indicator {
    width: 18px;
    height: 18px;
    border: 2px solid )" + m_colors.border.name() + R"(;
    background-color: )" + m_colors.surface.name() + R"(;
}

QCheckBox::indicator {
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
}

QRadioButton::indicator {
    border-radius: 9px;
}

QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background-color: )" + m_colors.primary.name() + R"(;
    border-color: )" + m_colors.primary.name() + R"(;
}

/* Tab Widget - Modern card-based tabs */
QTabWidget::pane {
    background-color: )" + m_colors.surface.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
    padding: )" + QString::number(m_spacing.md) + R"(px;
}

QTabBar::tab {
    background-color: transparent;
    color: )" + m_colors.textSecondary.name() + R"(;
    padding: )" + QString::number(m_spacing.sm) + R"(px )" + QString::number(m_spacing.md) + R"(px;
    margin-right: )" + QString::number(m_spacing.xs) + R"(px;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
    font-weight: 500;
}

QTabBar::tab:hover {
    background-color: )" + m_colors.hoverBackground.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
}

QTabBar::tab:selected {
    background-color: )" + m_colors.selectedBackground.name() + R"(;
    color: )" + m_colors.selectedText.name() + R"(;
}

/* List and Tree Widgets */
QListWidget, QTreeWidget, QTableWidget {
    background-color: )" + m_colors.surface.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
    color: )" + m_colors.textPrimary.name() + R"(;
}

QListWidget::item, QTreeWidget::item, QTableWidget::item {
    padding: )" + QString::number(m_spacing.sm) + R"(px;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
}

QListWidget::item:hover, QTreeWidget::item:hover, QTableWidget::item:hover {
    background-color: )" + m_colors.hoverBackground.name() + R"(;
}

QListWidget::item:selected, QTreeWidget::item:selected, QTableWidget::item:selected {
    background-color: )" + m_colors.selectedBackground.name() + R"(;
    color: )" + m_colors.selectedText.name() + R"(;
}

/* Scroll Bars - Minimal modern style */
QScrollBar:vertical {
    background-color: transparent;
    width: 12px;
    margin: 0px;
}

QScrollBar::handle:vertical {
    background-color: )" + m_colors.border.name() + R"(;
    border-radius: 6px;
    min-height: 20px;
    margin: 2px;
}

QScrollBar::handle:vertical:hover {
    background-color: )" + m_colors.borderHover.name() + R"(;
}

QScrollBar:horizontal {
    background-color: transparent;
    height: 12px;
    margin: 0px;
}

QScrollBar::handle:horizontal {
    background-color: )" + m_colors.border.name() + R"(;
    border-radius: 6px;
    min-width: 20px;
    margin: 2px;
}

QScrollBar::handle:horizontal:hover {
    background-color: )" + m_colors.borderHover.name() + R"(;
}

QScrollBar::add-line, QScrollBar::sub-line {
    height: 0px;
    width: 0px;
}

/* Progress Bar */
QProgressBar {
    background-color: )" + m_colors.surfaceDisabled.name() + R"(;
    border: none;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
    text-align: center;
    height: 8px;
}

QProgressBar::chunk {
    background-color: )" + m_colors.primary.name() + R"(;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
}

/* Tooltips */
QToolTip {
    background-color: )" + m_colors.surfaceElevated.name() + R"(;
    color: )" + m_colors.textPrimary.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
    padding: )" + QString::number(m_spacing.xs) + R"(px )" + QString::number(m_spacing.sm) + R"(px;
}

/* Menu */
QMenu {
    background-color: )" + m_colors.surfaceElevated.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
    padding: )" + QString::number(m_spacing.xs) + R"(px;
}

QMenu::item {
    padding: )" + QString::number(m_spacing.sm) + R"(px )" + QString::number(m_spacing.md) + R"(px;
    border-radius: )" + QString::number(m_radius.sm) + R"(px;
}

QMenu::item:selected {
    background-color: )" + m_colors.selectedBackground.name() + R"(;
    color: )" + m_colors.selectedText.name() + R"(;
}

/* Dialogs */
QDialog {
    background-color: )" + m_colors.background.name() + R"(;
}

QDialogButtonBox QPushButton {
    min-width: 80px;
}

/* Status Bar */
QStatusBar {
    background-color: )" + m_colors.surface.name() + R"(;
    border-top: 1px solid )" + m_colors.border.name() + R"(;
    color: )" + m_colors.textSecondary.name() + R"(;
}

/* Group Box */
QGroupBox {
    background-color: )" + m_colors.surface.name() + R"(;
    border: 1px solid )" + m_colors.border.name() + R"(;
    border-radius: )" + QString::number(m_radius.md) + R"(px;
    margin-top: )" + QString::number(m_spacing.md) + R"(px;
    padding: )" + QString::number(m_spacing.md) + R"(px;
    font-weight: 600;
}

QGroupBox::title {
    color: )" + m_colors.textPrimary.name() + R"(;
    subcontrol-origin: margin;
    subcontrol-position: top left;
    padding: 0 )" + QString::number(m_spacing.sm) + R"(px;
    background-color: )" + m_colors.surface.name() + R"(;
}
)";

    return qss;
}
