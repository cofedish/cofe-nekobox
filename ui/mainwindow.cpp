#include "./ui_mainwindow.h"
#include "mainwindow.h"

#include "fmt/Preset.hpp"
#include "db/ProfileFilter.hpp"
#include "db/ConfigBuilder.hpp"
#include "sub/GroupUpdater.hpp"
#include "sys/ExternalProcess.hpp"
#include "sys/AutoRun.hpp"

#include "ui/ThemeManager.hpp"
#include "ui/UpdateService.hpp"
#include "ui/HotspotGatewayService.hpp"
#include "ui/Icon.hpp"
#include "ui/widget/WaveBackground.h"
#include "ui/widget/ConnectButton.h"
#include "ui/edit/dialog_edit_profile.h"
#include "ui/dialog_basic_settings.h"
#include "main/AppInfo.hpp"
#include "ui/dialog_manage_groups.h"
#include "ui/dialog_manage_routes.h"
#include "ui/dialog_app_routing.h"
#include "ui/dialog_vpn_settings.h"
#include "ui/dialog_hotkey.h"
#include "ui/edit/dialog_edit_group.h"
#include "ui/widget/GroupItem.h"
#include "main/ProcessRoutingRules.hpp"

#include "3rdparty/fix_old_qt.h"
#include "3rdparty/qrcodegen.hpp"
#include "3rdparty/VT100Parser.hpp"
#include "3rdparty/qv2ray/v2/components/proxy/QvProxyConfigurator.hpp"

#ifndef NKR_NO_ZXING
#include "3rdparty/ZxingQtReader.hpp"
#endif

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#else
#ifdef Q_OS_LINUX
#include "sys/linux/LinuxCap.h"
#endif
#include <unistd.h>
#endif

#include <QClipboard>
#include <QAbstractButton>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QTextBlock>
#include <QScrollBar>
#include <QScreen>
#include <QDesktopServices>
#include <QInputDialog>
#include <QThread>
#include <QTimer>
#include <QSignalBlocker>
#include <QButtonGroup>
#include <QToolButton>
#include <QMenu>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QSizePolicy>
#include <QMessageBox>
#include <QPainter>
#include <QWidgetAction>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QCheckBox>
#include <QPushButton>
#include <QStyle>
#include <QSizePolicy>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QCryptographicHash>
#include <QProcessEnvironment>

#include "ui/widget/ToastWidget.h"

void UI_InitMainWindow() {
    mainwindow = new MainWindow;
}

inline int tabIndex2GroupId(int index);
inline int groupId2TabIndex(int gid);

namespace {
    QString NormalizeThemeKey(const QString &value) {
        const auto key = value.trimmed().toLower();
        if (key == "0" || key == "system") return "system";
        if (key == "1" || key == "light") return "light";
        if (key == "2" || key == "dark") return "dark";
        if (key == "bad600light" || key == "bad600-light") return "bad600-light";
        if (key == "bad600dark" || key == "bad600-dark") return "bad600-dark";
        if (key == "lucifer") return "lucifer";
        return "system";
    }

    QString ThemeKeyFromIndex(int index) {
        if (index == 1) return "light";
        if (index == 2) return "dark";
        if (index == 3) return "lucifer";
        if (index == 4) return "bad600-light";
        if (index == 5) return "bad600-dark";
        return "system";
    }

    int ThemeIndexFromKey(const QString &value) {
        const auto key = NormalizeThemeKey(value);
        if (key == "light") return 1;
        if (key == "dark") return 2;
        if (key == "lucifer") return 3;
        if (key == "bad600-light") return 4;
        if (key == "bad600-dark") return 5;
        return 0;
    }

    QIcon BuildThemeSwatchIcon(const ThemeOption &option, int size = 20) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF rect(1, 1, size - 2, size - 2);
        const QList<QColor> colors = {option.window, option.surface, option.accent, option.text};
        int startAngle = 0;
        for (const auto &color : colors) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            painter.drawPie(rect, startAngle * 16, 90 * 16);
            startAngle += 90;
        }

        QColor border = option.text;
        border.setAlpha(90);
        painter.setPen(QPen(border, 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(rect);
        return QIcon(pixmap);
    }

    QString Sha256Hex(const QByteArray &data) {
        return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()).toLower();
    }

    QString Sha256FileHex(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QCryptographicHash hash(QCryptographicHash::Sha256);
        while (!file.atEnd()) {
            hash.addData(file.read(1 << 16));
        }
        return QString::fromLatin1(hash.result().toHex()).toLower();
    }
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
    mainwindow = this;
    MW_dialog_message = [=](const QString &a, const QString &b) {
        runOnUiThread([=] { dialog_message_impl(a, b); });
    };

    // Load Manager
    NekoGui::profileManager->LoadManager();

    // Setup misc UI
    auto initialTheme = NormalizeThemeKey(NekoGui::dataStore->theme);
    if (initialTheme != NekoGui::dataStore->theme) {
        NekoGui::dataStore->theme = initialTheme;
        NekoGui::dataStore->Save();
    }
    themeManager->ApplyTheme(initialTheme);
    ui->setupUi(this);
    toast = new ToastWidget(ui->centralwidget);
    toast->setAnchorRect(ui->centralwidget->rect());
    add_debounce_timer = new QTimer(this);
    add_debounce_timer->setSingleShot(true);
    ui->drawer_app_name->setText(software_name);
    ui->about_title->setText(software_name);
    if (ui->about_logo != nullptr) {
        const int logoSize = 200;
        QPixmap logo(":/cofebox/cofebox.png");
        ui->about_logo->setPixmap(logo.scaled(logoSize, logoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    if (ui->aboutLayout != nullptr) {
        if (ui->about_logo != nullptr) {
            ui->aboutLayout->setAlignment(ui->about_logo, Qt::AlignHCenter);
        }
        if (ui->about_title != nullptr) {
            ui->aboutLayout->setAlignment(ui->about_title, Qt::AlignHCenter);
        }
        if (ui->about_text != nullptr) {
            ui->aboutLayout->setAlignment(ui->about_text, Qt::AlignHCenter);
        }
    }
    ui->about_text->setText(tr("Qt-based proxy manager for sing-box.\nVersion: %1").arg(AppInfo::Version()));
    ui->drawer_toggle->setText(QString(QChar(0x2630)));
    if (ui->home_center != nullptr) {
        ui->home_center->setMaximumWidth(QWIDGETSIZE_MAX);
        ui->home_center->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    }
    if (ui->label_running != nullptr) {
        ui->label_running->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        ui->label_running->setMinimumWidth(0);
        set_home_running_text(ui->label_running->text());
    }
    if (ui->homeCenterLayout != nullptr) {
        ui->homeCenterLayout->setAlignment(ui->home_connect_button, Qt::AlignHCenter);
    }
    if (auto wave = qobject_cast<WaveBackground *>(ui->centralwidget)) {
        wave->setReduceMotion(NekoGui::dataStore->reduce_motion);
    }
    if (auto connectButton = qobject_cast<ConnectButton *>(ui->home_connect_button)) {
        connectButton->setReduceMotion(NekoGui::dataStore->reduce_motion);
        connectButton->refreshMetrics();
    }
    //
    connect(ui->menu_start, &QAction::triggered, this, [=]() { startProxy(); });
    connect(ui->menu_stop, &QAction::triggered, this, [=]() { stopProxy(); });
    connect(ui->tabWidget->tabBar(), &QTabBar::tabMoved, this, [=](int from, int to) {
        // use tabData to track tab & gid
        NekoGui::profileManager->groupsTabOrder.clear();
        for (int i = 0; i < ui->tabWidget->tabBar()->count(); i++) {
            NekoGui::profileManager->groupsTabOrder += ui->tabWidget->tabBar()->tabData(i).toInt();
        }
        NekoGui::profileManager->SaveManager();
    });
    ui->label_running->installEventFilter(this);
    ui->label_inbound->installEventFilter(this);
    ui->proxyListTable->installEventFilter(this);
    //
    RegisterHotkey(false);
    //
    auto last_size = NekoGui::dataStore->mw_size.split("x");
    if (last_size.length() == 2) {
        auto w = last_size[0].toInt();
        auto h = last_size[1].toInt();
        if (w > 0 && h > 0) {
            resize(w, h);
        }
    }

    if (QDir("dashboard").count() == 0) {
        QDir().mkdir("dashboard");
        QFile::copy(":/cofebox/dashboard-notice.html", "dashboard/index.html");
    }

    // drawer + navigation
    ui->menubar->setVisible(false);
    ui->drawer_nav->setCurrentRow(0);
    drawer_theme_menu = new QMenu(this);
    drawer_theme_menu->setObjectName("drawer_theme_menu");
    auto themeMenuWidget = new QWidget(drawer_theme_menu);
    auto themeMenuLayout = new QHBoxLayout(themeMenuWidget);
    themeMenuLayout->setContentsMargins(8, 8, 8, 8);
    themeMenuLayout->setSpacing(8);
    drawer_theme_group = new QButtonGroup(this);
    drawer_theme_group->setExclusive(true);
    for (const auto &option : themeManager->AvailableThemes()) {
        auto button = new QToolButton(themeMenuWidget);
        button->setObjectName("drawer_theme_swatch");
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIcon(BuildThemeSwatchIcon(option, 20));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(option.displayName);
        button->setCursor(Qt::PointingHandCursor);
        button->setFocusPolicy(Qt::NoFocus);
        button->setProperty("themeKey", option.id);
        themeMenuLayout->addWidget(button);
        drawer_theme_group->addButton(button);
    }
    auto themeMenuAction = new QWidgetAction(drawer_theme_menu);
    themeMenuAction->setDefaultWidget(themeMenuWidget);
    drawer_theme_menu->addAction(themeMenuAction);
    ui->drawer_theme_button->setMenu(drawer_theme_menu);
    ui->drawer_theme_button->setPopupMode(QToolButton::InstantPopup);
    ui->drawer_theme_button->setText(tr("Theme"));
    ui->drawer_theme_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    ui->drawer_theme_button->setLayoutDirection(Qt::RightToLeft);
    ui->drawer_theme_button->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    ui->drawer_theme_button->setIconSize(QSize(10, 10));
    ui->drawer_theme_button->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    connect(ui->drawer_theme_button, &QToolButton::clicked, ui->drawer_theme_button, &QToolButton::showMenu);
    sync_drawer_theme(NekoGui::dataStore->theme);
    connect(themeManager, &ThemeManager::themeChanged, this, [=](const QString &themeKey) {
        sync_drawer_theme(themeKey);
    });
    connect(drawer_theme_group,
            static_cast<void (QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
            this,
            [=](QAbstractButton *button) {
                if (button == nullptr) return;
                const auto themeKey = button->property("themeKey").toString();
                if (themeKey.isEmpty()) return;
                themeManager->ApplyTheme(themeKey);
                NekoGui::dataStore->theme = themeKey;
                NekoGui::dataStore->Save();
                if (drawer_theme_menu != nullptr) {
                    drawer_theme_menu->close();
                }
            });
    connect(ui->drawer_nav, &QListWidget::currentRowChanged, this, [=](int row) {
        ui->stacked_pages->setCurrentIndex(row);
        if (auto item = ui->drawer_nav->item(row)) {
            ui->topbar_title->setText(item->text());
        }
        if (drawer_open) {
            set_drawer_open(false);
        }
    });
    connect(ui->toolButton_url_test, &QToolButton::clicked, this, [=] { speedtest_current_group(1, true); });
    connect(ui->home_open_logs, &QToolButton::clicked, this, [=] { ui->drawer_nav->setCurrentRow(5); });
    connect(ui->home_sub_add, &QPushButton::clicked, this, [=] { submit_home_subscription(); });
    connect(ui->home_sub_url, &QLineEdit::returnPressed, this, [=] { submit_home_subscription(); });
    ui->home_sub_url->installEventFilter(this);
    connect(ui->servers_add_button, &QToolButton::clicked, this, [=] { submit_servers_subscription(); });
    connect(ui->servers_add_url, &QLineEdit::returnPressed, this, [=] { submit_servers_subscription(); });
    connect(ui->servers_add_paste, &QToolButton::clicked, this, [=] {
        auto clipboardText = QApplication::clipboard()->text().trimmed();
        if (!clipboardText.isEmpty()) {
            ui->servers_add_url->setText(clipboardText);
            ui->servers_add_url->setFocus();
        }
    });
    connect(ui->home_connect_button, &QPushButton::clicked, this, [=] {
        if (connect_state == ConnectState::Connecting || connect_state == ConnectState::Disconnecting) {
            return;
        }
        if (running == nullptr) {
            startProxy();
        } else {
            stopProxy();
        }
    });
    connect(ui->home_select_server, &QToolButton::clicked, this, [=] {
        auto group = NekoGui::profileManager->CurrentGroup();
        if (group == nullptr) {
            return;
        }
        QMenu menu(this);
        int active_count = 0;
        for (const auto &pf: group->ProfilesWithOrder()) {
            auto action = menu.addAction(pf->bean->DisplayTypeAndName());
            action->setCheckable(true);
            action->setChecked(NekoGui::dataStore->started_id == pf->id);
            connect(action, &QAction::triggered, this, [=] {
                if (NekoGui::dataStore->started_id == pf->id) {
                    stopProxy();
                } else {
                    startProxy(pf->id);
                }
            });
            if (++active_count == 100) break;
        }
        menu.exec(ui->home_select_server->mapToGlobal(QPoint(0, ui->home_select_server->height())));
    });
    connect(ui->home_select_profile, &QToolButton::clicked, this, [=] {
        QMenu menu(this);
        for (const auto &gid: NekoGui::profileManager->groupsTabOrder) {
            auto group = NekoGui::profileManager->GetGroup(gid);
            if (group == nullptr) continue;
            auto action = menu.addAction(group->name);
            action->setCheckable(true);
            action->setChecked(NekoGui::dataStore->current_group == gid);
            connect(action, &QAction::triggered, this, [=] {
                ui->drawer_nav->setCurrentRow(1);
                ui->tabWidget->setCurrentIndex(groupId2TabIndex(gid));
                show_group(gid);
            });
        }
        menu.exec(ui->home_select_profile->mapToGlobal(QPoint(0, ui->home_select_profile->height())));
    });
    connect(ui->profiles_new, &QPushButton::clicked, this, &MainWindow::on_menu_add_from_input_triggered);
    connect(ui->profiles_clone, &QPushButton::clicked, this, &MainWindow::on_menu_clone_triggered);
    connect(ui->profiles_delete, &QPushButton::clicked, this, &MainWindow::on_menu_delete_triggered);
    connect(ui->profiles_import_clipboard, &QPushButton::clicked, this, &MainWindow::on_menu_add_from_clipboard_triggered);
    connect(ui->profiles_export, &QPushButton::clicked, this, &MainWindow::on_menu_export_config_triggered);
    connect(ui->profiles_open_servers, &QPushButton::clicked, this, [=] { ui->drawer_nav->setCurrentRow(1); });
    connect(ui->profiles_edit, &QPushButton::clicked, this, [=] {
        auto items = ui->proxyListTable->selectedItems();
        if (items.isEmpty()) return;
        auto id = items.first()->data(114514).toInt();
        auto dialog = new DialogEditProfile("", id, this);
        connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
    });
    connect(ui->subscriptions_new, &QPushButton::clicked, this, [=] {
        auto ent = NekoGui::ProfileManager::NewGroup();
        auto dialog = new DialogEditGroup(ent, this);
        if (dialog->exec() == QDialog::Accepted) {
            NekoGui::profileManager->AddGroup(ent);
            refresh_groups();
            refresh_subscriptions_list();
        }
        dialog->deleteLater();
    });
    connect(ui->subscriptions_update_all, &QPushButton::clicked, this, [=] {
        if (QMessageBox::question(this, tr("Confirmation"), tr("Update all subscriptions?")) == QMessageBox::StandardButton::Yes) {
            UI_update_all_groups();
        }
    });
    connect(ui->rules_open, &QPushButton::clicked, this, &MainWindow::on_menu_routing_settings_triggered);
    connect(ui->rules_app_routing, &QPushButton::clicked, this, [=] {
        DialogAppRouting dialog(this);
        if (dialog.exec() != QDialog::Accepted) return;
        NekoGui::dataStore->Save();
        MW_dialog_message("", "UpdateDataStore,VPNChanged");
        if (NekoGui::dataStore->spmode_vpn) {
            MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
        }
        show_toast_success(tr("Application routing updated."));
        refresh_status();
    });
    setupHotspotUi();

    connect(ui->settings_basic, &QPushButton::clicked, this, &MainWindow::on_menu_basic_settings_triggered);
    connect(ui->settings_vpn, &QPushButton::clicked, this, &MainWindow::on_menu_vpn_settings_triggered);
    connect(ui->settings_hotkey, &QPushButton::clicked, this, &MainWindow::on_menu_hotkey_settings_triggered);
    connect(ui->settings_open_config, &QPushButton::clicked, ui->menu_open_config_folder, &QAction::trigger);
    connect(ui->settings_restart_proxy, &QPushButton::clicked, ui->actionRestart_Proxy, &QAction::trigger);
    connect(ui->settings_restart_app, &QPushButton::clicked, ui->actionRestart_Program, &QAction::trigger);
    connect(ui->about_docs, &QPushButton::clicked, this, [=] { QDesktopServices::openUrl(QUrl(AppInfo::DocsUrl())); });
    connect(ui->about_repo, &QPushButton::clicked, this, [=] { QDesktopServices::openUrl(QUrl(AppInfo::RepoUrl())); });

    updateService_ = new UpdateService(this);
    if (ui->about_update_progress != nullptr) {
        ui->about_update_progress->setVisible(false);
        ui->about_update_progress->setRange(0, 100);
        ui->about_update_progress->setValue(0);
    }
    if (ui->about_update_status != nullptr) {
        ui->about_update_status->setText(tr("Updates are ready to check."));
    }

    const auto openReleasePage = [=]() {
        const auto info = updateService_->info();
        const auto url = info.releaseUrl.isValid() ? info.releaseUrl : QUrl(AppInfo::RepoUrl() + "/releases");
        QDesktopServices::openUrl(url);
    };

    const auto syncUpdateUi = [=]() {
        if (updateService_ == nullptr) return;
        const auto state = updateService_->state();
        const auto info = updateService_->info();
        const bool busy = state == UpdateService::State::Checking ||
                          state == UpdateService::State::Downloading ||
                          state == UpdateService::State::Verifying ||
                          state == UpdateService::State::Installing;
        const bool available = state == UpdateService::State::UpdateAvailable;
        const bool canAutoInstall = available && info.autoInstallSupported;
        const bool canManualOpen = available && !info.autoInstallSupported;

        if (ui->about_check_updates != nullptr) {
            ui->about_check_updates->setEnabled(!busy);
        }
        if (ui->about_update_action != nullptr) {
            if (canAutoInstall) {
                ui->about_update_action->setText(tr("Update to v%1").arg(info.latestVersion));
                ui->about_update_action->setEnabled(true);
            } else if (canManualOpen) {
                ui->about_update_action->setText(tr("Open release page"));
                ui->about_update_action->setEnabled(true);
            } else {
                ui->about_update_action->setText(tr("Update now"));
                ui->about_update_action->setEnabled(false);
            }
        }
        if (ui->about_update_progress != nullptr) {
            const bool showProgress = state == UpdateService::State::Downloading ||
                                      state == UpdateService::State::Verifying ||
                                      state == UpdateService::State::Installing;
            ui->about_update_progress->setVisible(showProgress);
            ui->about_update_progress->setValue(qRound(update_progress_cached_ * 100.0));
        }
    };

    connect(ui->about_check_updates, &QPushButton::clicked, this, [=] {
        if (updateService_ != nullptr) {
            updateService_->checkForUpdates(true);
        }
    });
    connect(ui->about_update_action, &QPushButton::clicked, this, [=] {
        if (updateService_ == nullptr) return;
        const auto info = updateService_->info();
        if (updateService_->state() == UpdateService::State::UpdateAvailable) {
            if (info.autoInstallSupported) {
                updateService_->startUpdate();
            } else {
                openReleasePage();
            }
        }
    });
    connect(ui->about_release_page, &QPushButton::clicked, this, [=] { openReleasePage(); });
    connect(ui->about_update_logs, &QPushButton::clicked, this, [=] {
        if (updateService_ == nullptr) return;
        const auto path = updateService_->logFilePath();
        if (!QFileInfo::exists(path)) {
            show_toast_error(tr("Update logs are not created yet."));
            return;
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });
    connect(ui->about_update_details, &QPushButton::clicked, this, [=] {
        if (updateService_ == nullptr) return;
        QString detail = updateService_->details().trimmed();
        if (detail.isEmpty()) {
            detail = updateService_->message().trimmed();
        }
        if (detail.isEmpty()) {
            detail = tr("No additional details.");
        }
        QMessageBox::information(this, tr("Update details"), detail);
    });

    connect(updateService_, &UpdateService::stateChanged, this, [=](UpdateService::State state) {
        syncUpdateUi();
        if (state == UpdateService::State::UpdateAvailable && !updateService_->lastCheckWasManual()) {
            const auto info = updateService_->info();
            show_toast_success(tr("Update available: v%1").arg(info.latestVersion));
        }
    });
    connect(updateService_, &UpdateService::messageChanged, this, [=](const QString &message) {
        if (ui->about_update_status != nullptr) {
            ui->about_update_status->setText(message);
        }
    });
    connect(updateService_, &UpdateService::progressChanged, this, [=](double progress01) {
        update_progress_cached_ = progress01;
        if (ui->about_update_progress != nullptr) {
            ui->about_update_progress->setValue(qRound(update_progress_cached_ * 100.0));
        }
    });
    connect(updateService_, &UpdateService::updateInfoChanged, this, [=] { syncUpdateUi(); });
    connect(updateService_, &UpdateService::requestApplicationExitForInstall, this, [=] {
        this->exit_reason = 1;
        on_menu_exit_triggered();
    });
    connect(updateService_, &UpdateService::restartSuggested, this, [=] {
        if (QMessageBox::question(this, tr("Update"), tr("Update installed successfully. Restart now?")) == QMessageBox::Yes) {
            this->exit_reason = 1;
            on_menu_exit_triggered();
        }
    });

    syncUpdateUi();
    QTimer::singleShot(1500, this, [=] {
        if (updateService_ != nullptr) {
            updateService_->checkForUpdates(false);
        }
    });

    drawer_scrim = new QWidget(ui->centralwidget);
    drawer_scrim->setObjectName("drawer_scrim");
    drawer_scrim->setVisible(false);
    drawer_scrim->setAttribute(Qt::WA_NoSystemBackground, false);
    drawer_scrim->installEventFilter(this);
    update_drawer_scrim();

    ui->drawer_toggle->setIcon(QIcon());

    drawer_anim = new QParallelAnimationGroup(this);
    drawer_anim_max = new QPropertyAnimation(ui->drawer_container, "maximumWidth");
    drawer_anim_min = new QPropertyAnimation(ui->drawer_container, "minimumWidth");
    drawer_anim->addAnimation(drawer_anim_max);
    drawer_anim->addAnimation(drawer_anim_min);
    drawer_anim_max->setDuration(220);
    drawer_anim_min->setDuration(220);
    drawer_anim_max->setEasingCurve(QEasingCurve::InOutCubic);
    drawer_anim_min->setEasingCurve(QEasingCurve::InOutCubic);
    connect(drawer_anim, &QParallelAnimationGroup::finished, this, [=] {
        if (!drawer_open) {
            ui->drawer_container->setVisible(false);
            if (drawer_scrim) drawer_scrim->setVisible(false);
        }
    });
    connect(ui->drawer_toggle, &QToolButton::clicked, this, [=] {
        set_drawer_open(!drawer_open);
    });
    set_drawer_open(false, false);

    // Setup log UI
    qvLogDocument->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setUndoRedoEnabled(false);
    ui->masterLogBrowser->setDocument(qvLogDocument);
    ui->masterLogBrowser->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    {
        auto font = ui->masterLogBrowser->font();
        font.setPointSize(9);
        ui->masterLogBrowser->setFont(font);
        qvLogDocument->setDefaultFont(font);
    }
    connect(ui->masterLogBrowser->verticalScrollBar(), &QSlider::valueChanged, this, [=](int value) {
        if (ui->masterLogBrowser->verticalScrollBar()->maximum() == value)
            qvLogAutoScoll = true;
        else
            qvLogAutoScoll = false;
    });
    connect(ui->masterLogBrowser, &QTextBrowser::textChanged, this, [=]() {
        if (!qvLogAutoScoll)
            return;
        auto bar = ui->masterLogBrowser->verticalScrollBar();
        bar->setValue(bar->maximum());
    });
    MW_show_log = [=](const QString &log) {
        runOnUiThread([=] { show_log_impl(log); });
    };
    MW_show_log_ext = [=](const QString &tag, const QString &log) {
        runOnUiThread([=] { show_log_impl("[" + tag + "] " + log); });
    };
    MW_show_log_ext_vt100 = [=](const QString &log) {
        runOnUiThread([=] { show_log_impl(cleanVT100String(log)); });
    };

    // table UI
    ui->proxyListTable->callback_save_order = [=] {
        auto group = NekoGui::profileManager->CurrentGroup();
        group->order = ui->proxyListTable->order;
        group->Save();
    };
    ui->proxyListTable->refresh_data = [=](int id) { refresh_proxy_list_impl_refresh_data(id); };
    if (auto button = ui->proxyListTable->findChild<QAbstractButton *>(QString(), Qt::FindDirectChildrenOnly)) {
        // Corner Button
        connect(button, &QAbstractButton::clicked, this, [=] { refresh_proxy_list_impl(-1, {GroupSortMethod::ById}); });
    }
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionClicked, this, [=](int logicalIndex) {
        GroupSortAction action;
        // toggle descending sort order
        if (proxy_last_order == logicalIndex) {
            action.descending = true;
            proxy_last_order = -1;
        } else {
            proxy_last_order = logicalIndex;
        }
        action.save_sort = true;
        // sort method
        if (logicalIndex == 0) {
            action.method = GroupSortMethod::ByType;
        } else if (logicalIndex == 1) {
            action.method = GroupSortMethod::ByAddress;
        } else if (logicalIndex == 2) {
            action.method = GroupSortMethod::ByName;
        } else if (logicalIndex == 3) {
            action.method = GroupSortMethod::ByLatency;
        } else {
            return;
        }
        refresh_proxy_list_impl(-1, action);
    });
    connect(ui->proxyListTable->horizontalHeader(), &QHeaderView::sectionResized, this, [=](int logicalIndex, int oldSize, int newSize) {
        auto group = NekoGui::profileManager->CurrentGroup();
        if (NekoGui::dataStore->refreshing_group || group == nullptr || !group->manually_column_width) return;
        // save manually column width
        group->column_width.clear();
        for (int i = 0; i < ui->proxyListTable->horizontalHeader()->count(); i++) {
            group->column_width.push_back(ui->proxyListTable->horizontalHeader()->sectionSize(i));
        }
        group->column_width[logicalIndex] = newSize;
        group->Save();
    });
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableWidget_conn->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    ui->proxyListTable->verticalHeader()->setDefaultSectionSize(24);

    // search box
    ui->search->setVisible(false);
    connect(shortcut_ctrl_f, &QShortcut::activated, this, [=] {
        ui->search->setVisible(true);
        ui->search->setFocus();
    });
    connect(shortcut_esc, &QShortcut::activated, this, [=] {
        if (drawer_open) {
            set_drawer_open(false);
            return;
        }
        if (ui->search->isVisible()) {
            ui->search->setText("");
            ui->search->textChanged("");
            ui->search->setVisible(false);
        }
        if (select_mode) {
            emit profile_selected(-1);
            select_mode = false;
            refresh_status();
        }
    });
    connect(ui->search, &QLineEdit::textChanged, this, [=](const QString &text) {
        if (text.isEmpty()) {
            for (int i = 0; i < ui->proxyListTable->rowCount(); i++) {
                ui->proxyListTable->setRowHidden(i, false);
            }
        } else {
            QList<QTableWidgetItem *> findItem = ui->proxyListTable->findItems(text, Qt::MatchContains);
            for (int i = 0; i < ui->proxyListTable->rowCount(); i++) {
                ui->proxyListTable->setRowHidden(i, true);
            }
            for (auto item: findItem) {
                if (item != nullptr) ui->proxyListTable->setRowHidden(item->row(), false);
            }
        }
    });

    // refresh
    this->refresh_groups();

    // Setup Tray
    tray = new QSystemTrayIcon(this); // initialize tray icon
    tray->setIcon(Icon::GetTrayIcon(Icon::NONE));
    tray->setContextMenu(ui->menu_program); // attach tray menu
    tray->show();                           // show tray icon
    connect(tray, &QSystemTrayIcon::activated, this, [=](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            if (this->isVisible()) {
                hide();
            } else {
                ActivateWindow(this);
            }
        }
    });

    // Misc menu
    connect(ui->menu_open_config_folder, &QAction::triggered, this, [=] { QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath())); });
    ui->menu_program_preference->addActions(ui->menu_preferences->actions());
    connect(ui->menu_add_from_clipboard2, &QAction::triggered, ui->menu_add_from_clipboard, &QAction::trigger);
    connect(ui->actionRestart_Proxy, &QAction::triggered, this, [=] { if (NekoGui::dataStore->started_id>=0) startProxy(NekoGui::dataStore->started_id); });
    connect(ui->actionRestart_Program, &QAction::triggered, this, [=] { MW_dialog_message("", "RestartProgram"); });
    connect(ui->actionShow_window, &QAction::triggered, this, [=] { tray->activated(QSystemTrayIcon::ActivationReason::Trigger); });
    //
    connect(ui->menu_program, &QMenu::aboutToShow, this, [=]() {
        ui->actionRemember_last_proxy->setChecked(NekoGui::dataStore->remember_enable);
        ui->actionStart_with_system->setChecked(AutoRun_IsEnabled());
        ui->actionAllow_LAN->setChecked(QStringList{"::", "0.0.0.0"}.contains(NekoGui::dataStore->inbound_address));
        // active server
        for (const auto &old: ui->menuActive_Server->actions()) {
            ui->menuActive_Server->removeAction(old);
            old->deleteLater();
        }
        int active_server_item_count = 0;
        for (const auto &pf: NekoGui::profileManager->CurrentGroup()->ProfilesWithOrder()) {
            auto a = new QAction(pf->bean->DisplayTypeAndName(), this);
            a->setProperty("id", pf->id);
            a->setCheckable(true);
            if (NekoGui::dataStore->started_id == pf->id) a->setChecked(true);
            ui->menuActive_Server->addAction(a);
            if (++active_server_item_count == 100) break;
        }
        // active routing
        for (const auto &old: ui->menuActive_Routing->actions()) {
            ui->menuActive_Routing->removeAction(old);
            old->deleteLater();
        }
        for (const auto &name: NekoGui::Routing::List()) {
            auto a = new QAction(name, this);
            a->setCheckable(true);
            a->setChecked(name == NekoGui::dataStore->active_routing);
            ui->menuActive_Routing->addAction(a);
        }
    });
    connect(ui->menuActive_Server, &QMenu::triggered, this, [=](QAction *a) {
        bool ok;
        auto id = a->property("id").toInt(&ok);
        if (!ok) return;
        if (NekoGui::dataStore->started_id == id) {
            stopProxy();
        } else {
            startProxy(id);
        }
    });
    connect(ui->menuActive_Routing, &QMenu::triggered, this, [=](QAction *a) {
        auto fn = a->text();
        if (!fn.isEmpty()) {
            NekoGui::Routing r;
            r.load_control_must = true;
            r.fn = ROUTES_PREFIX + fn;
            if (r.Load()) {
                if (QMessageBox::question(GetMessageBoxParent(), software_name, tr("Load routing and apply: %1").arg(fn) + "\n" + r.DisplayRouting()) == QMessageBox::Yes) {
                    NekoGui::Routing::SetToActive(fn);
                    if (NekoGui::dataStore->started_id >= 0) {
                        startProxy(NekoGui::dataStore->started_id);
                    } else {
                        refresh_status();
                    }
                }
            }
        }
    });
    connect(ui->actionRemember_last_proxy, &QAction::triggered, this, [=](bool checked) {
        NekoGui::dataStore->remember_enable = checked;
        NekoGui::dataStore->Save();
    });
    connect(ui->actionStart_with_system, &QAction::triggered, this, [=](bool checked) {
        AutoRun_SetEnabled(checked);
    });
    connect(ui->actionAllow_LAN, &QAction::triggered, this, [=](bool checked) {
        NekoGui::dataStore->inbound_address = checked ? "::" : "127.0.0.1";
        MW_dialog_message("", "UpdateDataStore");
    });
    //
    connect(ui->checkBox_VPN, &QCheckBox::clicked, this, [=](bool checked) { setVpnMode(checked); });
    connect(ui->checkBox_SystemProxy, &QCheckBox::clicked, this, [=](bool checked) { setSystemProxyMode(checked); });
    connect(ui->menu_spmode, &QMenu::aboutToShow, this, [=]() {
        ui->menu_spmode_disabled->setChecked(!(NekoGui::dataStore->spmode_system_proxy || NekoGui::dataStore->spmode_vpn));
        ui->menu_spmode_system_proxy->setChecked(NekoGui::dataStore->spmode_system_proxy);
        ui->menu_spmode_vpn->setChecked(NekoGui::dataStore->spmode_vpn);
    });
    connect(ui->menu_spmode_system_proxy, &QAction::triggered, this, [=](bool checked) { setSystemProxyMode(checked); });
    connect(ui->menu_spmode_vpn, &QAction::triggered, this, [=](bool checked) { setVpnMode(checked); });
    connect(ui->menu_spmode_disabled, &QAction::triggered, this, [=]() {
        setSystemProxyMode(false);
        setVpnMode(false);
    });
    connect(ui->menu_qr, &QAction::triggered, this, [=]() { display_qr_link(false); });
    connect(ui->menu_tcp_ping, &QAction::triggered, this, [=]() { speedtest_current_group(0, false); });
    connect(ui->menu_url_test, &QAction::triggered, this, [=]() { speedtest_current_group(1, false); });
    connect(ui->menu_full_test, &QAction::triggered, this, [=]() { speedtest_current_group(2, false); });
    connect(ui->menu_stop_testing, &QAction::triggered, this, [=]() { speedtest_current_group(114514, false); });
    //
    auto set_selected_or_group = [=](int mode) {
        // 0=group 1=select 2=unknown(menu is hide)
        ui->menu_server->setProperty("selected_or_group", mode);
    };
    auto move_tests_to_menu = [=](bool menuCurrent_Select) {
        return [=] {
            if (menuCurrent_Select) {
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_tcp_ping);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_url_test);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_full_test);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_stop_testing);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_clear_test_result);
                ui->menuCurrent_Select->insertAction(ui->actionfake_4, ui->menu_resolve_domain);
            } else {
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_tcp_ping);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_url_test);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_full_test);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_stop_testing);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_clear_test_result);
                ui->menuCurrent_Group->insertAction(ui->actionfake_5, ui->menu_resolve_domain);
            }
            set_selected_or_group(menuCurrent_Select ? 1 : 0);
        };
    };
    connect(ui->menuCurrent_Select, &QMenu::aboutToShow, this, move_tests_to_menu(true));
    connect(ui->menuCurrent_Group, &QMenu::aboutToShow, this, move_tests_to_menu(false));
    connect(ui->menu_server, &QMenu::aboutToHide, this, [=] {
        setTimeout([=] { set_selected_or_group(2); }, this, 200);
    });
    set_selected_or_group(2);
    //
    connect(ui->menu_share_item, &QMenu::aboutToShow, this, [=] {
        QString name;
        auto selected = get_now_selected_list();
        if (!selected.isEmpty()) {
            auto ent = selected.first();
            name = ent->bean->DisplayCoreType();
        }
        ui->menu_export_config->setVisible(name == software_core_name);
        ui->menu_export_config->setText(tr("Export %1 config").arg(name));
    });
    refresh_status();

    // Prepare core
    NekoGui::dataStore->core_token = GetRandomString(32);
    NekoGui::dataStore->core_port = MkPort();
    if (NekoGui::dataStore->core_port <= 0) NekoGui::dataStore->core_port = 19810;

    auto core_path = QApplication::applicationDirPath() + "/";
    core_path += "cofebox_core";

    QStringList args;
    args.push_back("cofebox");
    args.push_back("-port");
    args.push_back(Int2String(NekoGui::dataStore->core_port));
    if (NekoGui::dataStore->flag_debug) args.push_back("-debug");

    // Start core
    runOnUiThread(
        [=] {
            core_process = new NekoGui_sys::CoreProcess(core_path, args);
            // Remember last started
            if (NekoGui::dataStore->remember_enable && NekoGui::dataStore->remember_id >= 0) {
                core_process->start_profile_when_core_is_up = NekoGui::dataStore->remember_id;
            }
            // Setup
            core_process->Start();
            setup_grpc();
        },
        DS_cores);

    // Remember system proxy
    if (NekoGui::dataStore->remember_enable || NekoGui::dataStore->flag_restart_tun_on) {
        if (NekoGui::dataStore->remember_spmode.contains("system_proxy")) {
            setSystemProxyMode(true, false);
        }
        if (NekoGui::dataStore->remember_spmode.contains("vpn") || NekoGui::dataStore->flag_restart_tun_on) {
            setVpnMode(true, false);
        }
    }

    connect(qApp, &QGuiApplication::commitDataRequest, this, &MainWindow::on_commitDataRequest);

    auto t = new QTimer;
    connect(t, &QTimer::timeout, this, [=]() { refresh_status(); });
    t->start(2000);

    t = new QTimer;
    connect(t, &QTimer::timeout, this, [&] { NekoGui_sys::logCounter.fetchAndStoreRelaxed(0); });
    t->start(1000);

    TM_auto_update_subsctiption = new QTimer;
    TM_auto_update_subsctiption_Reset_Minute = [&](int m) {
        TM_auto_update_subsctiption->stop();
        if (m >= 30) TM_auto_update_subsctiption->start(m * 60 * 1000);
    };
    connect(TM_auto_update_subsctiption, &QTimer::timeout, this, [&] { UI_update_all_groups(true); });
    TM_auto_update_subsctiption_Reset_Minute(NekoGui::dataStore->sub_auto_update);

    if (!NekoGui::dataStore->flag_tray) show();
}

void MainWindow::setupHotspotUi() {
    if (ui == nullptr || ui->rulesLayout == nullptr || ui->page_rules == nullptr) return;

    if (ui->rules_open != nullptr) ui->rules_open->setText(tr("Open routing settings"));
    if (ui->rules_app_routing != nullptr) ui->rules_app_routing->setText(tr("Open application routing"));
    if (ui->rules_active_label != nullptr) {
        ui->rules_active_label->setText(tr("Active routing: %1").arg(NekoGui::dataStore->active_routing));
    }
    if (ui->rules_app_summary != nullptr) {
        ui->rules_app_summary->setText(tr("App routing: all applications through Proxy/TUN"));
    }

    hotspotService_ = new HotspotGatewayService(this);
    hotspotService_->setCredentials(NekoGui::dataStore->hotspot_ssid, NekoGui::dataStore->hotspot_password);

    hotspotCard_ = new QFrame(ui->page_rules);
    hotspotCard_->setObjectName("hotspot_gateway_card");
    auto *root = new QVBoxLayout(hotspotCard_);
    root->setContentsMargins(10, 10, 10, 10);
    root->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    auto *title = new QLabel(tr("Hotspot Gateway (CofeBox network)"), hotspotCard_);
    hotspotToggle_ = new QCheckBox(tr("Share internet (Hotspot)"), hotspotCard_);
    hotspotStatusLabel_ = new QLabel(tr("Idle"), hotspotCard_);
    titleRow->addWidget(title, 1);
    titleRow->addWidget(hotspotToggle_, 0);
    titleRow->addWidget(hotspotStatusLabel_, 0);
    root->addLayout(titleRow);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(6);
    grid->addWidget(new QLabel(tr("SSID"), hotspotCard_), 0, 0);
    hotspotSsidValue_ = new QLabel(hotspotCard_);
    hotspotSsidValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(hotspotSsidValue_, 0, 1);

    grid->addWidget(new QLabel(tr("Password"), hotspotCard_), 1, 0);
    hotspotPasswordValue_ = new QLabel(hotspotCard_);
    hotspotPasswordValue_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    grid->addWidget(hotspotPasswordValue_, 1, 1);

    grid->addWidget(new QLabel(tr("Connected devices"), hotspotCard_), 2, 0);
    hotspotDevicesValue_ = new QLabel("0", hotspotCard_);
    grid->addWidget(hotspotDevicesValue_, 2, 1);

    grid->addWidget(new QLabel(tr("Mode"), hotspotCard_), 3, 0);
    hotspotModeValue_ = new QLabel(tr("Full-tunnel for Hotspot"), hotspotCard_);
    grid->addWidget(hotspotModeValue_, 3, 1);
    root->addLayout(grid);

    auto *buttons = new QHBoxLayout;
    hotspotStartButton_ = new QPushButton(tr("Start"), hotspotCard_);
    hotspotStopButton_ = new QPushButton(tr("Stop"), hotspotCard_);
    hotspotDiagButton_ = new QPushButton(tr("Diagnostics"), hotspotCard_);
    hotspotCopyPassButton_ = new QPushButton(tr("Copy password"), hotspotCard_);
    hotspotRegenButton_ = new QPushButton(tr("Regenerate"), hotspotCard_);
    hotspotQrButton_ = new QPushButton(tr("Show QR"), hotspotCard_);
    buttons->addWidget(hotspotStartButton_);
    buttons->addWidget(hotspotStopButton_);
    buttons->addWidget(hotspotDiagButton_);
    buttons->addWidget(hotspotCopyPassButton_);
    buttons->addWidget(hotspotRegenButton_);
    buttons->addWidget(hotspotQrButton_);
    root->addLayout(buttons);

    ui->rulesLayout->insertWidget(1, hotspotCard_);

    connect(hotspotToggle_, &QCheckBox::clicked, this, [this](bool checked) {
        if (checked) {
            if (!NekoGui::dataStore->spmode_vpn) {
                show_toast_error(tr("Hotspot is enabled, but internet is not shared: check tunnel connection."));
                QSignalBlocker blocker(hotspotToggle_);
                hotspotToggle_->setChecked(false);
                return;
            }
            if (!hotspotService_->start()) {
                show_toast_error(hotspotService_->lastMessage());
                QSignalBlocker blocker(hotspotToggle_);
                hotspotToggle_->setChecked(false);
                return;
            }
            NekoGui::dataStore->hotspot_enabled = true;
            NekoGui::dataStore->Save();
        } else {
            hotspotService_->stop();
            NekoGui::dataStore->hotspot_enabled = false;
            NekoGui::dataStore->Save();
        }
        syncHotspotUi();
    });
    connect(hotspotStartButton_, &QPushButton::clicked, this, [this] {
        if (!NekoGui::dataStore->spmode_vpn) {
            show_toast_error(tr("Hotspot is enabled, but internet is not shared: check tunnel connection."));
            return;
        }
        if (!hotspotService_->start()) {
            show_toast_error(hotspotService_->lastMessage());
            return;
        }
        NekoGui::dataStore->hotspot_enabled = true;
        NekoGui::dataStore->Save();
        syncHotspotUi();
    });
    connect(hotspotStopButton_, &QPushButton::clicked, this, [this] {
        hotspotService_->stop();
        NekoGui::dataStore->hotspot_enabled = false;
        NekoGui::dataStore->Save();
        syncHotspotUi();
    });
    connect(hotspotDiagButton_, &QPushButton::clicked, this, [this] { hotspotService_->runDiagnostics(); });
    connect(hotspotCopyPassButton_, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(hotspotService_->runtime().password);
        show_toast_success(tr("Password copied."));
    });
    connect(hotspotRegenButton_, &QPushButton::clicked, this, [this] {
        hotspotService_->regenerateCredentials();
        NekoGui::dataStore->hotspot_ssid = hotspotService_->runtime().ssid;
        NekoGui::dataStore->hotspot_password = hotspotService_->runtime().password;
        NekoGui::dataStore->Save();
        syncHotspotUi();
    });
    connect(hotspotQrButton_, &QPushButton::clicked, this, [this] { showHotspotQrDialog(); });

    connect(hotspotService_, &HotspotGatewayService::stateChanged, this, [this](HotspotGatewayService::State state, const QString &message) {
        if (state == HotspotGatewayService::State::Failed) {
            MessageBoxWarning(software_name, message);
        }
        syncHotspotUi();
    });
    connect(hotspotService_, &HotspotGatewayService::devicesChanged, this, [this](const QVector<HotspotDeviceInfo> &) {
        syncHotspotUi();
    });
    connect(hotspotService_, &HotspotGatewayService::diagReport, this, [this](bool ok, const QString &report) {
        if (ok) {
            QMessageBox::information(this, tr("Hotspot diagnostics"), report);
        } else {
            MessageBoxWarning(tr("Hotspot diagnostics"), report);
        }
    });
    connect(hotspotService_, &HotspotGatewayService::credentialsChanged, this, [this](const QString &, const QString &) {
        NekoGui::dataStore->hotspot_ssid = hotspotService_->runtime().ssid;
        NekoGui::dataStore->hotspot_password = hotspotService_->runtime().password;
        NekoGui::dataStore->Save();
        syncHotspotUi();
    });

    syncHotspotUi();
    if (NekoGui::dataStore->hotspot_enabled && NekoGui::dataStore->spmode_vpn) {
        QTimer::singleShot(900, this, [this] {
            hotspotService_->start();
            syncHotspotUi();
        });
    }
}

void MainWindow::syncHotspotUi() {
    if (hotspotService_ == nullptr || hotspotCard_ == nullptr) return;
    const auto runtime = hotspotService_->runtime();
    const auto state = hotspotService_->state();
    const auto devices = hotspotService_->devices();

    if (hotspotSsidValue_ != nullptr) hotspotSsidValue_->setText(runtime.ssid);
    if (hotspotPasswordValue_ != nullptr) hotspotPasswordValue_->setText(hotspotService_->maskedPassword());
    if (hotspotDevicesValue_ != nullptr) hotspotDevicesValue_->setText(Int2String(devices.size()));
    if (hotspotModeValue_ != nullptr) hotspotModeValue_->setText(tr("Full-tunnel for Hotspot"));

    QString statusText;
    switch (state) {
        case HotspotGatewayService::State::Idle:
            statusText = tr("Idle");
            break;
        case HotspotGatewayService::State::Starting:
            statusText = tr("Starting");
            break;
        case HotspotGatewayService::State::Running:
            statusText = tr("Running");
            break;
        case HotspotGatewayService::State::Stopping:
            statusText = tr("Stopping");
            break;
        case HotspotGatewayService::State::Failed:
            statusText = tr("Failed");
            break;
    }
    if (!hotspotService_->lastMessage().isEmpty()) {
        statusText += QStringLiteral(": ") + hotspotService_->lastMessage();
    }
    if (hotspotStatusLabel_ != nullptr) hotspotStatusLabel_->setText(statusText);

    const bool busy = state == HotspotGatewayService::State::Starting || state == HotspotGatewayService::State::Stopping;
    const bool running = state == HotspotGatewayService::State::Running;
    if (hotspotToggle_ != nullptr) {
        QSignalBlocker blocker(hotspotToggle_);
        hotspotToggle_->setChecked(running);
        hotspotToggle_->setEnabled(!busy);
    }
    if (hotspotStartButton_ != nullptr) hotspotStartButton_->setEnabled(!busy && !running);
    if (hotspotStopButton_ != nullptr) hotspotStopButton_->setEnabled(!busy && running);
    if (hotspotDiagButton_ != nullptr) hotspotDiagButton_->setEnabled(!busy);
    if (hotspotCopyPassButton_ != nullptr) hotspotCopyPassButton_->setEnabled(!runtime.password.isEmpty());
    if (hotspotRegenButton_ != nullptr) hotspotRegenButton_->setEnabled(!busy);
    if (hotspotQrButton_ != nullptr) hotspotQrButton_->setEnabled(!runtime.ssid.isEmpty() && !runtime.password.isEmpty());
}

void MainWindow::showHotspotQrDialog() {
    if (hotspotService_ == nullptr) return;
    const auto runtime = hotspotService_->runtime();
    if (runtime.ssid.isEmpty() || runtime.password.isEmpty()) {
        MessageBoxWarning(software_name, tr("Hotspot credentials are empty."));
        return;
    }

    const auto qrText = hotspotService_->wifiQrText();
    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(qrText.toUtf8().constData(), qrcodegen::QrCode::Ecc::MEDIUM);
    const int border = 4;
    const int scale = 6;
    const int side = (qr.getSize() + border * 2) * scale;
    QImage image(side, side, QImage::Format_RGB32);
    image.fill(Qt::white);
    for (int y = 0; y < qr.getSize(); ++y) {
        for (int x = 0; x < qr.getSize(); ++x) {
            if (!qr.getModule(x, y)) continue;
            const int px = (x + border) * scale;
            const int py = (y + border) * scale;
            for (int oy = 0; oy < scale; ++oy) {
                for (int ox = 0; ox < scale; ++ox) {
                    image.setPixel(px + ox, py + oy, qRgb(0, 0, 0));
                }
            }
        }
    }
    QMessageBox box(this);
    box.setWindowTitle(tr("Hotspot QR"));
    box.setIconPixmap(QPixmap::fromImage(image));
    box.setText(tr("SSID: %1\nPassword: %2").arg(runtime.ssid, runtime.password));
    box.exec();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (tray->isVisible()) {
        hide();          // keep app in tray
        event->ignore(); // ignore close event
    }
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    update_drawer_scrim();
    update_home_running_elide();
    update_connect_button();
    if (toast != nullptr) {
        toast->setAnchorRect(ui->centralwidget->rect());
    }
}

void MainWindow::set_home_running_text(const QString &text, const QString &tooltip) {
    home_running_full_text = text;
    home_running_tooltip = tooltip;
    update_home_running_elide();
}

void MainWindow::update_home_running_elide() {
    if (ui == nullptr || ui->label_running == nullptr) return;
    const int available = qMax(0, ui->label_running->contentsRect().width() - 4);
    const QString elided = ui->label_running->fontMetrics().elidedText(home_running_full_text, Qt::ElideRight, available);
    ui->label_running->setText(elided);
    if (!home_running_tooltip.isEmpty()) {
        ui->label_running->setToolTip(home_running_tooltip);
    } else if (elided != home_running_full_text) {
        ui->label_running->setToolTip(home_running_full_text);
    } else {
        ui->label_running->setToolTip({});
    }
}

void MainWindow::sync_drawer_theme(const QString &themeKey) {
    if (drawer_theme_group == nullptr || ui->drawer_theme_label == nullptr) return;
    const auto normalized = NormalizeThemeKey(themeKey);
    for (auto *button : drawer_theme_group->buttons()) {
        const auto key = button->property("themeKey").toString();
        const auto option = themeManager->ThemeOptionFor(key);
        QSignalBlocker blocker(button);
        button->setChecked(key == normalized);
        button->setIcon(BuildThemeSwatchIcon(option, 20));
        button->setIconSize(QSize(20, 20));
        button->setToolTip(option.displayName);
    }
    const auto option = themeManager->ThemeOptionFor(normalized);
    ui->drawer_theme_label->setText(tr("Theme: %1").arg(option.displayName));
}

MainWindow::~MainWindow() {
    delete ui;
}

// Group tab manage

inline int tabIndex2GroupId(int index) {
    if (NekoGui::profileManager->groupsTabOrder.length() <= index) return -1;
    return NekoGui::profileManager->groupsTabOrder[index];
}

inline int groupId2TabIndex(int gid) {
    for (int key = 0; key < NekoGui::profileManager->groupsTabOrder.count(); key++) {
        if (NekoGui::profileManager->groupsTabOrder[key] == gid) return key;
    }
    return 0;
}

void MainWindow::on_tabWidget_currentChanged(int index) {
    if (NekoGui::dataStore->refreshing_group_list) return;
    if (tabIndex2GroupId(index) == NekoGui::dataStore->current_group) return;
    show_group(tabIndex2GroupId(index));
}

void MainWindow::show_group(int gid) {
    if (NekoGui::dataStore->refreshing_group) return;
    NekoGui::dataStore->refreshing_group = true;

    auto group = NekoGui::profileManager->GetGroup(gid);
    if (group == nullptr) {
        MessageBoxWarning(tr("Error"), QStringLiteral("No such group: %1").arg(gid));
        NekoGui::dataStore->refreshing_group = false;
        return;
    }

    if (NekoGui::dataStore->current_group != gid) {
        NekoGui::dataStore->current_group = gid;
        NekoGui::dataStore->Save();
    }
    ui->tabWidget->widget(groupId2TabIndex(gid))->layout()->addWidget(ui->proxyListTable);

    // restore column widths
    if (group->manually_column_width) {
        for (int i = 0; i <= 4; i++) {
            ui->proxyListTable->horizontalHeader()->setSectionResizeMode(i, QHeaderView::Interactive);
            auto size = group->column_width.value(i);
            if (size <= 0) size = ui->proxyListTable->horizontalHeader()->defaultSectionSize();
            ui->proxyListTable->horizontalHeader()->resizeSection(i, size);
        }
    } else {
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        ui->proxyListTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    }

    // show proxies
    GroupSortAction gsa;
    gsa.scroll_to_started = true;
    refresh_proxy_list_impl(-1, gsa);

    NekoGui::dataStore->refreshing_group = false;
}

// callback

void MainWindow::dialog_message_impl(const QString &sender, const QString &info) {
    // info
    if (info.contains("UpdateIcon")) {
        icon_status = -1;
        refresh_status();
    }
    if (info.contains("UpdateDataStore")) {
        auto suggestRestartProxy = NekoGui::dataStore->Save();
        if (info.contains("RouteChanged")) {
            suggestRestartProxy = true;
        }
        if (info.contains("NeedRestart")) {
            suggestRestartProxy = false;
        }
        refresh_proxy_list();
        sync_drawer_theme(NekoGui::dataStore->theme);
        if (auto wave = qobject_cast<WaveBackground *>(ui->centralwidget)) {
            wave->setReduceMotion(NekoGui::dataStore->reduce_motion);
        }
        if (auto connectButton = qobject_cast<ConnectButton *>(ui->home_connect_button)) {
            connectButton->setReduceMotion(NekoGui::dataStore->reduce_motion);
        }
        if (info.contains("VPNChanged") && NekoGui::dataStore->spmode_vpn) {
            MessageBoxWarning(tr("Tun Settings changed"), tr("Restart Tun to take effect."));
        }
        if (suggestRestartProxy && NekoGui::dataStore->started_id >= 0 &&
            QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
            startProxy(NekoGui::dataStore->started_id);
        }
        refresh_status();
    }
    if (info.contains("NeedRestart")) {
        auto n = QMessageBox::warning(GetMessageBoxParent(), tr("Settings changed"), tr("Restart the program to take effect."), QMessageBox::Yes | QMessageBox::No);
        if (n == QMessageBox::Yes) {
            this->exit_reason = 2;
            on_menu_exit_triggered();
        }
    }
    //
    if (info == "RestartProgram") {
        this->exit_reason = 2;
        on_menu_exit_triggered();
    } else if (info == "Raise") {
        ActivateWindow(this);
    } else if (info == "ClearConnectionList") {
        refresh_connection_list({});
    }
    // sender
    if (sender == Dialog_DialogEditProfile) {
        auto msg = info.split(",");
        if (msg.contains("accept")) {
            refresh_proxy_list();
            if (msg.contains("restart")) {
                if (QMessageBox::question(GetMessageBoxParent(), tr("Confirmation"), tr("Settings changed, restart proxy?")) == QMessageBox::StandardButton::Yes) {
                    startProxy(NekoGui::dataStore->started_id);
                }
            }
        }
    } else if (sender == Dialog_DialogManageGroups) {
        if (info.startsWith("refresh")) {
            this->refresh_groups();
        }
    } else if (sender == "SubUpdater") {
        if (info.startsWith("finish")) {
            refresh_proxy_list();
            refresh_subscriptions_list();
            if (!info.contains("dingyue")) {
                show_log_impl(tr("Imported %1 profile(s)").arg(NekoGui::dataStore->imported_count));
            }
        } else if (info == "NewGroup") {
            refresh_groups();
        }
    } else if (sender == "ExternalProcess") {
        if (info == "Crashed") {
            stopProxy();
        } else if (info == "CoreCrashed") {
            stopProxy(true);
        } else if (info.startsWith("CoreStarted")) {
            startProxy(info.split(",")[1].toInt());
        }
    }
}

// top bar & tray menu

inline bool dialog_is_using = false;

#define USE_DIALOG(a)                               \
    if (dialog_is_using) return;                    \
    dialog_is_using = true;                         \
    auto dialog = new a(this);                      \
    connect(dialog, &QDialog::finished, this, [=] { \
        dialog->deleteLater();                      \
        dialog_is_using = false;                    \
    });                                             \
    dialog->show();

void MainWindow::on_menu_basic_settings_triggered() {
    USE_DIALOG(DialogBasicSettings)
}

void MainWindow::on_menu_manage_groups_triggered() {
    USE_DIALOG(DialogManageGroups)
}

void MainWindow::on_menu_routing_settings_triggered() {
    USE_DIALOG(DialogManageRoutes)
}

void MainWindow::on_menu_vpn_settings_triggered() {
    USE_DIALOG(DialogVPNSettings)
}

void MainWindow::on_menu_hotkey_settings_triggered() {
    USE_DIALOG(DialogHotkey)
}

void MainWindow::on_commitDataRequest() {
    qDebug() << "Start of data save";
    //
    if (!isMaximized()) {
        auto olds = NekoGui::dataStore->mw_size;
        auto news = QStringLiteral("%1x%2").arg(size().width()).arg(size().height());
        if (olds != news) {
            NekoGui::dataStore->mw_size = news;
        }
    }
    //
    auto last_id = NekoGui::dataStore->started_id;
    if (NekoGui::dataStore->remember_enable && last_id >= 0) {
        NekoGui::dataStore->remember_id = last_id;
    }
    //
    NekoGui::dataStore->Save();
    NekoGui::profileManager->SaveManager();
    qDebug() << "End of data save";
}

void MainWindow::on_menu_exit_triggered() {
    if (mu_exit.tryLock()) {
        NekoGui::dataStore->prepare_exit = true;
        //
        setSystemProxyMode(false, false);
        setVpnMode(false, false);
        if (NekoGui::dataStore->spmode_vpn) {
            mu_exit.unlock(); // retry
            return;
        }
        RegisterHotkey(true);
        //
        on_commitDataRequest();
        //
        NekoGui::dataStore->save_control_no_save = true; // don't change datastore after this line
        stopProxy(false, true);
        //
        hide();
        runOnNewThread([=] {
            sem_stopped.acquire();
            stop_core_daemon();
            runOnUiThread([=] {
                on_menu_exit_triggered(); // continue exit progress
            });
        });
        return;
    }
    //
    MF_release_runguard();
    if (exit_reason == 1) {
        QDir::setCurrent(QApplication::applicationDirPath());
        QProcess::startDetached("./updater", QStringList{});
    } else if (exit_reason == 2 || exit_reason == 3) {
        QDir::setCurrent(QApplication::applicationDirPath());

        auto arguments = NekoGui::dataStore->argv;
        if (arguments.length() > 0) {
            arguments.removeFirst();
            arguments.removeAll("-tray");
            arguments.removeAll("-flag_restart_tun_on");
            arguments.removeAll("-flag_reorder");
        }
        auto isLauncher = qEnvironmentVariable("NKR_FROM_LAUNCHER") == "1";
        if (isLauncher) arguments.prepend("--");
        auto program = isLauncher ? "./launcher" : QApplication::applicationFilePath();

        if (exit_reason == 3) {
            // Tun restart as admin
            arguments << "-flag_restart_tun_on";
#ifdef Q_OS_WIN
            WinCommander::runProcessElevated(program, arguments, "", WinCommander::SW_NORMAL, false);
#else
            QProcess::startDetached(program, arguments);
#endif
        } else {
            QProcess::startDetached(program, arguments);
        }
    }
    tray->hide();
    QCoreApplication::quit();
}

#define set_spmode_FAILED \
    refresh_status();          \
    return;

void MainWindow::setSystemProxyMode(bool enable, bool save) {
    if (enable != NekoGui::dataStore->spmode_system_proxy) {
        if (enable) {
            auto socks_port = NekoGui::dataStore->inbound_socks_port;
            auto http_port = NekoGui::dataStore->inbound_socks_port;
            SetSystemProxy(http_port, socks_port);
        } else {
            ClearSystemProxy();
        }
    }

    if (save) {
        NekoGui::dataStore->remember_spmode.removeAll("system_proxy");
        if (enable && NekoGui::dataStore->remember_enable) {
            NekoGui::dataStore->remember_spmode.append("system_proxy");
        }
        NekoGui::dataStore->Save();
    }

    NekoGui::dataStore->spmode_system_proxy = enable;
    refresh_status();
}

void MainWindow::setVpnMode(bool enable, bool save) {
    if (enable != NekoGui::dataStore->spmode_vpn) {
        if (enable) {
            if (NekoGui::dataStore->vpn_internal_tun) {
                bool requestPermission = !NekoGui::IsAdmin();
                if (requestPermission) {
#ifdef Q_OS_LINUX
                    if (!Linux_HavePkexec()) {
                        MessageBoxWarning(software_name, "Please install \"pkexec\" first.");
                        set_spmode_FAILED
                    }
                    auto ret = Linux_Pkexec_SetCapString(NekoGui::FindCoreRealPath(), "cap_net_admin=ep");
                    if (ret == 0) {
                        this->exit_reason = 3;
                        on_menu_exit_triggered();
                    } else {
                        MessageBoxWarning(software_name, "Setcap for Tun mode failed.\n\n1. You may canceled the dialog.\n2. You may be using an incompatible environment like AppImage.");
                        if (QProcessEnvironment::systemEnvironment().contains("APPIMAGE")) {
                            MW_show_log("If you are using AppImage, it's impossible to start a Tun. Please use other package instead.");
                        }
                    }
#endif
#ifdef Q_OS_WIN
                    auto n = QMessageBox::warning(GetMessageBoxParent(), software_name, tr("Please run CofeBox as admin"), QMessageBox::Yes | QMessageBox::No);
                    if (n == QMessageBox::Yes) {
                        this->exit_reason = 3;
                        on_menu_exit_triggered();
                    }
#endif
                    set_spmode_FAILED
                }
            } else {
                if (NekoGui::dataStore->need_keep_vpn_off) {
                    MessageBoxWarning(software_name, tr("Current server is incompatible with Tun. Please stop the server first, enable Tun Mode, and then restart."));
                    set_spmode_FAILED
                }
                if (!StartVPNProcess()) {
                    set_spmode_FAILED
                }
            }
        } else {
            if (NekoGui::dataStore->vpn_internal_tun) {
                // current core is sing-box
            } else {
                if (!StopVPNProcess()) {
                    set_spmode_FAILED
                }
            }
        }
    }

    if (save) {
        NekoGui::dataStore->remember_spmode.removeAll("vpn");
        if (enable && NekoGui::dataStore->remember_enable) {
            NekoGui::dataStore->remember_spmode.append("vpn");
        }
        NekoGui::dataStore->Save();
    }

    NekoGui::dataStore->spmode_vpn = enable;

    if (!enable && hotspotService_ != nullptr &&
        hotspotService_->state() != HotspotGatewayService::State::Idle) {
        hotspotService_->stop();
        NekoGui::dataStore->hotspot_enabled = false;
    }
    if (enable && hotspotService_ != nullptr && NekoGui::dataStore->hotspot_enabled &&
        hotspotService_->state() == HotspotGatewayService::State::Idle) {
        hotspotService_->start();
    }

    refresh_status();
    syncHotspotUi();

    if (NekoGui::dataStore->vpn_internal_tun && NekoGui::dataStore->started_id >= 0) startProxy(NekoGui::dataStore->started_id);
}

void MainWindow::refresh_status(const QString &traffic_update) {
    auto refresh_speed_label = [=] {
        if (traffic_update_cache == "") {
            ui->label_speed->setText(QObject::tr("Proxy: %1\nDirect: %2").arg("", ""));
        } else {
            ui->label_speed->setText(traffic_update_cache);
        }
    };

    // From TrafficLooper
    if (!traffic_update.isEmpty()) {
        traffic_update_cache = traffic_update;
        if (traffic_update == "STOP") {
            traffic_update_cache = "";
        } else {
            refresh_speed_label();
            return;
        }
    }

    refresh_speed_label();

    // From UI
    QString group_name;
    if (running != nullptr) {
        auto group = NekoGui::profileManager->GetGroup(running->gid);
        if (group != nullptr) group_name = group->name;
    }

    if (connect_state == ConnectState::Connecting) {
        set_home_running_text(tr("Connecting"));
    } else if (connect_state == ConnectState::Disconnecting) {
        set_home_running_text(tr("Disconnecting"));
    } else if (last_test_time.addSecs(2) < QTime::currentTime()) {
        auto txt = running == nullptr ? tr("Not Running")
                                      : QStringLiteral("[%1] %2").arg(group_name, running->bean->DisplayName());
        set_home_running_text(txt);
    }
    if (running == nullptr) {
        QString statusText = connect_state == ConnectState::Connecting ? tr("Connecting") : tr("Disconnected");
        ui->drawer_status->setText(statusText);
        ui->drawer_profile->setText(tr("Profile: -"));
        if (connect_state != ConnectState::Connecting) {
            connect_state = ConnectState::Disconnected;
        }
        ui->drawer_status->setProperty("state", "disconnected");
    } else {
        auto profile_label = running->bean->DisplayTypeAndName();
        if (!group_name.isEmpty()) {
            profile_label = QStringLiteral("%1 / %2").arg(group_name, profile_label);
        }
        QString statusText = connect_state == ConnectState::Disconnecting ? tr("Disconnecting") : tr("Connected");
        ui->drawer_status->setText(statusText);
        ui->drawer_profile->setText(tr("Profile: %1").arg(profile_label));
        if (connect_state != ConnectState::Disconnecting) {
            connect_state = ConnectState::Connected;
        }
        ui->drawer_status->setProperty("state", "connected");
    }
    ui->drawer_status->style()->unpolish(ui->drawer_status);
    ui->drawer_status->style()->polish(ui->drawer_status);
    update_connect_button();
    //
    auto display_socks = DisplayAddress(NekoGui::dataStore->inbound_address, NekoGui::dataStore->inbound_socks_port);
    auto inbound_txt = QStringLiteral("Mixed: %1").arg(display_socks);
    ui->label_inbound->setText(inbound_txt);
    ui->rules_active_label->setText(tr("Active routing: %1").arg(NekoGui::dataStore->active_routing));
    const auto processRules = AppRoutingRules::Parse(NekoGui::dataStore->vpn_rule_process);
    QString appRoutingSummary;
    if (processRules.isEmpty() && !NekoGui::dataStore->vpn_rule_white) {
        appRoutingSummary = tr("Application routing: all applications through Proxy/TUN");
    } else if (NekoGui::dataStore->vpn_rule_white) {
        appRoutingSummary = tr("Application routing: only selected (%1)").arg(processRules.size());
    } else {
        appRoutingSummary = tr("Application routing: direct exceptions (%1)").arg(processRules.size());
    }
    ui->rules_app_summary->setText(appRoutingSummary);
    //
    ui->checkBox_VPN->setChecked(NekoGui::dataStore->spmode_vpn);
    ui->checkBox_SystemProxy->setChecked(NekoGui::dataStore->spmode_system_proxy);
    if (select_mode) {
        set_home_running_text(tr("Select") + " *",
                              tr("Select mode, double-click or press Enter to select a profile, press ESC to exit."));
    } else {
        update_home_running_elide();
    }

    auto make_title = [=](bool isTray) {
        QStringList tt;
        if (!isTray && NekoGui::IsAdmin()) tt << "[Admin]";
        if (select_mode) tt << "[" + tr("Select") + "]";
        if (!title_error.isEmpty()) tt << "[" + title_error + "]";
        if (NekoGui::dataStore->spmode_vpn && !NekoGui::dataStore->spmode_system_proxy) tt << "[Tun]";
        if (!NekoGui::dataStore->spmode_vpn && NekoGui::dataStore->spmode_system_proxy) tt << "[" + tr("System Proxy") + "]";
        if (NekoGui::dataStore->spmode_vpn && NekoGui::dataStore->spmode_system_proxy) tt << "[Tun+" + tr("System Proxy") + "]";
        tt << software_name;
        if (!isTray) tt << "(" + AppInfo::Version() + ")";
        if (!NekoGui::dataStore->active_routing.isEmpty() && NekoGui::dataStore->active_routing != "Default") {
            tt << "[" + NekoGui::dataStore->active_routing + "]";
        }
        if (running != nullptr) tt << running->bean->DisplayTypeAndName() + "@" + group_name;
        return tt.join(isTray ? "\n" : " ");
    };

    auto icon_status_new = Icon::NONE;

    if (running != nullptr) {
        if (NekoGui::dataStore->spmode_vpn) {
            icon_status_new = Icon::VPN;
        } else if (NekoGui::dataStore->spmode_system_proxy) {
            icon_status_new = Icon::SYSTEM_PROXY;
        } else {
            icon_status_new = Icon::RUNNING;
        }
    }

    // refresh title & window icon
    setWindowTitle(make_title(false));
    if (icon_status_new != icon_status) QApplication::setWindowIcon(Icon::GetTrayIcon(Icon::NONE));

    // refresh tray
    if (tray != nullptr) {
        tray->setToolTip(make_title(true));
        if (icon_status_new != icon_status) tray->setIcon(Icon::GetTrayIcon(icon_status_new));
    }

    icon_status = icon_status_new;
}

void MainWindow::set_connect_state(MainWindow::ConnectState state) {
    if (connect_state == state) return;
    connect_state = state;
    update_connect_button();
    if (state == ConnectState::Connecting || state == ConnectState::Disconnecting) {
        refresh_status();
    }
}

void MainWindow::update_connect_button() {
    auto button = qobject_cast<ConnectButton *>(ui->home_connect_button);
    if (button == nullptr) return;

    ConnectButton::State state = ConnectButton::State::Disconnected;
    switch (connect_state) {
        case ConnectState::Disconnected:
            state = ConnectButton::State::Disconnected;
            break;
        case ConnectState::Connecting:
            state = ConnectButton::State::Connecting;
            break;
        case ConnectState::Connected:
            state = ConnectButton::State::Connected;
            break;
        case ConnectState::Disconnecting:
            state = ConnectButton::State::Disconnecting;
            break;
    }
    button->setState(state);
    button->refreshMetrics();
    const bool busy = connect_state == ConnectState::Connecting || connect_state == ConnectState::Disconnecting;
    button->setEnabled(!busy);
}

void MainWindow::set_drawer_open(bool open, bool animated) {
    drawer_open = open;
    if (drawer_anim != nullptr) {
        drawer_anim->stop();
    }

    const int target = open ? 300 : 0;
    if (open) {
        ui->drawer_container->setVisible(true);
        if (drawer_scrim) {
            drawer_scrim->setVisible(true);
            update_drawer_scrim();
        }
    }

    const bool allow_animation = animated && !NekoGui::dataStore->reduce_motion;
    if (allow_animation && drawer_anim_max != nullptr && drawer_anim_min != nullptr) {
        drawer_anim_max->setStartValue(ui->drawer_container->maximumWidth());
        drawer_anim_min->setStartValue(ui->drawer_container->minimumWidth());
        drawer_anim_max->setEndValue(target);
        drawer_anim_min->setEndValue(target);
        drawer_anim->start();
    } else {
        ui->drawer_container->setMaximumWidth(target);
        ui->drawer_container->setMinimumWidth(target);
        if (!open) {
            ui->drawer_container->setVisible(false);
            if (drawer_scrim) drawer_scrim->setVisible(false);
        }
    }
}

void MainWindow::update_drawer_scrim() {
    if (drawer_scrim == nullptr) return;
    drawer_scrim->setGeometry(ui->centralwidget->rect());
    drawer_scrim->raise();
    ui->drawer_container->raise();
}

void MainWindow::submit_home_subscription() {
    if (!can_start_add()) return;
    auto text = ui->home_sub_url->text().trimmed();
    if (text.isEmpty()) {
        show_toast_error(tr("Please paste a subscription URL."));
        return;
    }

    QUrl url(text);
    const auto scheme = url.scheme().toLower();
    if (!url.isValid()) {
        show_toast_error(tr("Please paste a valid URL."));
        return;
    }
    if (scheme != "http" && scheme != "https") {
        show_toast_error(tr("Only http(s) URLs are allowed."));
        return;
    }

    add_in_progress = true;
    add_base_count = static_cast<int>(NekoGui::profileManager->profiles.size());
    set_add_controls_enabled(false);
    if (add_debounce_timer != nullptr) add_debounce_timer->start(400);
    NekoGui_sub::groupUpdater->AsyncUpdate(text, -1, [=] {
        runOnUiThread([=] { finish_add_operation(); });
    });
    ui->home_sub_url->clear();
}

void MainWindow::submit_servers_subscription() {
    if (!can_start_add()) return;
    auto text = ui->servers_add_url->text().trimmed();
    if (text.isEmpty()) {
        show_toast_error(tr("Please paste a link."));
        return;
    }

    add_in_progress = true;
    add_base_count = static_cast<int>(NekoGui::profileManager->profiles.size());
    set_add_controls_enabled(false);
    if (add_debounce_timer != nullptr) add_debounce_timer->start(400);
    NekoGui_sub::groupUpdater->AsyncUpdate(text, -1, [=] {
        runOnUiThread([=] { finish_add_operation(); });
    });
    ui->servers_add_url->clear();
}

void MainWindow::show_toast(const QString &text, int durationMs) {
    if (toast == nullptr) return;
    toast->setAnchorRect(ui->centralwidget->rect());
    toast->showMessage(text, ToastWidget::Level::Info, durationMs);
}

void MainWindow::show_toast_success(const QString &text) {
    if (toast == nullptr) return;
    toast->setAnchorRect(ui->centralwidget->rect());
    toast->showMessage(text, ToastWidget::Level::Success, 2500);
}

void MainWindow::show_toast_error(const QString &text) {
    if (toast == nullptr) return;
    toast->setAnchorRect(ui->centralwidget->rect());
    toast->showMessage(text, ToastWidget::Level::Error, 3000);
}

bool MainWindow::can_start_add() const {
    if (add_in_progress) return false;
    if (add_debounce_timer != nullptr && add_debounce_timer->isActive()) return false;
    return true;
}

void MainWindow::set_add_controls_enabled(bool enabled) {
    const auto busyText = tr("Adding...");
    const auto updateButton = [&](QAbstractButton *button) {
        if (button == nullptr) return;
        if (!enabled) {
            if (!button->property("text_backup").isValid()) {
                button->setProperty("text_backup", button->text());
            }
            button->setText(busyText);
        } else if (button->property("text_backup").isValid()) {
            button->setText(button->property("text_backup").toString());
            button->setProperty("text_backup", {});
        }
        button->setEnabled(enabled);
    };

    updateButton(ui->home_sub_add);
    if (ui->servers_add_button != nullptr) updateButton(ui->servers_add_button);
    if (ui->servers_add_paste != nullptr) ui->servers_add_paste->setEnabled(enabled);
    if (ui->menu_add_from_clipboard != nullptr) ui->menu_add_from_clipboard->setEnabled(enabled);
}

void MainWindow::finish_add_operation() {
    if (!add_in_progress) return;
    add_in_progress = false;
    set_add_controls_enabled(true);

    const int afterCount = static_cast<int>(NekoGui::profileManager->profiles.size());
    const int added = afterCount - add_base_count;
    if (added > 0) {
        show_toast_success(tr("Added: %1").arg(added));
    } else {
        show_toast(tr("No new items"));
    }
}

// table display

// refresh_groups -> show_group -> refresh_proxy_list
void MainWindow::refresh_groups() {
    NekoGui::dataStore->refreshing_group_list = true;

    // refresh group?
    for (int i = ui->tabWidget->count() - 1; i > 0; i--) {
        ui->tabWidget->removeTab(i);
    }

    int index = 0;
    for (const auto &gid: NekoGui::profileManager->groupsTabOrder) {
        auto group = NekoGui::profileManager->GetGroup(gid);
        if (index == 0) {
            ui->tabWidget->setTabText(0, group->name);
        } else {
            auto widget2 = new QWidget();
            auto layout2 = new QVBoxLayout();
            layout2->setContentsMargins(QMargins());
            layout2->setSpacing(0);
            widget2->setLayout(layout2);
            ui->tabWidget->addTab(widget2, group->name);
        }
        ui->tabWidget->tabBar()->setTabData(index, gid);
        index++;
    }

    // show after group changed
    if (NekoGui::profileManager->CurrentGroup() == nullptr) {
        NekoGui::dataStore->current_group = -1;
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(0));
        show_group(NekoGui::profileManager->groupsTabOrder.count() > 0 ? NekoGui::profileManager->groupsTabOrder.first() : 0);
    } else {
        ui->tabWidget->setCurrentIndex(groupId2TabIndex(NekoGui::dataStore->current_group));
        show_group(NekoGui::dataStore->current_group);
    }

    NekoGui::dataStore->refreshing_group_list = false;
    refresh_subscriptions_list();
}

void MainWindow::refresh_subscriptions_list() {
    if (ui->subscriptions_list == nullptr) return;
    ui->subscriptions_list->clear();
    for (const auto &gid: NekoGui::profileManager->groupsTabOrder) {
        auto group = NekoGui::profileManager->GetGroup(gid);
        if (group == nullptr) continue;
        auto item = new QListWidgetItem(ui->subscriptions_list);
        auto widget = new GroupItem(this, group, item);
        ui->subscriptions_list->addItem(item);
        ui->subscriptions_list->setItemWidget(item, widget);
    }
}


void MainWindow::refresh_proxy_list(const int &id) {
    refresh_proxy_list_impl(id, {});
}

void MainWindow::refresh_proxy_list_impl(const int &id, GroupSortAction groupSortAction) {
    // id < 0 means full refresh
    if (id < 0) {
        // clear data
        ui->proxyListTable->row2Id.clear();
        ui->proxyListTable->setRowCount(0);
        // append rows
        int row = -1;
        for (const auto &[id, profile]: NekoGui::profileManager->profiles) {
            if (NekoGui::dataStore->current_group != profile->gid) continue;
            row++;
            ui->proxyListTable->insertRow(row);
            ui->proxyListTable->row2Id += id;
        }
    }

    // apply sort mode
    if (id < 0) {
        switch (groupSortAction.method) {
            case GroupSortMethod::Raw: {
                auto group = NekoGui::profileManager->CurrentGroup();
                if (group == nullptr) return;
                ui->proxyListTable->order = group->order;
                break;
            }
            case GroupSortMethod::ById: {
                // Clear Order
                ui->proxyListTable->order.clear();
                ui->proxyListTable->callback_save_order();
                break;
            }
            case GroupSortMethod::ByAddress:
            case GroupSortMethod::ByName:
            case GroupSortMethod::ByLatency:
            case GroupSortMethod::ByType: {
                std::sort(ui->proxyListTable->order.begin(), ui->proxyListTable->order.end(),
                          [=](int a, int b) {
                              QString ms_a;
                              QString ms_b;
                              if (groupSortAction.method == GroupSortMethod::ByType) {
                                  ms_a = NekoGui::profileManager->GetProfile(a)->bean->DisplayType();
                                  ms_b = NekoGui::profileManager->GetProfile(b)->bean->DisplayType();
                              } else if (groupSortAction.method == GroupSortMethod::ByName) {
                                  ms_a = NekoGui::profileManager->GetProfile(a)->bean->name;
                                  ms_b = NekoGui::profileManager->GetProfile(b)->bean->name;
                              } else if (groupSortAction.method == GroupSortMethod::ByAddress) {
                                  ms_a = NekoGui::profileManager->GetProfile(a)->bean->DisplayAddress();
                                  ms_b = NekoGui::profileManager->GetProfile(b)->bean->DisplayAddress();
                              } else if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                  ms_a = NekoGui::profileManager->GetProfile(a)->full_test_report;
                                  ms_b = NekoGui::profileManager->GetProfile(b)->full_test_report;
                              }
                              auto get_latency_for_sort = [](int id) {
                                  auto i = NekoGui::profileManager->GetProfile(id)->latency;
                                  if (i == 0) i = 100000;
                                  if (i < 0) i = 99999;
                                  return i;
                              };
                              if (groupSortAction.descending) {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) > get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a > ms_b;
                              } else {
                                  if (groupSortAction.method == GroupSortMethod::ByLatency) {
                                      auto int_a = NekoGui::profileManager->GetProfile(a)->latency;
                                      auto int_b = NekoGui::profileManager->GetProfile(b)->latency;
                                      if (ms_a.isEmpty() && ms_b.isEmpty()) {
                                          // compare latency if full_test_report is empty
                                          return get_latency_for_sort(a) < get_latency_for_sort(b);
                                      }
                                  }
                                  return ms_a < ms_b;
                              }
                          });
                break;
            }
        }
        ui->proxyListTable->update_order(groupSortAction.save_sort);
    }

    // refresh data
    refresh_proxy_list_impl_refresh_data(id);
}

void MainWindow::refresh_proxy_list_impl_refresh_data(const int &id) {
    // draw/update table items
    for (int row = 0; row < ui->proxyListTable->rowCount(); row++) {
        auto profileId = ui->proxyListTable->row2Id[row];
        if (id >= 0 && profileId != id) continue; // refresh ONE item
        auto profile = NekoGui::profileManager->GetProfile(profileId);
        if (profile == nullptr) continue;

        auto isRunning = profileId == NekoGui::dataStore->started_id;
        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(114514, profileId);

        // Check state
        auto check = f0->clone();
        check->setText(isRunning ? QString(QChar(0x2713)) : Int2String(row + 1));
        ui->proxyListTable->setVerticalHeaderItem(row, check);

        // C0: Type
        auto f = f0->clone();
        f->setText(profile->bean->DisplayType());
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 0, f);

        // C1: Address+Port
        f = f0->clone();
        f->setText(profile->bean->DisplayAddress());
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 1, f);

        // C2: Name
        f = f0->clone();
        f->setText(profile->bean->name);
        if (isRunning) f->setForeground(palette().link());
        ui->proxyListTable->setItem(row, 2, f);

        // C3: Test Result
        f = f0->clone();
        if (profile->full_test_report.isEmpty()) {
            auto color = profile->DisplayLatencyColor();
            if (color.isValid()) f->setForeground(color);
            f->setText(profile->DisplayLatency());
        } else {
            f->setText(profile->full_test_report);
        }
        ui->proxyListTable->setItem(row, 3, f);

        // C4: Traffic
        f = f0->clone();
        f->setText(profile->traffic_data->DisplayTraffic());
        ui->proxyListTable->setItem(row, 4, f);
    }
}

// table context menu handlers

void MainWindow::on_proxyListTable_itemDoubleClicked(QTableWidgetItem *item) {
    auto id = item->data(114514).toInt();
    if (select_mode) {
        emit profile_selected(id);
        select_mode = false;
        refresh_status();
        return;
    }
    auto dialog = new DialogEditProfile("", id, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_input_triggered() {
    auto dialog = new DialogEditProfile("socks", NekoGui::dataStore->current_group, this);
    connect(dialog, &QDialog::finished, dialog, &QDialog::deleteLater);
}

void MainWindow::on_menu_add_from_clipboard_triggered() {
    if (!can_start_add()) return;
    auto clipboard = QApplication::clipboard()->text().trimmed();
    if (clipboard.isEmpty()) {
        show_toast_error(tr("Clipboard is empty."));
        return;
    }
    add_in_progress = true;
    add_base_count = static_cast<int>(NekoGui::profileManager->profiles.size());
    set_add_controls_enabled(false);
    if (add_debounce_timer != nullptr) add_debounce_timer->start(400);
    NekoGui_sub::groupUpdater->AsyncUpdate(clipboard, -1, [=] {
        runOnUiThread([=] { finish_add_operation(); });
    });
}

void MainWindow::on_menu_clone_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

    auto btn = QMessageBox::question(this, tr("Clone"), tr("Clone %1 item(s)").arg(ents.count()));
    if (btn != QMessageBox::Yes) return;

    QStringList sls;
    for (const auto &ent: ents) {
        sls << ent->bean->ToNekorayShareLink(ent->type);
    }

    NekoGui_sub::groupUpdater->AsyncUpdate(sls.join("\n"));
}

void MainWindow::on_menu_move_triggered() {
    auto ents = get_now_selected_list();
    if (ents.isEmpty()) return;

    auto items = QStringList{};
    for (auto gid: NekoGui::profileManager->groupsTabOrder) {
        auto group = NekoGui::profileManager->GetGroup(gid);
        if (group == nullptr) continue;
        items += Int2String(gid) + " " + group->name;
    }

    bool ok;
    auto a = QInputDialog::getItem(nullptr,
                                   tr("Move"),
                                   tr("Move %1 item(s)").arg(ents.count()),
                                   items, 0, false, &ok);
    if (!ok) return;
    auto gid = SubStrBefore(a, " ").toInt();
    for (const auto &ent: ents) {
        NekoGui::profileManager->MoveProfile(ent, gid);
    }
    refresh_proxy_list();
}

void MainWindow::on_menu_delete_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;

    const QString prompt = ents.count() == 1
        ? tr("Delete selected server?")
        : tr("Delete selected items (%1)?").arg(ents.count());
    QMessageBox msg(QMessageBox::Warning, tr("Confirmation"), prompt, QMessageBox::NoButton, this);
    auto deleteBtn = msg.addButton(tr("Delete"), QMessageBox::AcceptRole);
    msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
    msg.exec();
    if (msg.clickedButton() != deleteBtn) return;

    for (const auto &ent: ents) {
        NekoGui::profileManager->DeleteProfile(ent->id);
    }
    refresh_proxy_list();
    show_toast_success(tr("Deleted: %1").arg(ents.count()));
}

void MainWindow::on_menu_reset_traffic_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() == 0) return;
    for (const auto &ent: ents) {
        ent->traffic_data->Reset();
        ent->Save();
        refresh_proxy_list(ent->id);
    }
}

void MainWindow::on_menu_profile_debug_info_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto btn = QMessageBox::information(this, software_name, ents.first()->ToJsonBytes(), "OK", "Edit", "Reload", 0, 0);
    if (btn == 1) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(QStringLiteral("profiles/%1.json").arg(ents.first()->id)).absoluteFilePath()));
    } else if (btn == 2) {
        NekoGui::dataStore->Load();
        NekoGui::profileManager->LoadManager();
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_copy_links_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->copy();
        return;
    }
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToShareLink();
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_copy_links_nkr_triggered() {
    auto ents = get_now_selected_list();
    QStringList links;
    for (const auto &ent: ents) {
        links += ent->bean->ToNekorayShareLink(ent->type);
    }
    if (links.length() == 0) return;
    QApplication::clipboard()->setText(links.join("\n"));
    show_log_impl(tr("Copied %1 item(s)").arg(links.length()));
}

void MainWindow::on_menu_export_config_triggered() {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;
    auto ent = ents.first();
    if (ent->bean->DisplayCoreType() != software_core_name) return;

    auto result = BuildConfig(ent, false, true);
    QString config_core = QJsonObject2QString(result->coreConfig, false);
    QApplication::clipboard()->setText(config_core);

    QMessageBox msg(QMessageBox::Information, tr("Config copied"), tr("Config copied"));
    msg.addButton("Copy core config", QMessageBox::YesRole);
    msg.addButton("Copy test config", QMessageBox::NoRole);
    msg.addButton(QMessageBox::Ok);
    msg.setEscapeButton(QMessageBox::Ok);
    msg.setDefaultButton(QMessageBox::Ok);
    auto ret = msg.exec();
    if (ret == 2) {
        result = BuildConfig(ent, false, false);
        config_core = QJsonObject2QString(result->coreConfig, false);
        QApplication::clipboard()->setText(config_core);
    } else if (ret == 3) {
        result = BuildConfig(ent, true, false);
        config_core = QJsonObject2QString(result->coreConfig, false);
        QApplication::clipboard()->setText(config_core);
    }
}

void MainWindow::display_qr_link(bool nkrFormat) {
    auto ents = get_now_selected_list();
    if (ents.count() != 1) return;

    class W : public QDialog {
    public:
        QLabel *l = nullptr;
        QCheckBox *cb = nullptr;
        //
        QPlainTextEdit *l2 = nullptr;
        QImage im;
        //
        QString link;
        QString link_nk;

        void show_qr(const QSize &size) const {
            auto side = size.height() - 20 - l2->size().height() - cb->size().height();
            l->setPixmap(QPixmap::fromImage(im.scaled(side, side, Qt::KeepAspectRatio, Qt::FastTransformation),
                                            Qt::MonoOnly));
            l->resize(side, side);
        }

        void refresh(bool is_nk) {
            auto link_display = is_nk ? link_nk : link;
            l2->setPlainText(link_display);
            constexpr qint32 qr_padding = 2;
            //
            try {
                qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(link_display.toUtf8().data(), qrcodegen::QrCode::Ecc::MEDIUM);
                qint32 sz = qr.getSize();
                im = QImage(sz + qr_padding * 2, sz + qr_padding * 2, QImage::Format_RGB32);
                QRgb black = qRgb(0, 0, 0);
                QRgb white = qRgb(255, 255, 255);
                im.fill(white);
                for (int y = 0; y < sz; y++)
                    for (int x = 0; x < sz; x++)
                        if (qr.getModule(x, y))
                            im.setPixel(x + qr_padding, y + qr_padding, black);
                show_qr(size());
            } catch (const std::exception &ex) {
                QMessageBox::warning(nullptr, "error", ex.what());
            }
        }

        W(const QString &link_, const QString &link_nk_) {
            link = link_;
            link_nk = link_nk_;
            //
            setLayout(new QVBoxLayout);
            setMinimumSize(256, 256);
            QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            sizePolicy.setHeightForWidth(true);
            setSizePolicy(sizePolicy);
            //
            l = new QLabel();
            l->setMinimumSize(256, 256);
            l->setMargin(6);
            l->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            l->setScaledContents(true);
            layout()->addWidget(l);
            cb = new QCheckBox;
            cb->setText("CofeBox Links");
            layout()->addWidget(cb);
            l2 = new QPlainTextEdit();
            l2->setReadOnly(true);
            layout()->addWidget(l2);
            //
            connect(cb, &QCheckBox::toggled, this, &W::refresh);
            refresh(false);
        }

        void resizeEvent(QResizeEvent *resizeEvent) override {
            show_qr(resizeEvent->size());
        }
    };

    auto link = ents.first()->bean->ToShareLink();
    auto link_nk = ents.first()->bean->ToNekorayShareLink(ents.first()->type);
    auto w = new W(link, link_nk);
    w->setWindowTitle(ents.first()->bean->DisplayTypeAndName());
    w->exec();
    w->deleteLater();
}

void MainWindow::on_menu_scan_qr_triggered() {
#ifndef NKR_NO_ZXING
    using namespace ZXingQt;

    hide();
    QThread::sleep(1);

    auto screen = QGuiApplication::primaryScreen();
    auto geom = screen->geometry();
    auto qpx = screen->grabWindow(0, geom.x(), geom.y(), geom.width(), geom.height());

    show();

    auto hints = DecodeHints()
                     .setFormats(BarcodeFormat::QRCode)
                     .setTryRotate(false)
                     .setBinarizer(Binarizer::FixedThreshold);

    auto result = ReadBarcode(qpx.toImage(), hints);
    const auto &text = result.text();
    if (text.isEmpty()) {
        MessageBoxInfo(software_name, tr("QR Code not found"));
    } else {
        show_log_impl("QR Code Result:\n" + text);
        NekoGui_sub::groupUpdater->AsyncUpdate(text);
    }
#endif
}

void MainWindow::on_menu_clear_test_result_triggered() {
    for (const auto &profile: get_selected_or_group()) {
        profile->latency = 0;
        profile->full_test_report = "";
        profile->Save();
    }
    refresh_proxy_list();
}

void MainWindow::on_menu_select_all_triggered() {
    if (ui->masterLogBrowser->hasFocus()) {
        ui->masterLogBrowser->selectAll();
        return;
    }
    ui->proxyListTable->selectAll();
}

void MainWindow::on_menu_delete_repeat_triggered() {
    QList<std::shared_ptr<NekoGui::ProxyEntity>> out;
    QList<std::shared_ptr<NekoGui::ProxyEntity>> out_del;

    NekoGui::ProfileFilter::Uniq(NekoGui::profileManager->CurrentGroup()->Profiles(), out, true, false);
    NekoGui::ProfileFilter::OnlyInSrc_ByPointer(NekoGui::profileManager->CurrentGroup()->Profiles(), out, out_del);

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (out_del.length() > 0 &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            NekoGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

bool mw_sub_updating = false;

void MainWindow::on_menu_update_subscription_triggered() {
    auto group = NekoGui::profileManager->CurrentGroup();
    if (group->url.isEmpty()) return;
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    NekoGui_sub::groupUpdater->AsyncUpdate(group->url, group->id, [&] { mw_sub_updating = false; });
}

void MainWindow::on_menu_remove_unavailable_triggered() {
    QList<std::shared_ptr<NekoGui::ProxyEntity>> out_del;

    for (const auto &[_, profile]: NekoGui::profileManager->profiles) {
        if (NekoGui::dataStore->current_group != profile->gid) continue;
        if (profile->latency < 0) out_del += profile;
    }

    int remove_display_count = 0;
    QString remove_display;
    for (const auto &ent: out_del) {
        remove_display += ent->bean->DisplayTypeAndName() + "\n";
        if (++remove_display_count == 20) {
            remove_display += "...";
            break;
        }
    }

    if (out_del.length() > 0 &&
        QMessageBox::question(this, tr("Confirmation"), tr("Remove %1 item(s) ?").arg(out_del.length()) + "\n" + remove_display) == QMessageBox::StandardButton::Yes) {
        for (const auto &ent: out_del) {
            NekoGui::profileManager->DeleteProfile(ent->id);
        }
        refresh_proxy_list();
    }
}

void MainWindow::on_menu_resolve_domain_triggered() {
    auto profiles = get_selected_or_group();
    if (profiles.isEmpty()) return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Resolving domain to IP, if support.")) != QMessageBox::StandardButton::Yes) {
        return;
    }
    if (mw_sub_updating) return;
    mw_sub_updating = true;
    NekoGui::dataStore->resolve_count = profiles.count();

    for (const auto &profile: profiles) {
        profile->bean->ResolveDomainToIP([=] {
            profile->Save();
            if (--NekoGui::dataStore->resolve_count != 0) return;
            refresh_proxy_list();
            mw_sub_updating = false;
        });
    }
}

void MainWindow::on_proxyListTable_customContextMenuRequested(const QPoint &pos) {
    ui->menu_server->popup(ui->proxyListTable->viewport()->mapToGlobal(pos)); // show menu
}

QList<std::shared_ptr<NekoGui::ProxyEntity>> MainWindow::get_now_selected_list() {
    auto items = ui->proxyListTable->selectedItems();
    QList<std::shared_ptr<NekoGui::ProxyEntity>> list;
    for (auto item: items) {
        auto id = item->data(114514).toInt();
        auto ent = NekoGui::profileManager->GetProfile(id);
        if (ent != nullptr && !list.contains(ent)) list += ent;
    }
    return list;
}

QList<std::shared_ptr<NekoGui::ProxyEntity>> MainWindow::get_selected_or_group() {
    auto selected_or_group = ui->menu_server->property("selected_or_group").toInt();
    QList<std::shared_ptr<NekoGui::ProxyEntity>> profiles;
    if (selected_or_group > 0) {
        profiles = get_now_selected_list();
        if (profiles.isEmpty() && selected_or_group == 2) profiles = NekoGui::profileManager->CurrentGroup()->ProfilesWithOrder();
    } else {
        profiles = NekoGui::profileManager->CurrentGroup()->ProfilesWithOrder();
    }
    return profiles;
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    switch (event->key()) {
        case Qt::Key_Escape:
            // take over by shortcut_esc
            break;
        case Qt::Key_Enter:
            startProxy();
            break;
        default:
            QMainWindow::keyPressEvent(event);
    }
}

// Log

inline void FastAppendTextDocument(const QString &message, QTextDocument *doc) {
    QTextCursor cursor(doc);
    cursor.movePosition(QTextCursor::End);
    cursor.beginEditBlock();
    cursor.insertBlock();
    cursor.insertText(message);
    cursor.endEditBlock();
}

void MainWindow::show_log_impl(const QString &log) {
    auto lines = SplitLines(log.trimmed());
    if (lines.isEmpty()) return;

    QStringList newLines;
    auto log_ignore = NekoGui::dataStore->log_ignore;
    for (const auto &line: lines) {
        bool showThisLine = true;
        for (const auto &str: log_ignore) {
            if (line.contains(str)) {
                showThisLine = false;
                break;
            }
        }
        if (showThisLine) newLines << line;
    }
    if (newLines.isEmpty()) return;

    FastAppendTextDocument(newLines.join("\n"), qvLogDocument);
    // qvLogDocument->setPlainText(qvLogDocument->toPlainText() + log);
    // From https://gist.github.com/jemyzhang/7130092
    auto block = qvLogDocument->begin();

    while (block.isValid()) {
        if (qvLogDocument->blockCount() > NekoGui::dataStore->max_log_line) {
            QTextCursor cursor(block);
            block = block.next();
            cursor.select(QTextCursor::BlockUnderCursor);
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor);
            cursor.removeSelectedText();
            continue;
        }
        break;
    }
}

#define ADD_TO_CURRENT_ROUTE(a, b)                                                                   \
    NekoGui::dataStore->routing->a = (SplitLines(NekoGui::dataStore->routing->a) << (b)).join("\n"); \
    NekoGui::dataStore->routing->Save();

void MainWindow::on_masterLogBrowser_customContextMenuRequested(const QPoint &pos) {
    QMenu *menu = ui->masterLogBrowser->createStandardContextMenu();

    auto sep = new QAction(this);
    sep->setSeparator(true);
    menu->addAction(sep);

    auto action_add_ignore = new QAction(this);
    action_add_ignore->setText(tr("Set ignore keyword"));
    connect(action_add_ignore, &QAction::triggered, this, [=] {
        auto list = NekoGui::dataStore->log_ignore;
        auto newStr = ui->masterLogBrowser->textCursor().selectedText().trimmed();
        if (!newStr.isEmpty()) list << newStr;
        bool ok;
        newStr = QInputDialog::getMultiLineText(GetMessageBoxParent(), tr("Set ignore keyword"), tr("Set the following keywords to ignore?\nSplit by line."), list.join("\n"), &ok);
        if (ok) {
            NekoGui::dataStore->log_ignore = SplitLines(newStr);
            NekoGui::dataStore->Save();
        }
    });
    menu->addAction(action_add_ignore);

    auto action_add_route = new QAction(this);
    action_add_route->setText(tr("Save as route"));
    connect(action_add_route, &QAction::triggered, this, [=] {
        auto newStr = ui->masterLogBrowser->textCursor().selectedText().trimmed();
        if (newStr.isEmpty()) return;
        //
        bool ok;
        newStr = QInputDialog::getText(GetMessageBoxParent(), tr("Save as route"), tr("Edit"), {}, newStr, &ok).trimmed();
        if (!ok) return;
        if (newStr.isEmpty()) return;
        //
        auto select = IsIpAddress(newStr) ? 0 : 3;
        QStringList items = {"proxyIP", "bypassIP", "blockIP", "proxyDomain", "bypassDomain", "blockDomain"};
        auto item = QInputDialog::getItem(GetMessageBoxParent(), tr("Save as route"),
                                          tr("Save \"%1\" as a routing rule?").arg(newStr),
                                          items, select, false, &ok);
        if (ok) {
            auto index = items.indexOf(item);
            switch (index) {
                case 0:
                    ADD_TO_CURRENT_ROUTE(proxy_ip, newStr);
                    break;
                case 1:
                    ADD_TO_CURRENT_ROUTE(direct_ip, newStr);
                    break;
                case 2:
                    ADD_TO_CURRENT_ROUTE(block_ip, newStr);
                    break;
                case 3:
                    ADD_TO_CURRENT_ROUTE(proxy_domain, newStr);
                    break;
                case 4:
                    ADD_TO_CURRENT_ROUTE(direct_domain, newStr);
                    break;
                case 5:
                    ADD_TO_CURRENT_ROUTE(block_domain, newStr);
                    break;
                default:
                    break;
            }
            MW_dialog_message("", "UpdateDataStore,RouteChanged");
        }
    });
    menu->addAction(action_add_route);

    auto action_clear = new QAction(this);
    action_clear->setText(tr("Clear"));
    connect(action_clear, &QAction::triggered, this, [=] {
        qvLogDocument->clear();
        ui->masterLogBrowser->clear();
    });
    menu->addAction(action_clear);

    menu->exec(ui->masterLogBrowser->viewport()->mapToGlobal(pos)); // show menu
}

// eventFilter

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (event->type() == QEvent::KeyPress && obj == ui->home_sub_url) {
        auto keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent != nullptr && keyEvent->matches(QKeySequence::Paste)) {
            if (ui->home_sub_url->text().trimmed().isEmpty()) {
                auto clipboardText = QApplication::clipboard()->text().trimmed();
                if (!clipboardText.isEmpty()) {
                    ui->home_sub_url->setText(clipboardText);
                    submit_home_subscription();
                    return true;
                }
            }
        }
    }
    if (event->type() == QEvent::KeyPress && obj == ui->proxyListTable) {
        auto keyEvent = dynamic_cast<QKeyEvent *>(event);
        if (keyEvent != nullptr && keyEvent->key() == Qt::Key_Delete) {
            on_menu_delete_triggered();
            return true;
        }
    }
    if (event->type() == QEvent::MouseButtonPress) {
        auto mouseEvent = dynamic_cast<QMouseEvent *>(event);
        if (obj == drawer_scrim && mouseEvent->button() == Qt::LeftButton) {
            set_drawer_open(false);
            return true;
        }
        if (obj == ui->label_running && mouseEvent->button() == Qt::LeftButton && running != nullptr) {
            speedtest_current();
            return true;
        } else if (obj == ui->label_inbound && mouseEvent->button() == Qt::LeftButton) {
            on_menu_basic_settings_triggered();
            return true;
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// profile selector

void MainWindow::start_select_mode(QObject *context, const std::function<void(int)> &callback) {
    select_mode = true;
    connectOnce(this, &MainWindow::profile_selected, context, callback);
    refresh_status();
}

// connection list

inline QJsonArray last_arr; // format is connection statistics json

void MainWindow::refresh_connection_list(const QJsonArray &arr) {
    if (last_arr == arr) {
        return;
    }
    last_arr = arr;

    if (NekoGui::dataStore->flag_debug) qDebug() << arr;

    ui->tableWidget_conn->setRowCount(0);

    int row = -1;
    for (const auto &_item: arr) {
        auto item = _item.toObject();
        if (NekoGui::dataStore->ignoreConnTag.contains(item["Tag"].toString())) continue;

        row++;
        ui->tableWidget_conn->insertRow(row);

        auto f0 = std::make_unique<QTableWidgetItem>();
        f0->setData(114514, item["ID"].toInt());

        // C0: Status
        auto c0 = new QLabel;
        auto start_t = item["Start"].toInt();
        auto end_t = item["End"].toInt();
        // icon
        auto outboundTag = item["Tag"].toString();
        if (outboundTag == "block") {
            c0->setPixmap(Icon::GetMaterialIcon("cancel"));
        } else {
            if (end_t > 0) {
                c0->setPixmap(Icon::GetMaterialIcon("history"));
            } else {
                c0->setPixmap(Icon::GetMaterialIcon("swap-vertical"));
            }
        }
        c0->setAlignment(Qt::AlignCenter);
        c0->setToolTip(tr("Start: %1\nEnd: %2").arg(DisplayTime(start_t), end_t > 0 ? DisplayTime(end_t) : ""));
        ui->tableWidget_conn->setCellWidget(row, 0, c0);

        // C1: Outbound
        auto f = f0->clone();
        f->setToolTip("");
        f->setText(outboundTag);
        ui->tableWidget_conn->setItem(row, 1, f);

        // C2: Destination
        f = f0->clone();
        QString target1 = item["Dest"].toString();
        QString target2 = item["RDest"].toString();
        if (target2.isEmpty() || target1 == target2) {
            target2 = "";
        }
        f->setText("[" + target1 + "] " + target2);
        ui->tableWidget_conn->setItem(row, 2, f);
    }
}

// Hotkey

#ifndef NKR_NO_QHOTKEY

#include <QHotkey>

inline QList<std::shared_ptr<QHotkey>> RegisteredHotkey;

void MainWindow::RegisterHotkey(bool unregister) {
    while (!RegisteredHotkey.isEmpty()) {
        auto hk = RegisteredHotkey.takeFirst();
        hk->deleteLater();
    }
    if (unregister) return;

    QStringList regstr{
        NekoGui::dataStore->hotkey_mainwindow,
        NekoGui::dataStore->hotkey_group,
        NekoGui::dataStore->hotkey_route,
        NekoGui::dataStore->hotkey_system_proxy_menu,
    };

    for (const auto &key: regstr) {
        if (key.isEmpty()) continue;
        if (regstr.count(key) > 1) return; // Conflict hotkey
    }
    for (const auto &key: regstr) {
        QKeySequence k(key);
        if (k.isEmpty()) continue;
        auto hk = std::make_shared<QHotkey>(k, true);
        if (hk->isRegistered()) {
            RegisteredHotkey += hk;
            connect(hk.get(), &QHotkey::activated, this, [=] { HotkeyEvent(key); });
        } else {
            hk->deleteLater();
        }
    }
}

void MainWindow::HotkeyEvent(const QString &key) {
    if (key.isEmpty()) return;
    runOnUiThread([=] {
        if (key == NekoGui::dataStore->hotkey_mainwindow) {
            tray->activated(QSystemTrayIcon::ActivationReason::Trigger);
        } else if (key == NekoGui::dataStore->hotkey_group) {
            on_menu_manage_groups_triggered();
        } else if (key == NekoGui::dataStore->hotkey_route) {
            on_menu_routing_settings_triggered();
        } else if (key == NekoGui::dataStore->hotkey_system_proxy_menu) {
            ui->menu_spmode->popup(QCursor::pos());
        }
    });
}

#else

void MainWindow::RegisterHotkey(bool unregister) {}

void MainWindow::HotkeyEvent(const QString &key) {}

#endif

// VPN Launcher

bool MainWindow::StartVPNProcess() {
    //
    if (vpn_pid != 0) {
        return true;
    }
    //
    auto configPath = NekoGui::WriteVPNSingBoxConfig();
    auto scriptPath = NekoGui::WriteVPNLinuxScript();
    auto corePath = QDir(QApplication::applicationDirPath()).absoluteFilePath("cofebox_core");
    if (!QFileInfo::exists(corePath)) {
        MessageBoxWarning(tr("Error"), tr("Core binary not found for TUN mode."));
        return false;
    }
#ifdef Q_OS_LINUX
    if (!QFileInfo::exists("/dev/net/tun")) {
        MessageBoxWarning(tr("Error"), tr("TUN device is unavailable: /dev/net/tun"));
        return false;
    }
    const auto expectedScriptHash = Sha256Hex(ReadFile(":/cofebox/vpn/vpn-run-root.sh"));
    const auto actualScriptHash = Sha256FileHex(scriptPath);
    if (expectedScriptHash.isEmpty() || actualScriptHash.isEmpty() || expectedScriptHash != actualScriptHash) {
        MessageBoxWarning(tr("Error"),
                          tr("VPN root script integrity check failed.\nExpected: %1\nActual: %2")
                              .arg(expectedScriptHash, actualScriptHash));
        return false;
    }
#endif
    //
#ifdef Q_OS_WIN
    runOnNewThread([=] {
        vpn_pid = 1; // TODO get pid?
        WinCommander::runProcessElevated(QApplication::applicationDirPath() + "/cofebox_core.exe",
                                         {"--disable-color", "run", "-c", configPath}, "",
                                         NekoGui::dataStore->vpn_hide_console ? WinCommander::SW_HIDE : WinCommander::SW_SHOWMINIMIZED); // blocking
        vpn_pid = 0;
        runOnUiThread([=] { setVpnMode(false); });
    });
#else
    //
    auto vpn_process = new QProcess;
    QProcess::connect(vpn_process, &QProcess::stateChanged, this, [=](QProcess::ProcessState state) {
        if (state == QProcess::NotRunning) {
            vpn_pid = 0;
            vpn_process->deleteLater();
            GetMainWindow()->setVpnMode(false);
        }
    });
    //
    vpn_process->setProcessChannelMode(QProcess::ForwardedChannels);
#ifdef Q_OS_MACOS
    auto shellQuote = [](QString v) {
        return QStringLiteral("'") + v.replace("'", "'\\''") + QStringLiteral("'");
    };
    const auto cmd = QStringLiteral("/bin/bash --noprofile --norc %1 %2 %3")
                         .arg(shellQuote(scriptPath), shellQuote(corePath), shellQuote(configPath));
    vpn_process->start("osascript", {"-e", QStringLiteral("do shell script \"%1\" with administrator privileges")
                                               .arg(cmd)});
#else
    QProcessEnvironment cleanEnv;
    cleanEnv.insert("PATH", "/usr/sbin:/usr/bin:/sbin:/bin");
    for (const auto &key : {QStringLiteral("DISPLAY"),
                            QStringLiteral("XAUTHORITY"),
                            QStringLiteral("WAYLAND_DISPLAY"),
                            QStringLiteral("XDG_RUNTIME_DIR"),
                            QStringLiteral("DBUS_SESSION_BUS_ADDRESS"),
                            QStringLiteral("LANG"),
                            QStringLiteral("LC_ALL"),
                            QStringLiteral("LC_CTYPE")}) {
        if (qEnvironmentVariableIsSet(key.toUtf8().constData())) {
            cleanEnv.insert(key, qEnvironmentVariable(key.toUtf8().constData()));
        }
    }
    vpn_process->setProcessEnvironment(cleanEnv);
    vpn_process->start("pkexec", {"/bin/bash", "--noprofile", "--norc", scriptPath, corePath, configPath});
#endif
    vpn_process->waitForStarted();
    if (vpn_process->state() == QProcess::NotRunning) {
        MessageBoxWarning(tr("Error"), tr("Failed to start privileged TUN helper."));
        vpn_process->deleteLater();
        return false;
    }
    vpn_pid = vpn_process->processId(); // actually it's pkexec or bash PID
#endif
    return true;
}

bool MainWindow::StopVPNProcess(bool unconditional) {
    if (unconditional || vpn_pid != 0) {
        bool ok;
        core_process->processId();
#ifdef Q_OS_WIN
        auto ret = WinCommander::runProcessElevated("taskkill", {"/IM", "cofebox_core.exe",
                                                                 "/FI",
                                                                 "PID ne " + Int2String(core_process->processId())});
        ok = ret == 0;
#else
        QProcess p;
#ifdef Q_OS_MACOS
        p.start("osascript", {"-e", QStringLiteral("do shell script \"%1\" with administrator privileges")
                                        .arg("pkill -2 -U 0 cofebox_core")});
#else
        if (unconditional) {
            p.start("pkexec", {"killall", "-2", "cofebox_core"});
        } else {
            p.start("pkexec", {"pkill", "-2", "-P", Int2String(vpn_pid)});
        }
#endif
        p.waitForFinished();
        ok = p.exitCode() == 0;
#endif
        if (!unconditional) {
            ok ? vpn_pid = 0 : MessageBoxWarning(tr("Error"), tr("Failed to stop Tun process"));
        }
        return ok;
    }
    return true;
}


