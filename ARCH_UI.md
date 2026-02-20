# UI Architecture

## App shell
- Main window uses a drawer + stacked pages layout (`ui/mainwindow.ui`).
- Drawer navigation is a `QListWidget` (`drawer_nav`) with quick buttons and a theme switcher.
- Page container is `QStackedWidget` (`stacked_pages`) in `ui/mainwindow.ui`.
- Navigation wiring is in `ui/mainwindow.cpp` (drawer selection + quick buttons).

## Home
- `page_home` hosts the large Connect/Disconnect button and status labels.
- Connect button calls `MainWindow::startProxy` / `MainWindow::stopProxy`.
- Quick actions:
  - Select server: menu of profiles from current group.
  - Select profile: menu of groups, changes group + switches to Servers page.
  - Open logs: switches to Logs page.

## Servers
- `page_servers` embeds the existing group tabs + proxy list table.
- Group tabs: `QTabWidget` (`tabWidget`) with `proxyListTable` from the original UI.
- Search and URL test actions remain wired to the same logic.

## Profiles
- `page_profiles` provides profile CRUD actions mapped to existing menu slots:
  - New/Clone/Delete/Import/Export.
  - Edit selected profile uses the same dialog as double-click in Servers.

## Subscriptions
- `page_subscriptions` embeds a group list using `GroupItem` widgets.
- `refresh_subscriptions_list()` mirrors `DialogManageGroups` logic to render groups.
- Update All triggers `UI_update_all_groups()`.

## Rules / Routing
- `page_rules` shows active routing label and opens `DialogManageRoutes`.

## Logs
- `page_logs` reuses the existing Log + Connection tabs.

## Settings
- `page_settings` exposes buttons to open existing settings dialogs and actions.

## About
- `page_about` shows app info and links to docs/repo.

## Theme system
- Theme selection: `ThemeManager::ApplyTheme` supports system/light/dark plus legacy themes.
- Theme tokens are centralized in `ui/ThemeManager.cpp` (no scattered hard-coded colors).
- Drawer theme switcher (`drawer_theme`) is kept in sync with stored settings.

## Animated background
- `WaveBackground` (`ui/widget/WaveBackground.*`) is used as the central widget.
- Animated gradient waves are drawn in `WaveBackground::paintEvent`.
- Reduce motion disables the timer animation but keeps static gradient.
- Reduce motion is stored in `NekoGui::DataStore::reduce_motion` and set in `DialogBasicSettings`.

