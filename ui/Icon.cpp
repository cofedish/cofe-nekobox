#include "Icon.hpp"

#include "main/NekoGui.hpp"

#include <QApplication>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QStyle>

namespace {
    QColor SecondaryTextColor(const QPalette &palette) {
        QColor color = palette.color(QPalette::Disabled, QPalette::WindowText);
        if (!color.isValid() || color.alpha() == 0) {
            color = palette.color(QPalette::WindowText);
            color.setAlpha(166);
        }
        return color;
    }

    void DrawGlowStroke(QPainter &painter, const QPainterPath &path, const QColor &color, qreal width, qreal alphaScale) {
        QColor outer = color;
        outer.setAlpha(qBound(0, qRound(color.alpha() * alphaScale), 255));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(outer, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
    }

    QPainterPath BuildGlyphPath(Icon::SidebarGlyph glyph, const QRectF &rect) {
        const qreal left = rect.left();
        const qreal top = rect.top();
        const qreal width = rect.width();
        const qreal height = rect.height();
        const auto px = [&](qreal x) { return left + width * x; };
        const auto py = [&](qreal y) { return top + height * y; };

        QPainterPath path;
        switch (glyph) {
            case Icon::SidebarGlyph::Home: {
                path.moveTo(px(0.18), py(0.48));
                path.lineTo(px(0.50), py(0.18));
                path.lineTo(px(0.82), py(0.48));
                path.moveTo(px(0.26), py(0.42));
                path.lineTo(px(0.26), py(0.80));
                path.lineTo(px(0.74), py(0.80));
                path.lineTo(px(0.74), py(0.42));
                path.moveTo(px(0.44), py(0.80));
                path.lineTo(px(0.44), py(0.56));
                path.lineTo(px(0.56), py(0.56));
                path.lineTo(px(0.56), py(0.80));
                break;
            }
            case Icon::SidebarGlyph::Servers: {
                path.addRoundedRect(QRectF(px(0.16), py(0.20), width * 0.68, height * 0.24), width * 0.08, width * 0.08);
                path.addRoundedRect(QRectF(px(0.16), py(0.56), width * 0.68, height * 0.24), width * 0.08, width * 0.08);
                path.moveTo(px(0.30), py(0.32));
                path.lineTo(px(0.58), py(0.32));
                path.moveTo(px(0.30), py(0.68));
                path.lineTo(px(0.58), py(0.68));
                path.addEllipse(QRectF(px(0.68), py(0.27), width * 0.06, height * 0.06));
                path.addEllipse(QRectF(px(0.68), py(0.63), width * 0.06, height * 0.06));
                break;
            }
            case Icon::SidebarGlyph::Profiles: {
                path.addRoundedRect(QRectF(px(0.14), py(0.20), width * 0.72, height * 0.60), width * 0.10, width * 0.10);
                path.addEllipse(QRectF(px(0.24), py(0.30), width * 0.20, height * 0.20));
                path.moveTo(px(0.22), py(0.66));
                path.cubicTo(px(0.30), py(0.52), px(0.40), py(0.52), px(0.48), py(0.66));
                path.moveTo(px(0.58), py(0.38));
                path.lineTo(px(0.76), py(0.38));
                path.moveTo(px(0.58), py(0.52));
                path.lineTo(px(0.76), py(0.52));
                path.moveTo(px(0.58), py(0.66));
                path.lineTo(px(0.70), py(0.66));
                break;
            }
            case Icon::SidebarGlyph::Subscriptions: {
                path.moveTo(px(0.18), py(0.34));
                path.lineTo(px(0.82), py(0.34));
                path.moveTo(px(0.18), py(0.66));
                path.lineTo(px(0.82), py(0.66));
                path.moveTo(px(0.36), py(0.18));
                path.lineTo(px(0.36), py(0.82));
                path.moveTo(px(0.64), py(0.18));
                path.lineTo(px(0.64), py(0.82));
                path.moveTo(px(0.18), py(0.50));
                path.lineTo(px(0.82), py(0.50));
                break;
            }
            case Icon::SidebarGlyph::Routes: {
                path.addEllipse(QRectF(px(0.18), py(0.22), width * 0.14, height * 0.14));
                path.addEllipse(QRectF(px(0.68), py(0.26), width * 0.14, height * 0.14));
                path.addEllipse(QRectF(px(0.42), py(0.64), width * 0.14, height * 0.14));
                path.moveTo(px(0.31), py(0.30));
                path.lineTo(px(0.68), py(0.34));
                path.moveTo(px(0.28), py(0.36));
                path.lineTo(px(0.47), py(0.64));
                path.moveTo(px(0.74), py(0.40));
                path.lineTo(px(0.55), py(0.66));
                break;
            }
            case Icon::SidebarGlyph::Logs: {
                path.addRoundedRect(QRectF(px(0.20), py(0.18), width * 0.60, height * 0.64), width * 0.08, width * 0.08);
                path.moveTo(px(0.32), py(0.34));
                path.lineTo(px(0.68), py(0.34));
                path.moveTo(px(0.32), py(0.50));
                path.lineTo(px(0.68), py(0.50));
                path.moveTo(px(0.32), py(0.66));
                path.lineTo(px(0.58), py(0.66));
                break;
            }
            case Icon::SidebarGlyph::Settings: {
                path.addEllipse(QRectF(px(0.32), py(0.32), width * 0.36, height * 0.36));
                path.moveTo(px(0.50), py(0.14));
                path.lineTo(px(0.50), py(0.28));
                path.moveTo(px(0.50), py(0.72));
                path.lineTo(px(0.50), py(0.86));
                path.moveTo(px(0.14), py(0.50));
                path.lineTo(px(0.28), py(0.50));
                path.moveTo(px(0.72), py(0.50));
                path.lineTo(px(0.86), py(0.50));
                path.moveTo(px(0.25), py(0.25));
                path.lineTo(px(0.34), py(0.34));
                path.moveTo(px(0.66), py(0.66));
                path.lineTo(px(0.75), py(0.75));
                path.moveTo(px(0.25), py(0.75));
                path.lineTo(px(0.34), py(0.66));
                path.moveTo(px(0.66), py(0.34));
                path.lineTo(px(0.75), py(0.25));
                break;
            }
            case Icon::SidebarGlyph::About: {
                path.addEllipse(QRectF(px(0.18), py(0.18), width * 0.64, height * 0.64));
                path.addEllipse(QRectF(px(0.46), py(0.30), width * 0.08, height * 0.08));
                path.moveTo(px(0.50), py(0.42));
                path.lineTo(px(0.50), py(0.66));
                break;
            }
            case Icon::SidebarGlyph::Add: {
                path.addEllipse(QRectF(px(0.18), py(0.18), width * 0.64, height * 0.64));
                path.moveTo(px(0.50), py(0.30));
                path.lineTo(px(0.50), py(0.70));
                path.moveTo(px(0.30), py(0.50));
                path.lineTo(px(0.70), py(0.50));
                break;
            }
            case Icon::SidebarGlyph::Group: {
                path.addEllipse(QRectF(px(0.16), py(0.30), width * 0.18, height * 0.18));
                path.addEllipse(QRectF(px(0.42), py(0.24), width * 0.22, height * 0.22));
                path.addEllipse(QRectF(px(0.66), py(0.30), width * 0.18, height * 0.18));
                path.moveTo(px(0.24), py(0.66));
                path.cubicTo(px(0.28), py(0.54), px(0.40), py(0.54), px(0.44), py(0.66));
                path.moveTo(px(0.44), py(0.70));
                path.cubicTo(px(0.50), py(0.52), px(0.62), py(0.52), px(0.68), py(0.70));
                path.moveTo(px(0.56), py(0.66));
                path.cubicTo(px(0.60), py(0.54), px(0.72), py(0.54), px(0.76), py(0.66));
                break;
            }
            case Icon::SidebarGlyph::Connection: {
                path.moveTo(px(0.24), py(0.50));
                path.lineTo(px(0.40), py(0.50));
                path.moveTo(px(0.60), py(0.50));
                path.lineTo(px(0.76), py(0.50));
                path.addEllipse(QRectF(px(0.14), py(0.40), width * 0.12, height * 0.12));
                path.addEllipse(QRectF(px(0.74), py(0.40), width * 0.12, height * 0.12));
                path.moveTo(px(0.40), py(0.32));
                path.cubicTo(px(0.52), py(0.20), px(0.60), py(0.20), px(0.60), py(0.50));
                path.moveTo(px(0.40), py(0.68));
                path.cubicTo(px(0.52), py(0.80), px(0.60), py(0.80), px(0.60), py(0.50));
                break;
            }
            case Icon::SidebarGlyph::Folder: {
                path.moveTo(px(0.16), py(0.34));
                path.lineTo(px(0.36), py(0.34));
                path.lineTo(px(0.44), py(0.24));
                path.lineTo(px(0.82), py(0.24));
                path.lineTo(px(0.82), py(0.74));
                path.lineTo(px(0.16), py(0.74));
                path.closeSubpath();
                break;
            }
            case Icon::SidebarGlyph::Refresh: {
                path.moveTo(px(0.72), py(0.40));
                path.cubicTo(px(0.66), py(0.24), px(0.44), py(0.18), px(0.28), py(0.30));
                path.cubicTo(px(0.16), py(0.38), px(0.12), py(0.56), px(0.20), py(0.70));
                path.cubicTo(px(0.30), py(0.86), px(0.54), py(0.88), px(0.68), py(0.76));
                path.moveTo(px(0.62), py(0.24));
                path.lineTo(px(0.76), py(0.24));
                path.lineTo(px(0.76), py(0.38));
                break;
            }
            case Icon::SidebarGlyph::Apps: {
                for (int row = 0; row < 2; ++row) {
                    for (int col = 0; col < 2; ++col) {
                        path.addRoundedRect(QRectF(px(0.20 + col * 0.28), py(0.20 + row * 0.28),
                                                   width * 0.18, height * 0.18),
                                            width * 0.04,
                                            width * 0.04);
                    }
                }
                break;
            }
            case Icon::SidebarGlyph::Hotkey: {
                path.addRoundedRect(QRectF(px(0.18), py(0.28), width * 0.64, height * 0.44), width * 0.10, width * 0.10);
                path.moveTo(px(0.28), py(0.42));
                path.lineTo(px(0.72), py(0.42));
                path.moveTo(px(0.30), py(0.58));
                path.lineTo(px(0.40), py(0.58));
                path.moveTo(px(0.48), py(0.58));
                path.lineTo(px(0.58), py(0.58));
                path.moveTo(px(0.66), py(0.58));
                path.lineTo(px(0.72), py(0.58));
                break;
            }
            case Icon::SidebarGlyph::Restart: {
                path.moveTo(px(0.72), py(0.34));
                path.cubicTo(px(0.60), py(0.18), px(0.36), py(0.18), px(0.24), py(0.34));
                path.cubicTo(px(0.12), py(0.50), px(0.18), py(0.74), px(0.38), py(0.82));
                path.moveTo(px(0.52), py(0.18));
                path.lineTo(px(0.72), py(0.18));
                path.lineTo(px(0.72), py(0.38));
                break;
            }
        }
        return path;
    }

    QPixmap RenderSidebarGlyph(Icon::SidebarGlyph glyph, const QColor &stroke, const QColor &glow, int size) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

        const QRectF rect(size * 0.12, size * 0.12, size * 0.76, size * 0.76);
        const auto path = BuildGlyphPath(glyph, rect);
        const qreal strokeWidth = qMax(1.4, size * 0.095);

        if (glow.alpha() > 0) {
            DrawGlowStroke(painter, path, glow, strokeWidth * 2.0, 0.24);
            DrawGlowStroke(painter, path, glow, strokeWidth * 1.35, 0.42);
        }

        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(stroke, strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(path);
        return pixmap;
    }
} // namespace

QPixmap Icon::GetTrayIcon(Icon::TrayIconStatus status) {
    QPixmap pixmap;

    // software embedded icon
    auto pixmap_read = QPixmap(":/cofebox/" + software_name.toLower() + ".png");
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    // software pack icon
    pixmap_read = QPixmap("../" + software_name.toLower() + ".png");
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    // user icon
    pixmap_read = QPixmap("./" + software_name.toLower() + ".png");
    if (!pixmap_read.isNull()) pixmap = pixmap_read;

    if (pixmap.isNull()) {
        pixmap = QPixmap(":/cofebox/cofebox.png");
    }
    if (pixmap.isNull()) {
        const auto fallback = QIcon::fromTheme("cofebox");
        if (!fallback.isNull()) pixmap = fallback.pixmap(64, 64);
    }
    if (pixmap.isNull()) {
        const auto fallback = QIcon::fromTheme("applications-internet");
        if (!fallback.isNull()) pixmap = fallback.pixmap(64, 64);
    }
    if (pixmap.isNull()) {
        pixmap = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon).pixmap(64, 64);
    }

    if (status == TrayIconStatus::NONE) return pixmap;

    auto p = QPainter(&pixmap);

    auto side = pixmap.width();
    auto radius = side * 0.4;
    auto d = side * 0.3;
    auto margin = side * 0.05;

    if (status == TrayIconStatus::RUNNING) {
        p.setBrush(QBrush(Qt::darkGreen));
    } else if (status == TrayIconStatus::SYSTEM_PROXY) {
        p.setBrush(QBrush(Qt::blue));
    } else if (status == TrayIconStatus::VPN) {
        p.setBrush(QBrush(Qt::red));
    }
    p.drawRoundedRect(
        QRect(side - d - margin,
              side - d - margin,
              d,
              d),
        radius,
        radius);
    p.end();

    return pixmap;
}

QPixmap Icon::GetMaterialIcon(const QString &name) {
    QPixmap pixmap(":/icon/material/" + name + ".svg");
    return pixmap;
}

QPixmap Icon::GetSidebarIconPixmap(Icon::SidebarGlyph glyph, const QPalette &palette, Icon::SidebarIconState state, int size) {
    QColor stroke = SecondaryTextColor(palette);
    QColor glow(0, 0, 0, 0);

    if (state == Icon::SidebarIconState::Hovered) {
        stroke = palette.color(QPalette::WindowText);
        glow = stroke;
        glow.setAlpha(78);
    } else if (state == Icon::SidebarIconState::Active) {
        stroke = palette.color(QPalette::Highlight);
        glow = stroke;
        glow.setAlpha(132);
    }

    return RenderSidebarGlyph(glyph, stroke, glow, size);
}

QIcon Icon::GetSidebarIcon(Icon::SidebarGlyph glyph, const QPalette &palette, int size) {
    QIcon icon;
    icon.addPixmap(GetSidebarIconPixmap(glyph, palette, Icon::SidebarIconState::Normal, size), QIcon::Normal, QIcon::Off);
    icon.addPixmap(GetSidebarIconPixmap(glyph, palette, Icon::SidebarIconState::Hovered, size), QIcon::Active, QIcon::Off);
    icon.addPixmap(GetSidebarIconPixmap(glyph, palette, Icon::SidebarIconState::Active, size), QIcon::Selected, QIcon::Off);
    icon.addPixmap(GetSidebarIconPixmap(glyph, palette, Icon::SidebarIconState::Active, size), QIcon::Selected, QIcon::On);
    icon.addPixmap(GetSidebarIconPixmap(glyph, palette, Icon::SidebarIconState::Active, size), QIcon::Normal, QIcon::On);
    return icon;
}
