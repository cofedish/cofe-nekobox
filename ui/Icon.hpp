#pragma once

#include <QIcon>
#include <QPalette>
#include <QPixmap>

namespace Icon {

    enum TrayIconStatus {
        NONE,
        RUNNING,
        SYSTEM_PROXY,
        VPN,
    };

    enum class SidebarGlyph {
        Home,
        Servers,
        Profiles,
        Subscriptions,
        Routes,
        Logs,
        Settings,
        About,
        Add,
        Group,
        Connection,
        Folder,
        Refresh,
        Apps,
        Hotkey,
        Restart
    };

    enum class SidebarIconState {
        Normal,
        Hovered,
        Active
    };

    QPixmap GetTrayIcon(TrayIconStatus status);

    QPixmap GetMaterialIcon(const QString &name);

    QIcon GetSidebarIcon(SidebarGlyph glyph, const QPalette &palette, int size = 20);
    QPixmap GetSidebarIconPixmap(SidebarGlyph glyph, const QPalette &palette, SidebarIconState state, int size = 20);

} // namespace Icon
