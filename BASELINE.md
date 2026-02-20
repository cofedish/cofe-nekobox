# Baseline

## UI stack and entry points
- UI stack: Qt Widgets with .ui forms (no QML detected). Main window is `QMainWindow` built from `ui/mainwindow.ui` + `ui/mainwindow.cpp`.
- Entry point: `main/main.cpp` sets up QApplication, loads config, then creates the main window via `UI_InitMainWindow()` (declared in `ui/mainwindow_interface.h`).
- Theme handling: `ui/ThemeManager.hpp` + `ui/ThemeManager.cpp`, current theme stored in `NekoGui::DataStore` (`main/NekoGui_DataStore.hpp`).

## Build and run (as-is)
### Windows (from docs)
- Prereqs: MSVC toolchain (VS 2019/2022), Qt (docs mention 6.5.x; 5.15.x also referenced), CMake + Ninja.
- C++ deps: `bash ./libs/build_deps_all.sh`.
- Configure/build:
  - `cmake -S . -B build -GNinja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=D:/path/to/Qt/...`
  - `cmake --build build`
- Go core:
  - `bash libs/get_source.sh`
  - `GOOS=windows GOARCH=amd64 bash libs/build_go.sh`
- Run: `build/cofebox.exe` (use `windeployqt` for runtime Qt DLLs as needed).

### Linux (from docs)
- Prereqs: Qt5/Qt6 dev packages (qtbase/qtsvg/qttools/qtx11extras), CMake + Ninja, C++ deps (protobuf, yaml-cpp, zxing-cpp).
- Configure/build:
  - `cmake -S . -B build -GNinja`
  - `cmake --build build`
- Run: `build/cofebox`.

### Local build attempt (this workspace)
- `cmake -S . -B build -GNinja` failed because no C++ compiler was found (`CMAKE_CXX_COMPILER` missing). Build/run not completed.

## Existing navigation and screens
### Main window
- Top toolbar menus: Program, Preferences, Server (menus), plus Ads, Document, Update actions, Tun Mode and System Proxy toggles, search box, and URL Test button.
- Central area: group tabs (one tab per group) with proxy/profile table (Type/Address/Name/Test Result/Traffic).
- Bottom area: Log and Connection tabs (log viewer + live connection table).
- Files: `ui/mainwindow.ui`, `ui/mainwindow.cpp`, `ui/mainwindow.h`.

### Dialogs and editors
- Basic Settings (tabs: Common, Style, Subscription, Core, Extra Core, Security): `ui/dialog_basic_settings.ui`, `ui/dialog_basic_settings.cpp`.
- Routing Settings / Rules: `ui/dialog_manage_routes.ui`, `ui/dialog_manage_routes.cpp`.
- VPN/Tun Settings: `ui/dialog_vpn_settings.ui`, `ui/dialog_vpn_settings.cpp`.
- Hotkey Settings: `ui/dialog_hotkey.ui`, `ui/dialog_hotkey.cpp`.
- Groups management: `ui/dialog_manage_groups.ui`, `ui/dialog_manage_groups.cpp`.
- Edit Group: `ui/edit/dialog_edit_group.ui`, `ui/edit/dialog_edit_group.cpp`.
- Edit Profile: `ui/edit/dialog_edit_profile.ui`, `ui/edit/dialog_edit_profile.cpp`.
- Protocol editors (Socks/HTTP, Shadowsocks, VMess, Trojan/VLESS, Naive, QUIC, Custom, Chain): `ui/edit/edit_*.ui` + `ui/edit/edit_*.cpp`.

### Custom widgets
- Proxy row widget + group list item: `ui/widget/ProxyItem.*`, `ui/widget/GroupItem.*`.
- Table wrapper: `ui/widget/MyTableWidget.h`.

## Key user scenarios and code locations
- Connect / Disconnect: `MainWindow::startProxy`, `MainWindow::stopProxy` in `ui/mainwindow.cpp` (bound to `menu_start`, `menu_stop`).
- Select active server/profile:
  - Double-click row in proxy table: `MainWindow::on_proxyListTable_itemDoubleClicked`.
  - Group tabs + selection state: `MainWindow::show_group`, `MainWindow::refresh_groups`.
- Profiles CRUD (add/edit/clone/delete/move): `MainWindow::on_menu_add_from_input_triggered`, `on_menu_add_from_clipboard_triggered`, `on_menu_clone_triggered`, `on_menu_delete_triggered`, `on_menu_move_triggered` in `ui/mainwindow.cpp`, profile storage in `db/ProxyEntity.hpp` and `db/Database.cpp`.
- Import/export:
  - Import from clipboard / QR: `MainWindow::on_menu_add_from_clipboard_triggered`, `on_menu_scan_qr_triggered`.
  - Export config and share links: `MainWindow::on_menu_export_config_triggered`, `on_menu_copy_links_triggered`, `display_qr_link`.
- Subscriptions:
  - Update current group: `MainWindow::on_menu_update_subscription_triggered`.
  - Update all groups: `DialogManageGroups::on_update_all_clicked` + `sub/GroupUpdater.hpp`.
- Routing / rules: `DialogManageRoutes` (`ui/dialog_manage_routes.*`) and `NekoGui::Routing` in `main/NekoGui_DataStore.hpp` + `main/NekoGui.cpp`.
- Logs & connections:
  - Log output: `MainWindow::show_log_impl` + `masterLogBrowser` in `ui/mainwindow.cpp`.
  - Connection list: `MainWindow::refresh_connection_list` in `ui/mainwindow.cpp`.
- Settings (general/core/theme/etc.): `DialogBasicSettings` (`ui/dialog_basic_settings.*`), persisted in `NekoGui::DataStore` (`main/NekoGui_DataStore.hpp`).
- Theme selection: `ThemeManager::ApplyTheme` in `ui/ThemeManager.cpp`, theme value in `NekoGui::DataStore::theme`.


