#include "ui/dialog_app_routing.h"

#include "main/GuiUtils.hpp"
#include "main/NekoGui.hpp"

#include <QApplication>
#include <QDir>
#include <QButtonGroup>
#include <QClipboard>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QShortcut>
#include <QSet>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#ifdef Q_OS_LINUX
#include <QFile>
#endif

namespace {

enum RuleDataRole {
    RoleSourceIndex = Qt::UserRole + 1
};

struct RunningProcessRow {
    QString displayName;
    QString processName;
    QString processPath;
    QIcon icon;
};

QString normalizePath(QString path) {
    path = path.trimmed().replace("\\", "/");
    while (path.contains("//")) {
        path.replace("//", "/");
    }
    return path;
}

QString entryKey(const AppRoutingRules::Entry &entry) {
    auto value = entry.value.trimmed();
    if (entry.matchType == AppRoutingRules::MatchType::ProcessPath) {
        value = normalizePath(value);
    }
#ifdef Q_OS_WIN
    value = value.toLower();
#endif
    return AppRoutingRules::MatchTypeKey(entry.matchType) + ":" + value;
}

QString modeKey(DialogAppRouting::Mode mode) {
    if (mode == DialogAppRouting::Mode::OnlySelected) return "allowlist";
    if (mode == DialogAppRouting::Mode::ExceptSelected) return "blocklist";
    return "all";
}

DialogAppRouting::Mode modeFromKey(const QString &value) {
    const auto key = value.trimmed().toLower();
    if (key == "allowlist" || key == "only_selected") return DialogAppRouting::Mode::OnlySelected;
    if (key == "blocklist" || key == "except_selected") return DialogAppRouting::Mode::ExceptSelected;
    return DialogAppRouting::Mode::AllThroughProxy;
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
constexpr auto kSkipEmptyParts = Qt::SkipEmptyParts;
#else
constexpr auto kSkipEmptyParts = QString::SkipEmptyParts;
#endif

QList<RunningProcessRow> queryRunningProcesses(QString *errorText) {
    QList<RunningProcessRow> rows;
    QSet<QString> dedupe;
    QFileIconProvider iconProvider;

#ifdef Q_OS_WIN
    QProcess process;
    process.start(
        "powershell",
        {"-NoProfile",
         "-ExecutionPolicy",
         "Bypass",
         "-Command",
         "[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; $ErrorActionPreference='SilentlyContinue'; Get-CimInstance Win32_Process | ForEach-Object { '{0}|{1}' -f $_.Name, $_.ExecutablePath }"});
    if (!process.waitForFinished(6000)) {
        process.kill();
        if (errorText != nullptr) *errorText = QObject::tr("Failed to enumerate running processes.");
        return rows;
    }
    if (process.exitStatus() != QProcess::NormalExit) {
        if (errorText != nullptr) *errorText = QObject::tr("Failed to enumerate running processes.");
        return rows;
    }
    const auto text = QString::fromUtf8(process.readAllStandardOutput());
    for (const auto &lineRaw : text.split(QRegularExpression("[\r\n]"), kSkipEmptyParts)) {
        const auto line = lineRaw.trimmed();
        const int delim = line.indexOf('|');
        if (delim <= 0) continue;
        RunningProcessRow row;
        row.processName = line.left(delim).trimmed();
        row.processPath = normalizePath(line.mid(delim + 1).trimmed());
        if (row.processName.isEmpty()) continue;
        row.displayName = QFileInfo(row.processName).completeBaseName();
        if (row.displayName.isEmpty()) row.displayName = row.processName;
        const auto key = (row.processName + "|" + row.processPath).toLower();
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        if (!row.processPath.isEmpty() && QFileInfo::exists(row.processPath)) {
            row.icon = iconProvider.icon(QFileInfo(row.processPath));
        } else {
            row.icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        rows << row;
    }
#elif defined(Q_OS_LINUX)
    QDir proc("/proc");
    for (const auto &pid : proc.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        bool ok = false;
        pid.toInt(&ok);
        if (!ok) continue;
        RunningProcessRow row;
        QFile commFile(QStringLiteral("/proc/%1/comm").arg(pid));
        if (commFile.open(QIODevice::ReadOnly)) {
            const auto raw = commFile.readAll();
            row.processName = QString::fromUtf8(raw).trimmed();
            if (row.processName.contains(QChar::ReplacementCharacter)) {
                row.processName = QString::fromLocal8Bit(raw).trimmed();
            }
            commFile.close();
        }
        QFileInfo exeInfo(QStringLiteral("/proc/%1/exe").arg(pid));
        row.processPath = normalizePath(exeInfo.symLinkTarget());
        if (row.processName.isEmpty() && !row.processPath.isEmpty()) {
            row.processName = QFileInfo(row.processPath).fileName();
        }
        if (row.processName.isEmpty()) continue;
        row.displayName = QFileInfo(row.processName).completeBaseName();
        if (row.displayName.isEmpty()) row.displayName = row.processName;
        const auto key = row.processName + "|" + row.processPath;
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        if (!row.processPath.isEmpty() && QFileInfo::exists(row.processPath)) {
            row.icon = iconProvider.icon(QFileInfo(row.processPath));
        } else {
            row.icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        rows << row;
    }
#else
    QProcess process;
    process.start("ps", {"-axo", "comm=,args="});
    if (!process.waitForFinished(4000) || process.exitStatus() != QProcess::NormalExit) {
        if (errorText != nullptr) *errorText = QObject::tr("Failed to enumerate running processes.");
        return rows;
    }
    const auto text = QString::fromUtf8(process.readAllStandardOutput());
    for (const auto &lineRaw : text.split(QRegularExpression("[\r\n]"), kSkipEmptyParts)) {
        const auto line = lineRaw.trimmed();
        if (line.isEmpty()) continue;
        const auto parts = line.split(QRegularExpression("\\s+"), kSkipEmptyParts);
        if (parts.isEmpty()) continue;
        RunningProcessRow row;
        row.processName = parts.first();
        if (parts.size() >= 2) {
            const auto firstArg = parts.at(1);
            if (firstArg.startsWith("/")) row.processPath = normalizePath(firstArg);
        }
        row.displayName = QFileInfo(row.processName).completeBaseName();
        if (row.displayName.isEmpty()) row.displayName = row.processName;
        const auto key = row.processName + "|" + row.processPath;
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        row.icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
        rows << row;
    }
#endif

    return rows.mid(0, 2000);
}

class AddProcessDialog final : public QDialog {
public:
    Q_DECLARE_TR_FUNCTIONS(AddProcessDialog)
public:

    explicit AddProcessDialog(QWidget *parent = nullptr);
    QList<AppRoutingRules::Entry> selectedEntries() const;

private:
    QTabWidget *tabs_ = nullptr;
    QLineEdit *runningSearch_ = nullptr;
    QComboBox *runningMatchMode_ = nullptr;
    QTableWidget *runningTable_ = nullptr;
    QLineEdit *filePathEdit_ = nullptr;
    QLineEdit *fileDisplayEdit_ = nullptr;
    QComboBox *fileMatchMode_ = nullptr;
    QPlainTextEdit *clipboardPreview_ = nullptr;
    QComboBox *clipboardMatchMode_ = nullptr;
    QPushButton *addButton_ = nullptr;
    QList<RunningProcessRow> runningRows_;
    QList<AppRoutingRules::Entry> selectedEntries_;

    void reloadRunningProcesses();
    void refreshRunningTable();
    AppRoutingRules::Entry makeEntryFromRow(const RunningProcessRow &row, bool matchByPath) const;
    void addFromCurrentTab();
    void addFromRunning();
    void addFromFile();
    void addFromClipboard();
};

AddProcessDialog::AddProcessDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Add application"));
    resize(860, 560);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(10);

    tabs_ = new QTabWidget(this);
    rootLayout->addWidget(tabs_);

    auto *runningTab = new QWidget(this);
    auto *runningLayout = new QVBoxLayout(runningTab);
    runningLayout->setContentsMargins(0, 0, 0, 0);
    runningLayout->setSpacing(8);

    auto *runningToolbar = new QHBoxLayout;
    runningSearch_ = new QLineEdit(runningTab);
    runningSearch_->setPlaceholderText(tr("Search by name or path"));
    runningMatchMode_ = new QComboBox(runningTab);
    runningMatchMode_->addItem(tr("Match by process name"), "name");
    runningMatchMode_->addItem(tr("Match by full path"), "path");
    auto *refreshButton = new QPushButton(tr("Refresh"), runningTab);
    runningToolbar->addWidget(runningSearch_, 1);
    runningToolbar->addWidget(runningMatchMode_, 0);
    runningToolbar->addWidget(refreshButton, 0);
    runningLayout->addLayout(runningToolbar);

    runningTable_ = new QTableWidget(runningTab);
    runningTable_->setColumnCount(4);
    runningTable_->setHorizontalHeaderLabels(
        {tr("Application"), tr("Process"), tr("Path"), tr("Match")});
    runningTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    runningTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    runningTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    runningTable_->setAlternatingRowColors(true);
    runningTable_->setSortingEnabled(true);
    runningTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    runningTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    runningTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    runningTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    runningLayout->addWidget(runningTable_, 1);
    tabs_->addTab(runningTab, tr("Running"));

    auto *fileTab = new QWidget(this);
    auto *fileLayout = new QVBoxLayout(fileTab);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->setSpacing(8);
    auto *filePathRow = new QHBoxLayout;
    filePathEdit_ = new QLineEdit(fileTab);
    filePathEdit_->setPlaceholderText(tr("Path to executable"));
    auto *browseButton = new QPushButton(tr("Browse..."), fileTab);
    filePathRow->addWidget(filePathEdit_, 1);
    filePathRow->addWidget(browseButton, 0);
    fileLayout->addLayout(filePathRow);
    fileDisplayEdit_ = new QLineEdit(fileTab);
    fileDisplayEdit_->setPlaceholderText(tr("Display name (optional)"));
    fileLayout->addWidget(fileDisplayEdit_);
    fileMatchMode_ = new QComboBox(fileTab);
    fileMatchMode_->addItem(tr("Match by full path"), "path");
    fileMatchMode_->addItem(tr("Match by process name"), "name");
    fileLayout->addWidget(fileMatchMode_);
    fileLayout->addStretch(1);
    tabs_->addTab(fileTab, tr("Select file"));

    auto *clipboardTab = new QWidget(this);
    auto *clipboardLayout = new QVBoxLayout(clipboardTab);
    clipboardLayout->setContentsMargins(0, 0, 0, 0);
    clipboardLayout->setSpacing(8);
    clipboardPreview_ = new QPlainTextEdit(clipboardTab);
    clipboardPreview_->setReadOnly(true);
    clipboardPreview_->setPlaceholderText(tr("Clipboard is empty"));
    clipboardPreview_->setMaximumBlockCount(2);
    clipboardLayout->addWidget(clipboardPreview_);
    clipboardMatchMode_ = new QComboBox(clipboardTab);
    clipboardMatchMode_->addItem(tr("Match by full path"), "path");
    clipboardMatchMode_->addItem(tr("Match by process name"), "name");
    clipboardLayout->addWidget(clipboardMatchMode_);
    auto *pasteButton = new QPushButton(tr("Use clipboard path"), clipboardTab);
    clipboardLayout->addWidget(pasteButton, 0, Qt::AlignLeft);
    clipboardLayout->addStretch(1);
    tabs_->addTab(clipboardTab, tr("From clipboard"));

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    addButton_ = buttonBox->addButton(tr("Add"), QDialogButtonBox::AcceptRole);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addButton_, &QPushButton::clicked, this, [this] { addFromCurrentTab(); });
    connect(refreshButton, &QPushButton::clicked, this, [this] { reloadRunningProcesses(); });
    connect(runningSearch_, &QLineEdit::textChanged, this, [this] { refreshRunningTable(); });
    connect(runningTable_, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem *) {
        if (tabs_->currentIndex() == 0) addFromCurrentTab();
    });
    connect(browseButton, &QPushButton::clicked, this, [this] {
#ifdef Q_OS_WIN
        const auto file = QFileDialog::getOpenFileName(this, tr("Select executable"), {}, tr("Executables (*.exe);;All files (*)"));
#else
        const auto file = QFileDialog::getOpenFileName(this, tr("Select executable"));
#endif
        if (file.isEmpty()) return;
        filePathEdit_->setText(QDir::toNativeSeparators(file));
        if (fileDisplayEdit_->text().trimmed().isEmpty()) {
            fileDisplayEdit_->setText(QFileInfo(file).completeBaseName());
        }
    });
    connect(pasteButton, &QPushButton::clicked, this, [this] {
        auto text = QApplication::clipboard()->text().trimmed();
        if (text.isEmpty()) return;
        text = text.split(QRegularExpression("[\r\n]"), kSkipEmptyParts).value(0).trimmed();
        if (text.startsWith("\"") && text.endsWith("\"") && text.size() >= 2) {
            text = text.mid(1, text.size() - 2);
        }
        clipboardPreview_->setPlainText(text);
    });

    auto clipText = QApplication::clipboard()->text().trimmed();
    if (!clipText.isEmpty()) {
        clipText = clipText.split(QRegularExpression("[\r\n]"), kSkipEmptyParts).value(0).trimmed();
        clipboardPreview_->setPlainText(clipText);
    }

    reloadRunningProcesses();
}

QList<AppRoutingRules::Entry> AddProcessDialog::selectedEntries() const {
    return selectedEntries_;
}

void AddProcessDialog::reloadRunningProcesses() {
    QString error;
    runningRows_ = queryRunningProcesses(&error);
    refreshRunningTable();
    if (runningRows_.isEmpty() && !error.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), error);
    }
}

void AddProcessDialog::refreshRunningTable() {
    const auto query = runningSearch_->text().trimmed();
    runningTable_->setSortingEnabled(false);
    runningTable_->setRowCount(0);
    for (int i = 0; i < runningRows_.size(); ++i) {
        const auto &row = runningRows_.at(i);
        const auto haystack = (row.displayName + " " + row.processName + " " + row.processPath);
        if (!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive)) continue;

        const int tableRow = runningTable_->rowCount();
        runningTable_->insertRow(tableRow);

        auto *displayItem = new QTableWidgetItem(row.icon, row.displayName);
        displayItem->setData(Qt::UserRole, i);
        runningTable_->setItem(tableRow, 0, displayItem);
        runningTable_->setItem(tableRow, 1, new QTableWidgetItem(row.processName));
        runningTable_->setItem(tableRow, 2, new QTableWidgetItem(row.processPath));
        runningTable_->setItem(tableRow, 3, new QTableWidgetItem(
                                                row.processPath.isEmpty() ? tr("process_name") : tr("process_path")));
    }
    runningTable_->setSortingEnabled(true);
}

AppRoutingRules::Entry AddProcessDialog::makeEntryFromRow(const RunningProcessRow &row, bool matchByPath) const {
    if (matchByPath && !row.processPath.isEmpty()) {
        return AppRoutingRules::MakePathEntry(row.processPath, row.displayName);
    }
    return AppRoutingRules::MakeNameEntry(row.processName, row.displayName);
}

void AddProcessDialog::addFromCurrentTab() {
    selectedEntries_.clear();
    switch (tabs_->currentIndex()) {
        case 0:
            addFromRunning();
            break;
        case 1:
            addFromFile();
            break;
        case 2:
            addFromClipboard();
            break;
        default:
            break;
    }
    if (!selectedEntries_.isEmpty()) {
        accept();
    }
}

void AddProcessDialog::addFromRunning() {
    const auto indexes = runningTable_->selectionModel()->selectedRows();
    if (indexes.isEmpty()) {
        QMessageBox::information(this, tr("Select application"), tr("Choose at least one running process."));
        return;
    }
    const bool matchByPath = runningMatchMode_->currentData().toString() == "path";
    QSet<QString> dedupe;
    for (const auto &index : indexes) {
        auto *item = runningTable_->item(index.row(), 0);
        if (item == nullptr) continue;
        const int sourceIndex = item->data(Qt::UserRole).toInt();
        if (sourceIndex < 0 || sourceIndex >= runningRows_.size()) continue;
        auto entry = makeEntryFromRow(runningRows_.at(sourceIndex), matchByPath);
        const auto key = entryKey(entry);
        if (dedupe.contains(key)) continue;
        dedupe.insert(key);
        selectedEntries_ << entry;
    }
}

void AddProcessDialog::addFromFile() {
    auto filePath = filePathEdit_->text().trimmed();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Select executable file first."));
        return;
    }
    filePath = normalizePath(filePath);
    const auto displayName = fileDisplayEdit_->text().trimmed();
    const bool matchByPath = fileMatchMode_->currentData().toString() == "path";
    if (matchByPath) {
        selectedEntries_ << AppRoutingRules::MakePathEntry(filePath, displayName);
    } else {
        selectedEntries_ << AppRoutingRules::MakeNameEntry(QFileInfo(filePath).fileName(), displayName);
    }
}

void AddProcessDialog::addFromClipboard() {
    auto text = clipboardPreview_->toPlainText().trimmed();
    if (text.isEmpty()) {
        text = QApplication::clipboard()->text().trimmed();
    }
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Clipboard is empty."));
        return;
    }
    text = text.split(QRegularExpression("[\r\n]"), kSkipEmptyParts).value(0).trimmed();
    if (text.startsWith("\"") && text.endsWith("\"") && text.size() >= 2) {
        text = text.mid(1, text.size() - 2);
    }
    text = normalizePath(text);
    if (text.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Clipboard does not contain executable path."));
        return;
    }
    const bool matchByPath = clipboardMatchMode_->currentData().toString() == "path";
    if (matchByPath) {
        selectedEntries_ << AppRoutingRules::MakePathEntry(text);
    } else {
        selectedEntries_ << AppRoutingRules::MakeNameEntry(QFileInfo(text).fileName());
    }
}

} // namespace

DialogAppRouting::DialogAppRouting(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Application routing"));
    resize(940, 620);

    auto *rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(14, 14, 14, 14);
    rootLayout->setSpacing(10);

    auto *desc = new QLabel(
        tr("Simple setup: choose which applications use Proxy/TUN, and which go Direct."),
        this);
    desc->setWordWrap(true);
    rootLayout->addWidget(desc);

    auto *modeBox = new QGroupBox(tr("Mode"), this);
    auto *modeLayout = new QVBoxLayout(modeBox);
    modeLayout->setContentsMargins(10, 10, 10, 10);
    modeLayout->setSpacing(6);
    modeGroup_ = new QButtonGroup(this);
    modeAllRadio_ = new QRadioButton(tr("By default: all applications through Proxy/TUN"), modeBox);
    modeAllowRadio_ = new QRadioButton(tr("Only selected applications through Proxy/TUN"), modeBox);
    modeBlockRadio_ = new QRadioButton(tr("All through Proxy/TUN except selected"), modeBox);
    modeGroup_->addButton(modeAllRadio_, 0);
    modeGroup_->addButton(modeAllowRadio_, 1);
    modeGroup_->addButton(modeBlockRadio_, 2);
    modeLayout->addWidget(modeAllRadio_);
    modeLayout->addWidget(modeAllowRadio_);
    modeLayout->addWidget(modeBlockRadio_);
    modeHintLabel_ = new QLabel(modeBox);
    modeHintLabel_->setWordWrap(true);
    modeLayout->addWidget(modeHintLabel_);
    rootLayout->addWidget(modeBox);

    rulesGroup_ = new QGroupBox(this);
    auto *rulesLayout = new QVBoxLayout(rulesGroup_);
    rulesLayout->setContentsMargins(10, 10, 10, 10);
    rulesLayout->setSpacing(8);

    auto *toolbar = new QHBoxLayout;
    searchEdit_ = new QLineEdit(rulesGroup_);
    searchEdit_->setPlaceholderText(tr("Search in list"));
    addButton_ = new QPushButton(tr("Add application"), rulesGroup_);
    removeButton_ = new QPushButton(tr("Remove"), rulesGroup_);
    importButton_ = new QPushButton(tr("Import"), rulesGroup_);
    exportButton_ = new QPushButton(tr("Export"), rulesGroup_);
    checkButton_ = new QPushButton(tr("Check rule"), rulesGroup_);
    toolbar->addWidget(searchEdit_, 1);
    toolbar->addWidget(addButton_, 0);
    toolbar->addWidget(removeButton_, 0);
    toolbar->addWidget(importButton_, 0);
    toolbar->addWidget(exportButton_, 0);
    toolbar->addWidget(checkButton_, 0);
    rulesLayout->addLayout(toolbar);

    rulesTable_ = new QTableWidget(rulesGroup_);
    rulesTable_->setColumnCount(3);
    rulesTable_->setHorizontalHeaderLabels({tr("Application"), tr("Match"), tr("Value")});
    rulesTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    rulesTable_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    rulesTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    rulesTable_->setAlternatingRowColors(true);
    rulesTable_->setContextMenuPolicy(Qt::CustomContextMenu);
    rulesTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    rulesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    rulesLayout->addWidget(rulesTable_, 1);
    rootLayout->addWidget(rulesGroup_, 1);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    rootLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &DialogAppRouting::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(modeGroup_, &QButtonGroup::idClicked, this, [this](int) {
        syncModeUi();
    });
#else
    connect(modeGroup_, static_cast<void (QButtonGroup::*)(int)>(&QButtonGroup::buttonClicked), this, [this](int) {
        syncModeUi();
    });
#endif
    connect(searchEdit_, &QLineEdit::textChanged, this, [this] { updateFilter(); });
    connect(addButton_, &QPushButton::clicked, this, [this] { addRule(); });
    connect(removeButton_, &QPushButton::clicked, this, [this] { removeSelectedRules(); });
    connect(importButton_, &QPushButton::clicked, this, [this] { importRules(); });
    connect(exportButton_, &QPushButton::clicked, this, [this] { exportRules(); });
    connect(checkButton_, &QPushButton::clicked, this, [this] { checkRuleRouting(); });
    connect(rulesTable_, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        showRulesContextMenu(pos);
    });
    auto *delShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(delShortcut, &QShortcut::activated, this, [this] { removeSelectedRules(); });

    loadFromDataStore();
}

DialogAppRouting::Mode DialogAppRouting::currentMode() const {
    if (modeAllowRadio_->isChecked()) return Mode::OnlySelected;
    if (modeBlockRadio_->isChecked()) return Mode::ExceptSelected;
    return Mode::AllThroughProxy;
}

void DialogAppRouting::loadFromDataStore() {
    rules_ = AppRoutingRules::Parse(NekoGui::dataStore->vpn_rule_process);
    if (rules_.isEmpty() && !NekoGui::dataStore->vpn_rule_white) {
        modeAllRadio_->setChecked(true);
    } else if (NekoGui::dataStore->vpn_rule_white) {
        modeAllowRadio_->setChecked(true);
    } else {
        modeBlockRadio_->setChecked(true);
    }
    syncModeUi();
}

void DialogAppRouting::syncModeUi() {
    const auto mode = currentMode();
    if (mode == Mode::AllThroughProxy) {
        rulesGroup_->setTitle(tr("Lists are disabled"));
        modeHintLabel_->setText(tr("Everything goes through Proxy/TUN. Use this if you don't need per-app rules."));
    } else if (mode == Mode::OnlySelected) {
        rulesGroup_->setTitle(tr("Applications through Proxy/TUN"));
        modeHintLabel_->setText(tr("Useful for games or selected programs. Other applications go Direct."));
    } else {
        rulesGroup_->setTitle(tr("Direct exceptions"));
        modeHintLabel_->setText(tr("Useful when most traffic goes through Proxy/TUN, except selected applications."));
    }

    if (!NekoGui::dataStore->spmode_vpn) {
        modeHintLabel_->setText(modeHintLabel_->text() + "\n" +
                                tr("Note: process routing is applied in TUN mode."));
    }

    const bool listEnabled = mode != Mode::AllThroughProxy;
    rulesTable_->setEnabled(listEnabled);
    addButton_->setEnabled(listEnabled);
    removeButton_->setEnabled(listEnabled);
    importButton_->setEnabled(listEnabled);
    exportButton_->setEnabled(listEnabled);
    checkButton_->setEnabled(listEnabled);
    refreshRulesTable();
}

void DialogAppRouting::refreshRulesTable() {
    const auto query = searchEdit_->text().trimmed();
    rulesTable_->setRowCount(0);
    for (int i = 0; i < rules_.size(); ++i) {
        const auto &entry = rules_.at(i);
        const auto haystack = (entry.displayName + " " + entry.value);
        if (!query.isEmpty() && !haystack.contains(query, Qt::CaseInsensitive)) continue;

        const int row = rulesTable_->rowCount();
        rulesTable_->insertRow(row);
        auto *nameItem = new QTableWidgetItem(entry.displayName);
        nameItem->setData(RoleSourceIndex, i);
        rulesTable_->setItem(row, 0, nameItem);
        rulesTable_->setItem(row, 1, new QTableWidgetItem(AppRoutingRules::MatchTypeLabel(entry.matchType)));
        rulesTable_->setItem(row, 2, new QTableWidgetItem(entry.value));
    }
}

void DialogAppRouting::updateFilter() {
    refreshRulesTable();
}

void DialogAppRouting::addRule() {
    AddProcessDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted) return;

    int added = 0;
    QSet<QString> existing;
    for (const auto &entry : rules_) {
        existing.insert(entryKey(entry));
    }
    for (auto entry : dialog.selectedEntries()) {
        if (entry.displayName.trimmed().isEmpty()) {
            entry.displayName = AppRoutingRules::SuggestDisplayName(entry);
        }
        const auto key = entryKey(entry);
        if (existing.contains(key)) continue;
        existing.insert(key);
        rules_ << entry;
        ++added;
    }
    if (added == 0) {
        QMessageBox::information(this, tr("Already added"), tr("Selected application already exists in the list."));
    }
    refreshRulesTable();
}

void DialogAppRouting::removeSelectedRules() {
    const auto selected = rulesTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    QList<int> sourceIndexes;
    for (const auto &index : selected) {
        auto *item = rulesTable_->item(index.row(), 0);
        if (item == nullptr) continue;
        sourceIndexes << item->data(RoleSourceIndex).toInt();
    }
    std::sort(sourceIndexes.begin(), sourceIndexes.end(), std::greater<int>());
    sourceIndexes.erase(std::unique(sourceIndexes.begin(), sourceIndexes.end()), sourceIndexes.end());
    for (int sourceIndex : sourceIndexes) {
        if (sourceIndex < 0 || sourceIndex >= rules_.size()) continue;
        rules_.removeAt(sourceIndex);
    }
    refreshRulesTable();
}

void DialogAppRouting::showRulesContextMenu(const QPoint &pos) {
    auto *item = rulesTable_->itemAt(pos);
    if (item == nullptr) return;
    const int row = item->row();
    auto *nameItem = rulesTable_->item(row, 0);
    if (nameItem == nullptr) return;
    const int sourceIndex = nameItem->data(RoleSourceIndex).toInt();
    if (sourceIndex < 0 || sourceIndex >= rules_.size()) return;
    auto &entry = rules_[sourceIndex];

    QMenu menu(this);
    auto *renameAction = menu.addAction(tr("Rename display name"));
    auto *copyAction = menu.addAction(tr("Copy value"));
    QAction *openFolderAction = nullptr;
    if (entry.matchType == AppRoutingRules::MatchType::ProcessPath) {
        openFolderAction = menu.addAction(tr("Open containing folder"));
    }
    menu.addSeparator();
    auto *deleteAction = menu.addAction(tr("Delete"));

    const auto chosen = menu.exec(rulesTable_->viewport()->mapToGlobal(pos));
    if (chosen == nullptr) return;
    if (chosen == renameAction) {
        bool ok = false;
        const auto value = QInputDialog::getText(this, tr("Rename"),
                                                 tr("Display name"),
                                                 QLineEdit::Normal,
                                                 entry.displayName,
                                                 &ok).trimmed();
        if (ok && !value.isEmpty()) {
            entry.displayName = value;
            refreshRulesTable();
        }
        return;
    }
    if (chosen == copyAction) {
        QApplication::clipboard()->setText(entry.value);
        return;
    }
    if (openFolderAction != nullptr && chosen == openFolderAction) {
        QFileInfo info(entry.value);
        if (info.exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
        }
        return;
    }
    if (chosen == deleteAction) {
        rules_.removeAt(sourceIndex);
        refreshRulesTable();
    }
}

void DialogAppRouting::importRules() {
    const auto fileName = QFileDialog::getOpenFileName(this, tr("Import rules"), {}, tr("JSON files (*.json);;All files (*)"));
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open file."));
        return;
    }
    QJsonParseError error{};
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(this, tr("Error"), tr("Invalid JSON format."));
        return;
    }

    const auto object = document.object();
    const auto mode = modeFromKey(object.value("mode").toString());
    QList<AppRoutingRules::Entry> parsed;
    for (const auto &value : object.value("entries").toArray()) {
        if (!value.isObject()) continue;
        const auto o = value.toObject();
        AppRoutingRules::Entry entry;
        entry.displayName = o.value("display").toString().trimmed();
        entry.value = o.value("value").toString().trimmed();
        entry.matchType = (o.value("match").toString() == "process_path")
                              ? AppRoutingRules::MatchType::ProcessPath
                              : AppRoutingRules::MatchType::ProcessName;
        if (entry.value.isEmpty()) continue;
        if (entry.displayName.isEmpty()) {
            entry.displayName = AppRoutingRules::SuggestDisplayName(entry);
        }
        parsed << entry;
    }
    rules_ = AppRoutingRules::Parse(AppRoutingRules::Serialize(parsed));
    modeAllRadio_->setChecked(mode == Mode::AllThroughProxy);
    modeAllowRadio_->setChecked(mode == Mode::OnlySelected);
    modeBlockRadio_->setChecked(mode == Mode::ExceptSelected);
    syncModeUi();
}

void DialogAppRouting::exportRules() {
    const auto fileName = QFileDialog::getSaveFileName(this,
                                                       tr("Export rules"),
                                                       "app-routing-rules.json",
                                                       tr("JSON files (*.json)"));
    if (fileName.isEmpty()) return;

    QJsonArray entries;
    for (const auto &entry : rules_) {
        entries.append(QJsonObject{
            {"display", entry.displayName},
            {"match", AppRoutingRules::MatchTypeKey(entry.matchType)},
            {"value", entry.value},
        });
    }

    QJsonObject root{
        {"mode", modeKey(currentMode())},
        {"entries", entries},
    };

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot write file."));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    file.close();
}

void DialogAppRouting::checkRuleRouting() {
    const auto mode = currentMode();
    if (mode == Mode::AllThroughProxy) {
        QMessageBox::information(this,
                                 tr("Rule check"),
                                 tr("Result: all applications go through Proxy/TUN."));
        return;
    }
    if (rules_.isEmpty()) {
        QMessageBox::information(this,
                                 tr("Rule check"),
                                 mode == Mode::OnlySelected
                                     ? tr("List is empty: all applications will go Direct.")
                                     : tr("List is empty: all applications will go through Proxy/TUN."));
        return;
    }

    QStringList appNames;
    appNames.reserve(rules_.size());
    for (const auto &entry : rules_) {
        appNames << QStringLiteral("%1 (%2)").arg(entry.displayName, entry.value);
    }
    bool ok = false;
    const auto chosen = QInputDialog::getItem(this,
                                              tr("Rule check"),
                                              tr("Choose application from list"),
                                              appNames,
                                              0,
                                              false,
                                              &ok);
    if (!ok || chosen.isEmpty()) return;

    const auto result = (mode == Mode::OnlySelected) ? tr("Proxy/TUN") : tr("Direct");
    QMessageBox::information(this,
                             tr("Rule check"),
                             tr("For selected application: %1").arg(result));
}

void DialogAppRouting::accept() {
    const auto mode = currentMode();
    if (mode == Mode::AllThroughProxy) {
        NekoGui::dataStore->vpn_rule_white = false;
        NekoGui::dataStore->vpn_rule_process = "";
    } else if (mode == Mode::OnlySelected) {
        NekoGui::dataStore->vpn_rule_white = true;
        NekoGui::dataStore->vpn_rule_process = AppRoutingRules::Serialize(rules_);
    } else {
        NekoGui::dataStore->vpn_rule_white = false;
        NekoGui::dataStore->vpn_rule_process = AppRoutingRules::Serialize(rules_);
    }
    QDialog::accept();
}
