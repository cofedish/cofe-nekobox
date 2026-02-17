#include "Typography.hpp"

#include <QApplication>
#include <QFontDatabase>
#include <QFontInfo>
#include <QGuiApplication>
#include <QScreen>
#include <QStringList>

namespace {
QStringList PreferredFamilies() {
#ifdef Q_OS_WIN
    return {"Segoe UI", "Inter", "Noto Sans", "Arial"};
#else
    return {"Noto Sans", "DejaVu Sans", "Inter", "Arial"};
#endif
}

QString ResolveFamily() {
    QFontDatabase fontDb;
    for (const auto &family : PreferredFamilies()) {
        if (fontDb.hasFamily(family)) {
            return family;
        }
    }
    return QStringLiteral("Sans Serif");
}

qreal DpiScale() {
    const auto screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) return 1.0;
    constexpr qreal baseDpi = 96.0;
    const qreal scale = screen->logicalDotsPerInchY() / baseDpi;
    return qBound(1.0, scale, 2.0);
}

QFont MakeFont(int logicalPx, int weight, const QFont &fallback) {
    QFont font = fallback;
    font.setFamily(ResolveFamily());
    font.setPixelSize(qMax(8, qRound(logicalPx * DpiScale())));
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setHintingPreference(QFont::PreferFullHinting);
    return font;
}
} // namespace

namespace Typography {

int ScalePx(int px) {
    return qMax(1, qRound(px * DpiScale()));
}

QString QssFamilyChain() {
    QStringList chain;
    for (const auto &family : PreferredFamilies()) {
        chain << QStringLiteral("\"%1\"").arg(family);
    }
    chain << QStringLiteral("sans-serif");
    return chain.join(',');
}

QFont FontForRole(Role role, const QFont &fallback) {
    switch (role) {
        case Role::AppBase:
            return MakeFont(13, QFont::Normal, fallback);
        case Role::HomeTitle:
            return MakeFont(20, QFont::DemiBold, fallback);
        case Role::HomeStatus:
            return MakeFont(16, QFont::Medium, fallback);
        case Role::ConnectButtonState:
            return MakeFont(22, QFont::Medium, fallback);
    }
    return MakeFont(13, QFont::Normal, fallback);
}

QString FontDebugString(const QFont &font) {
    QFontInfo info(font);
    return QStringLiteral("request{family=%1 weight=%2 pixel=%3} actual{family=%4 weight=%5 pixel=%6}")
        .arg(font.family())
        .arg(font.weight())
        .arg(font.pixelSize())
        .arg(info.family())
        .arg(info.weight())
        .arg(info.pixelSize());
}

void ApplyAppFont(QApplication &app) {
    const auto font = FontForRole(Role::AppBase, app.font());
    app.setFont(font);
}

} // namespace Typography
