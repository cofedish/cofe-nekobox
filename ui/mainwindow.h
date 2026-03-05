#pragma once

#include <QMainWindow>

#include "main/NekoGui.hpp"

#ifndef MW_INTERFACE

#include <QTime>
#include <QTableWidgetItem>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSystemTrayIcon>
#include <QProcess>
#include <QTextDocument>
#include <QShortcut>
#include <QSemaphore>
#include <QMutex>

#include "GroupSort.hpp"

#include "db/ProxyEntity.hpp"
#include "main/GuiUtils.hpp"

#endif

namespace NekoGui_sys {
    class CoreProcess;
}

class QParallelAnimationGroup;
class QPropertyAnimation;
class QResizeEvent;
class QTimer;
class QMenu;
class QButtonGroup;
class ToastWidget;
class UpdateService;
class QLabel;

QT_BEGIN_NAMESPACE
namespace Ui {
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

    void refresh_proxy_list(const int &id = -1);

    void show_group(int gid);

    void refresh_groups();

    void refresh_status(const QString &traffic_update = "");

    void startProxy(int _id = -1);

    void stopProxy(bool crash = false, bool sem = false);

    void setSystemProxyMode(bool enable, bool save = true);

    void setVpnMode(bool enable, bool save = true);

    void show_log_impl(const QString &log);

    void start_select_mode(QObject *context, const std::function<void(int)> &callback);

    void refresh_connection_list(const QJsonArray &arr);

    void RegisterHotkey(bool unregister);

    bool StopVPNProcess(bool unconditional = false);

signals:

    void profile_selected(int id);

public slots:

    void on_commitDataRequest();

    void on_menu_exit_triggered();

#ifndef MW_INTERFACE

private slots:

    void on_masterLogBrowser_customContextMenuRequested(const QPoint &pos);

    void on_menu_basic_settings_triggered();

    void on_menu_routing_settings_triggered();

    void on_menu_vpn_settings_triggered();

    void on_menu_hotkey_settings_triggered();

    void on_menu_add_from_input_triggered();

    void on_menu_add_from_clipboard_triggered();

    void on_menu_clone_triggered();

    void on_menu_move_triggered();

    void on_menu_delete_triggered();

    void on_menu_reset_traffic_triggered();

    void on_menu_profile_debug_info_triggered();

    void on_menu_copy_links_triggered();

    void on_menu_copy_links_nkr_triggered();

    void on_menu_export_config_triggered();

    void display_qr_link(bool nkrFormat = false);

    void on_menu_scan_qr_triggered();

    void on_menu_clear_test_result_triggered();

    void on_menu_manage_groups_triggered();

    void on_menu_select_all_triggered();

    void on_menu_delete_repeat_triggered();

    void on_menu_remove_unavailable_triggered();

    void on_menu_update_subscription_triggered();

    void on_menu_resolve_domain_triggered();

    void on_proxyListTable_itemDoubleClicked(QTableWidgetItem *item);

    void on_proxyListTable_customContextMenuRequested(const QPoint &pos);

    void on_tabWidget_currentChanged(int index);

private:
    enum class ConnectState {
        Disconnected,
        Connecting,
        Connected,
        Disconnecting
    };
    Ui::MainWindow *ui;
    QSystemTrayIcon *tray;
    QShortcut *shortcut_ctrl_f = new QShortcut(QKeySequence("Ctrl+F"), this);
    QShortcut *shortcut_esc = new QShortcut(QKeySequence("Esc"), this);
    //
    NekoGui_sys::CoreProcess *core_process;
    qint64 vpn_pid = 0;
    //
    bool qvLogAutoScoll = true;
    QTextDocument *qvLogDocument = new QTextDocument(this);
    //
    QString title_error;
    int icon_status = -1;
    std::shared_ptr<NekoGui::ProxyEntity> running;
    QString traffic_update_cache;
    QTime last_test_time;
    //
    int proxy_last_order = -1;
    bool select_mode = false;
    QMutex mu_starting;
    QMutex mu_stopping;
    QMutex mu_exit;
    QSemaphore sem_stopped;
    int exit_reason = 0;
    ConnectState connect_state = ConnectState::Disconnected;
    bool drawer_open = false;
    QWidget *drawer_scrim = nullptr;
    QParallelAnimationGroup *drawer_anim = nullptr;
    QPropertyAnimation *drawer_anim_max = nullptr;
    QPropertyAnimation *drawer_anim_min = nullptr;
    QMenu *drawer_theme_menu = nullptr;
    QButtonGroup *drawer_theme_group = nullptr;
    ToastWidget *toast = nullptr;
    UpdateService *updateService_ = nullptr;
    QTimer *add_debounce_timer = nullptr;
    bool add_in_progress = false;
    double update_progress_cached_ = 0.0;
    int add_base_count = 0;
    QString home_running_full_text;
    QString home_running_tooltip;

    QList<std::shared_ptr<NekoGui::ProxyEntity>> get_now_selected_list();

    QList<std::shared_ptr<NekoGui::ProxyEntity>> get_selected_or_group();

    void dialog_message_impl(const QString &sender, const QString &info);

    void refresh_proxy_list_impl(const int &id = -1, GroupSortAction groupSortAction = {});

    void refresh_proxy_list_impl_refresh_data(const int &id = -1);

    void refresh_subscriptions_list();

    void set_connect_state(ConnectState state);
    void update_connect_button();
    void set_drawer_open(bool open, bool animated = true);
    void update_drawer_scrim();
    void submit_home_subscription();
    void submit_servers_subscription();
    void show_toast(const QString &text, int durationMs = 2500);
    void show_toast_success(const QString &text);
    void show_toast_error(const QString &text);
    bool can_start_add() const;
    void set_add_controls_enabled(bool enabled);
    void finish_add_operation();
    void sync_drawer_theme(const QString &themeKey);
    void set_home_running_text(const QString &text, const QString &tooltip = {});
    void update_home_running_elide();

    void keyPressEvent(QKeyEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    //

    void HotkeyEvent(const QString &key);

    bool StartVPNProcess();

    // grpc and ...

    static void setup_grpc();

    void speedtest_current_group(int mode, bool test_group);

    void speedtest_current();

    static void stop_core_daemon();

    void CheckUpdate();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

#endif // MW_INTERFACE
};

inline MainWindow *GetMainWindow() {
    return (MainWindow *) mainwindow;
}

void UI_InitMainWindow();

