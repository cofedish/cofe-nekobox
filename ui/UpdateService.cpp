#include "ui/UpdateService.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaEnum>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QVersionNumber>

#include "main/AppInfo.hpp"
#include "main/NekoGui.hpp"

#ifdef Q_OS_LINUX
#include "sys/linux/LinuxCap.h"
#endif

namespace {
constexpr const char *kGithubLatestApi = "https://api.github.com/repos/cofedish/cofe-nekobox/releases/latest";
constexpr const char *kGithubReleasesApi = "https://api.github.com/repos/cofedish/cofe-nekobox/releases";
constexpr const char *kReleaseDownloadPrefix = "https://github.com/cofedish/cofe-nekobox/releases/download/";

QString joinDetail(const QString &a, const QString &b) {
    if (a.isEmpty()) return b;
    if (b.isEmpty()) return a;
    return a + "\n" + b;
}
} // namespace

UpdateService::UpdateService(QObject *parent) : QObject(parent) {
    qRegisterMetaType<UpdateService::UpdateInfo>("UpdateService::UpdateInfo");
    network_ = new QNetworkAccessManager(this);
    info_.currentVersion = AppInfo::Version();
    QString modeReason;
    info_.installMode = detectInstallMode(&modeReason);
    if (!modeReason.isEmpty()) {
        info_.autoInstallReason = modeReason;
    }
    appendLog(QStringLiteral("UpdateService initialized. current=%1 mode=%2")
                  .arg(info_.currentVersion, installModeName(info_.installMode)));
}

UpdateService::~UpdateService() {
    cancelUpdate();
}

UpdateService::State UpdateService::state() const {
    return state_;
}

UpdateService::UpdateInfo UpdateService::info() const {
    return info_;
}

QString UpdateService::message() const {
    return message_;
}

QString UpdateService::details() const {
    return details_;
}

QString UpdateService::logFilePath() const {
    return QDir::current().absoluteFilePath("logs/updater.log");
}

bool UpdateService::lastCheckWasManual() const {
    return lastCheckManual_;
}

void UpdateService::checkForUpdates(bool manual) {
    if (state_ == State::Downloading || state_ == State::Installing) {
        return;
    }
    cancelUpdate();
    lastCheckManual_ = manual;
    details_.clear();
    setProgress(0.0);

    info_ = UpdateInfo{};
    info_.currentVersion = AppInfo::Version();
    QString modeReason;
    info_.installMode = detectInstallMode(&modeReason);
    if (!modeReason.isEmpty()) info_.autoInstallReason = modeReason;
    emit updateInfoChanged(info_);

    setState(State::Checking, tr("Checking for updates..."));
    appendLog(QStringLiteral("Check started manual=%1 mode=%2 reason=%3")
                  .arg(manual ? "true" : "false", installModeName(info_.installMode), info_.autoInstallReason));

    const bool includePrerelease = NekoGui::dataStore->check_include_pre;
    const QUrl apiUrl(includePrerelease ? QString::fromLatin1(kGithubReleasesApi) : QString::fromLatin1(kGithubLatestApi));
    auto reply = sendGet(apiUrl);
    if (reply == nullptr) {
        setFailure(tr("Update check could not start."));
        return;
    }
    activeReply_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleCheckReply(reply);
    });
}

void UpdateService::startUpdate() {
    if (state_ != State::UpdateAvailable) {
        return;
    }
    if (!info_.available) {
        return;
    }
    if (!info_.autoInstallSupported) {
        setFailure(tr("Automatic update is unavailable."), info_.autoInstallReason);
        return;
    }
    if (!info_.assetUrl.isValid() || !info_.checksumsUrl.isValid()) {
        setFailure(tr("Update asset metadata is incomplete."));
        return;
    }

    QString baseTemp = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (baseTemp.isEmpty()) {
        baseTemp = QDir::tempPath();
    }
    if (baseTemp.isEmpty()) {
        setFailure(tr("Temporary directory is unavailable."));
        return;
    }

    QDir tempBase(baseTemp);
    if (!tempBase.mkpath("cofebox-update")) {
        setFailure(tr("Cannot create update directory."), tempBase.absoluteFilePath("cofebox-update"));
        return;
    }
    downloadTempDir_ = tempBase.absoluteFilePath(QStringLiteral("cofebox-update/%1-%2")
                                                     .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMddhhmmsszzz"))
                                                     .arg(QCoreApplication::applicationPid()));
    if (!QDir().mkpath(downloadTempDir_)) {
        setFailure(tr("Cannot prepare update workspace."), downloadTempDir_);
        return;
    }

    appendLog(QStringLiteral("Update started version=%1 asset=%2 temp=%3")
                  .arg(info_.latestVersion, info_.assetName, downloadTempDir_));
    setState(State::Downloading, tr("Downloading checksums..."));
    downloadStage_ = DownloadStage::Checksums;
    auto reply = sendGet(info_.checksumsUrl);
    if (reply == nullptr) {
        setFailure(tr("Failed to request checksums."));
        return;
    }
    activeReply_ = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleChecksumsReply(reply);
    });
}

void UpdateService::cancelUpdate() {
    if (activeReply_ != nullptr) {
        disconnect(activeReply_, nullptr, this, nullptr);
        activeReply_->abort();
        activeReply_->deleteLater();
        activeReply_ = nullptr;
    }
    if (downloadFile_ != nullptr) {
        downloadFile_->flush();
        downloadFile_->close();
        downloadFile_->deleteLater();
        downloadFile_ = nullptr;
    }
    if (debInstallProcess_ != nullptr) {
        disconnect(debInstallProcess_, nullptr, this, nullptr);
        debInstallProcess_->kill();
        debInstallProcess_->deleteLater();
        debInstallProcess_ = nullptr;
    }
    downloadStage_ = DownloadStage::None;
}

void UpdateService::setState(State state, const QString &message) {
    state_ = state;
    emit stateChanged(state_);
    if (!message.isNull()) {
        message_ = message;
        emit messageChanged(message_);
    }
    appendLog(QStringLiteral("State -> %1 | %2")
                  .arg(QMetaEnum::fromType<State>().valueToKey(static_cast<int>(state_)))
                  .arg(message_));
}

void UpdateService::setProgress(double progress01) {
    const double bounded = qBound(0.0, progress01, 1.0);
    if (qFuzzyCompare(progress_ + 1.0, bounded + 1.0)) {
        return;
    }
    progress_ = bounded;
    emit progressChanged(progress_);
}

void UpdateService::setFailure(const QString &shortMessage, const QString &detailMessage) {
    details_ = detailMessage;
    setState(State::Failed, shortMessage);
    if (!detailMessage.isEmpty()) {
        appendLog(QStringLiteral("Failure detail: %1").arg(detailMessage));
    }
}

void UpdateService::appendLog(const QString &line) {
    QDir dir(QDir::current());
    if (!dir.exists("logs")) {
        dir.mkpath("logs");
    }
    QFile file(logFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    file.write(QStringLiteral("[%1] %2\n").arg(stamp, line).toUtf8());
    file.close();
}

UpdateService::InstallMode UpdateService::detectInstallMode(QString *reason) const {
    if (reason != nullptr) reason->clear();
#ifdef Q_OS_WIN
    QString writeReason;
    const auto appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
    if (!isWritableDirectory(appDir, &writeReason)) {
        if (reason != nullptr) *reason = tr("Application directory is read-only: %1").arg(writeReason);
        return InstallMode::Unknown;
    }
    return InstallMode::WindowsZip;
#endif

#ifdef Q_OS_LINUX
    if (isAppImage()) {
        const auto path = appImagePath();
        QString writeReason;
        if (!isWritablePath(path, &writeReason)) {
            if (reason != nullptr) {
                *reason = tr("AppImage file is not writable: %1").arg(writeReason);
            }
            return InstallMode::Unknown;
        }
        return InstallMode::LinuxAppImage;
    }

    if (hasDpkgPackageInstalled("cofebox") || hasDpkgPackageInstalled("nekoray")) {
        return InstallMode::LinuxDeb;
    }
    if (QFileInfo("/usr/bin/cofebox").exists() || QCoreApplication::applicationFilePath().startsWith("/opt/")) {
        return InstallMode::LinuxDeb;
    }

    if (reason != nullptr) {
        *reason = tr("Unknown Linux installation type.");
    }
    return InstallMode::Unknown;
#endif

    if (reason != nullptr) {
        *reason = tr("Unsupported platform.");
    }
    return InstallMode::Unknown;
}

bool UpdateService::isAppImage() const {
#ifdef Q_OS_LINUX
    return qEnvironmentVariableIsSet("APPIMAGE");
#else
    return false;
#endif
}

bool UpdateService::isWritablePath(const QString &path, QString *reason) const {
    QFileInfo fi(path);
    if (fi.exists()) {
        if (!fi.isWritable()) {
            if (reason != nullptr) *reason = tr("No write permission for %1").arg(path);
            return false;
        }
        return isWritableDirectory(fi.absolutePath(), reason);
    }
    return isWritableDirectory(fi.absolutePath(), reason);
}

bool UpdateService::isWritableDirectory(const QString &dirPath, QString *reason) const {
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (reason != nullptr) *reason = tr("Directory does not exist: %1").arg(dirPath);
        return false;
    }
    const QString probe = dir.absoluteFilePath(QStringLiteral(".cofebox-write-probe-%1.tmp").arg(QCoreApplication::applicationPid()));
    QFile file(probe);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (reason != nullptr) *reason = file.errorString();
        return false;
    }
    file.write("ok");
    file.close();
    QFile::remove(probe);
    return true;
}

bool UpdateService::hasDpkgPackageInstalled(const QString &packageName) const {
#ifdef Q_OS_LINUX
    QProcess process;
    process.start("dpkg-query", {"-W", "-f=${Status}", packageName});
    if (!process.waitForFinished(1800)) {
        process.kill();
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }
    return QString::fromUtf8(process.readAllStandardOutput()).contains("install ok installed", Qt::CaseInsensitive);
#else
    Q_UNUSED(packageName)
    return false;
#endif
}

QString UpdateService::updaterBinaryPath() const {
    const QString appDir = QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
#ifdef Q_OS_WIN
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("cofebox-updater.exe"),
        QDir(appDir).absoluteFilePath("updater.exe"),
    };
#else
    const QStringList candidates = {
        QDir(appDir).absoluteFilePath("cofebox-updater"),
        QDir(appDir).absoluteFilePath("updater"),
    };
#endif
    for (const auto &candidate : candidates) {
        QFileInfo fi(candidate);
        if (fi.exists() && fi.isFile() && fi.isExecutable()) {
            return candidate;
        }
    }
    return {};
}

QString UpdateService::appImagePath() const {
    return qEnvironmentVariable("APPIMAGE");
}

QNetworkReply *UpdateService::sendGet(const QUrl &url) {
    if (!url.isValid()) {
        return nullptr;
    }
    QNetworkRequest request(url);
#if QT_VERSION >= QT_VERSION_CHECK(5, 9, 0)
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
#endif
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("CofeBox-Updater/%1").arg(AppInfo::Version()));
    request.setRawHeader("Accept", "application/vnd.github+json");

    QString token = qEnvironmentVariable("UPDATE_TOKEN");
    if (token.isEmpty()) token = qEnvironmentVariable("GITHUB_TOKEN");
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    }
    appendLog(QStringLiteral("HTTP GET %1").arg(url.toString()));
    return network_->get(request);
}

void UpdateService::handleCheckReply(QNetworkReply *reply) {
    if (reply == nullptr) return;
    activeReply_ = nullptr;

    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply->error();
    const QString errorString = reply->errorString();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (error != QNetworkReply::NoError) {
        setFailure(tr("Update check failed."), joinDetail(QStringLiteral("HTTP %1").arg(httpCode), errorString));
        return;
    }
    appendLog(QStringLiteral("Release API OK. HTTP=%1 payload=%2 bytes").arg(httpCode).arg(payload.size()));

    const bool includePrerelease = NekoGui::dataStore->check_include_pre;
    auto release = parseLatestReleaseJson(payload, includePrerelease);
    if (!release.valid) {
        setFailure(tr("Could not parse release metadata."));
        return;
    }

    if (!isTrustedReleasePageUrl(release.htmlUrl)) {
        setFailure(tr("Release source is not trusted."), release.htmlUrl.toString());
        return;
    }

    const QString currentVersion = normalizeVersion(info_.currentVersion);
    const QString latestVersion = normalizeVersion(release.versionTag);
    const int cmp = compareVersions(currentVersion, latestVersion);
    appendLog(QStringLiteral("Version compare current=%1 latest=%2 result=%3")
                  .arg(currentVersion, latestVersion).arg(cmp));

    info_.latestVersion = latestVersion;
    info_.releaseTitle = release.title;
    info_.releaseNotes = release.notes;
    info_.releaseUrl = release.htmlUrl;
    info_.isPrerelease = release.prerelease;
    info_.available = cmp < 0;

    if (cmp >= 0) {
        setState(State::UpToDate, tr("You are using the latest version."));
        emit updateInfoChanged(info_);
        return;
    }

    auto selectAsset = [&](InstallMode mode) -> ReleaseAsset {
        ReleaseAsset best;
        int bestScore = -1;
        for (const auto &asset : release.assets) {
            const auto name = asset.name.toLower();
            int score = -1;
            if (mode == InstallMode::WindowsZip) {
                if (name.contains("windows") && name.endsWith(".zip")) score = 10;
                else if (name.endsWith(".zip")) score = 2;
            } else if (mode == InstallMode::LinuxAppImage) {
                if (name.endsWith(".appimage")) score = 10;
            } else if (mode == InstallMode::LinuxDeb) {
                if (name.contains("debian") && name.endsWith(".deb")) score = 10;
                else if (name.endsWith("_amd64.deb")) score = 9;
                else if (name.endsWith(".deb")) score = 3;
            }
            if (score > bestScore) {
                best = asset;
                bestScore = score;
            }
        }
        if (bestScore < 0) return {};
        return best;
    };

    ReleaseAsset targetAsset = selectAsset(info_.installMode);
    if (targetAsset.name.isEmpty()) {
#ifdef Q_OS_LINUX
        if (info_.installMode == InstallMode::Unknown) {
            targetAsset = selectAsset(InstallMode::LinuxAppImage);
            if (targetAsset.name.isEmpty()) targetAsset = selectAsset(InstallMode::LinuxDeb);
        }
#endif
#ifdef Q_OS_WIN
        if (info_.installMode == InstallMode::Unknown) {
            targetAsset = selectAsset(InstallMode::WindowsZip);
        }
#endif
    }

    if (targetAsset.name.isEmpty() || !isTrustedReleaseAssetUrl(targetAsset.url)) {
        setFailure(tr("No compatible update asset found for this system."));
        emit updateInfoChanged(info_);
        return;
    }

    QUrl checksumsUrl;
    QString checksumsName;
    for (const auto &asset : release.assets) {
        const auto lower = asset.name.toLower();
        if (lower == "sha256sums.txt" || lower == "checksums.txt" || lower == "sha256sum.txt") {
            checksumsUrl = asset.url;
            checksumsName = asset.name;
            break;
        }
    }

    info_.assetName = targetAsset.name;
    info_.assetUrl = targetAsset.url;
    info_.checksumsUrl = checksumsUrl;
    info_.assetSize = targetAsset.size;
    info_.autoInstallSupported = info_.installMode != InstallMode::Unknown;
#ifdef Q_OS_LINUX
    if (info_.installMode == InstallMode::LinuxDeb && !Linux_HavePkexec()) {
        info_.autoInstallSupported = false;
        info_.autoInstallReason = tr("pkexec is missing. Install policykit-1 to enable automatic .deb update.");
    }
#endif

    if (checksumsUrl.isEmpty()) {
        info_.autoInstallSupported = false;
        info_.autoInstallReason = tr("Release has no checksums file (sha256sums.txt).");
    } else if (!isTrustedReleaseAssetUrl(checksumsUrl)) {
        info_.autoInstallSupported = false;
        info_.autoInstallReason = tr("Checksums source is not trusted.");
    } else if (info_.installMode == InstallMode::Unknown && info_.autoInstallReason.isEmpty()) {
        info_.autoInstallReason = tr("Cannot determine installation type for automatic update.");
    }

    appendLog(QStringLiteral("Update available latest=%1 asset=%2 checksums=%3 auto=%4")
                  .arg(info_.latestVersion, info_.assetName, checksumsName,
                       info_.autoInstallSupported ? "true" : "false"));
    setState(State::UpdateAvailable, tr("Update available: v%1").arg(info_.latestVersion));
    emit updateInfoChanged(info_);
}

void UpdateService::handleChecksumsReply(QNetworkReply *reply) {
    if (reply == nullptr) return;
    activeReply_ = nullptr;

    const auto error = reply->error();
    const QString errorString = reply->errorString();
    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    reply->deleteLater();

    if (error != QNetworkReply::NoError) {
        setFailure(tr("Failed to download checksums."), joinDetail(QStringLiteral("HTTP %1").arg(httpCode), errorString));
        return;
    }

    expectedSha256_ = findChecksumForAsset(payload, info_.assetName);
    if (expectedSha256_.isEmpty()) {
        setFailure(tr("Checksums file does not contain target asset hash."), info_.assetName);
        return;
    }
    if (expectedSha256_.size() != 64) {
        setFailure(tr("Checksums file contains invalid SHA256 value."), expectedSha256_);
        return;
    }
    appendLog(QStringLiteral("Checksums loaded for asset=%1 sha256=%2").arg(info_.assetName, expectedSha256_));
    beginAssetDownload();
}

void UpdateService::beginAssetDownload() {
    downloadStage_ = DownloadStage::Asset;
    downloadedAssetPath_ = QDir(downloadTempDir_).absoluteFilePath(info_.assetName);
    if (downloadFile_ != nullptr) {
        downloadFile_->deleteLater();
    }
    downloadFile_ = new QFile(downloadedAssetPath_, this);
    if (!downloadFile_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setFailure(tr("Cannot write downloaded update package."), downloadFile_->errorString());
        return;
    }

    setState(State::Downloading, tr("Downloading update package..."));
    setProgress(0.0);
    auto reply = sendGet(info_.assetUrl);
    if (reply == nullptr) {
        setFailure(tr("Failed to start update download."));
        return;
    }
    activeReply_ = reply;
    connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
        if (downloadFile_ == nullptr) return;
        const auto chunk = reply->readAll();
        if (!chunk.isEmpty()) {
            downloadFile_->write(chunk);
        }
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (downloadStage_ != DownloadStage::Asset) return;
        if (total <= 0) return;
        setProgress(static_cast<double>(received) / static_cast<double>(total));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        handleAssetReplyFinished(reply);
    });
}

void UpdateService::handleAssetReplyFinished(QNetworkReply *reply) {
    if (reply == nullptr) return;
    activeReply_ = nullptr;

    if (downloadFile_ != nullptr) {
        downloadFile_->flush();
        downloadFile_->close();
    }

    const auto error = reply->error();
    const QString errorString = reply->errorString();
    const auto httpCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    downloadStage_ = DownloadStage::None;

    if (error != QNetworkReply::NoError) {
        setFailure(tr("Failed to download update package."), joinDetail(QStringLiteral("HTTP %1").arg(httpCode), errorString));
        return;
    }
    if (info_.assetSize > 0) {
        QFileInfo fi(downloadedAssetPath_);
        if (fi.size() != info_.assetSize) {
            setFailure(tr("Downloaded package size mismatch."),
                       tr("Expected: %1 bytes\nActual: %2 bytes").arg(info_.assetSize).arg(fi.size()));
            return;
        }
    }

    setProgress(1.0);
    setState(State::Verifying, tr("Verifying package integrity..."));

    QString actualHash;
    if (!verifySha256File(downloadedAssetPath_, expectedSha256_, &actualHash)) {
        setFailure(tr("SHA256 verification failed."),
                   tr("Expected: %1\nActual: %2").arg(expectedSha256_, actualHash));
        return;
    }
    appendLog(QStringLiteral("SHA256 verified ok asset=%1").arg(downloadedAssetPath_));
    finalizeVerifiedDownload();
}

void UpdateService::finalizeVerifiedDownload() {
    setState(State::Installing, tr("Installing update..."));

    if (info_.installMode == InstallMode::LinuxDeb) {
        beginDebInstall();
        return;
    }

    const QString helper = updaterBinaryPath();
    if (helper.isEmpty()) {
        setFailure(tr("Updater helper is missing."),
                   tr("Expected cofebox-updater in %1").arg(QFileInfo(QCoreApplication::applicationFilePath()).absolutePath()));
        return;
    }

    QStringList args;
    args << "--mode";
    if (info_.installMode == InstallMode::WindowsZip) {
        args << "windows"
             << "--pid" << QString::number(QCoreApplication::applicationPid())
             << "--install-dir" << QFileInfo(QCoreApplication::applicationFilePath()).absolutePath()
             << "--archive-path" << downloadedAssetPath_
             << "--app-exe" << QCoreApplication::applicationFilePath()
             << "--backup-tag" << normalizeVersion(info_.currentVersion);
    } else if (info_.installMode == InstallMode::LinuxAppImage) {
        const auto currentAppImage = appImagePath();
        if (currentAppImage.isEmpty()) {
            setFailure(tr("AppImage path is not available."));
            return;
        }
        QString writeReason;
        if (!isWritablePath(currentAppImage, &writeReason)) {
            setFailure(tr("AppImage file is not writable."), writeReason);
            return;
        }
        args << "appimage"
             << "--pid" << QString::number(QCoreApplication::applicationPid())
             << "--appimage-path" << currentAppImage
             << "--downloaded-path" << downloadedAssetPath_;
    } else {
        setFailure(tr("Automatic install mode is unsupported on this platform."));
        return;
    }

    const bool started = QProcess::startDetached(helper, args, QFileInfo(helper).absolutePath());
    if (!started) {
        setFailure(tr("Failed to start updater helper."), helper + "\n" + args.join(" "));
        return;
    }

    appendLog(QStringLiteral("Helper started: %1 %2").arg(helper, args.join(" ")));
    setState(State::Installing, tr("Updater started. Restarting application..."));
    emit requestApplicationExitForInstall();
}

bool UpdateService::verifySha256File(const QString &filePath, const QString &expectedHex, QString *actualHex) const {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (actualHex != nullptr) *actualHex = "<open-failed>";
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const auto chunk = file.read(1 << 20);
        if (!chunk.isEmpty()) hash.addData(chunk);
    }
    const QString actual = QString::fromLatin1(hash.result().toHex()).toLower();
    if (actualHex != nullptr) *actualHex = actual;
    return actual == expectedHex.trimmed().toLower();
}

QString UpdateService::findChecksumForAsset(const QByteArray &checksumsData, const QString &assetName) const {
    const QStringList lines = QString::fromUtf8(checksumsData).split('\n');
    const QString targetName = QFileInfo(assetName).fileName();
    const QRegularExpression defaultFormat("^\\s*([a-fA-F0-9]{64})\\s+[ *]?(.+?)\\s*$");
    const QRegularExpression bsdFormat("^\\s*SHA256\\s*\\((.+)\\)\\s*=\\s*([a-fA-F0-9]{64})\\s*$");

    for (const auto &lineRaw : lines) {
        const QString line = lineRaw.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        auto m = defaultFormat.match(line);
        if (m.hasMatch()) {
            const QString hash = m.captured(1).toLower();
            QString fileName = m.captured(2).trimmed();
            fileName = QFileInfo(fileName).fileName();
            if (fileName == targetName) return hash;
            continue;
        }

        m = bsdFormat.match(line);
        if (m.hasMatch()) {
            const QString fileName = QFileInfo(m.captured(1).trimmed()).fileName();
            const QString hash = m.captured(2).toLower();
            if (fileName == targetName) return hash;
        }
    }
    return {};
}

UpdateService::ReleaseInfo UpdateService::parseLatestReleaseJson(const QByteArray &payload, bool includePrerelease) const {
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(payload, &error);
    if (error.error != QJsonParseError::NoError) {
        appendLog(QStringLiteral("Release JSON parse error: %1").arg(error.errorString()));
        return {};
    }

    if (doc.isObject()) {
        auto info = releaseFromObject(doc.object());
        if (!includePrerelease && info.prerelease) {
            return {};
        }
        return info;
    }

    if (doc.isArray()) {
        const auto arr = doc.array();
        for (const auto &value : arr) {
            if (!value.isObject()) continue;
            auto info = releaseFromObject(value.toObject());
            if (!info.valid) continue;
            if (!includePrerelease && info.prerelease) continue;
            return info;
        }
    }
    return {};
}

UpdateService::ReleaseInfo UpdateService::releaseFromObject(const QJsonObject &obj) {
    ReleaseInfo release;
    if (obj.value("draft").toBool(false)) {
        return release;
    }
    release.versionTag = obj.value("tag_name").toString().trimmed();
    release.title = obj.value("name").toString().trimmed();
    release.notes = obj.value("body").toString();
    release.htmlUrl = QUrl(obj.value("html_url").toString().trimmed());
    release.prerelease = obj.value("prerelease").toBool(false);

    const auto assets = obj.value("assets").toArray();
    for (const auto &item : assets) {
        if (!item.isObject()) continue;
        const auto assetObj = item.toObject();
        ReleaseAsset asset;
        asset.name = assetObj.value("name").toString().trimmed();
        asset.url = QUrl(assetObj.value("browser_download_url").toString().trimmed());
        asset.size = static_cast<qint64>(assetObj.value("size").toDouble(-1));
        if (!asset.name.isEmpty() && asset.url.isValid()) {
            release.assets.push_back(asset);
        }
    }
    release.valid = !release.versionTag.isEmpty() && release.htmlUrl.isValid() && !release.assets.isEmpty();
    return release;
}

bool UpdateService::isTrustedReleaseAssetUrl(const QUrl &url) {
    if (!url.isValid()) return false;
    if (url.scheme() != "https") return false;
    if (url.host().compare("github.com", Qt::CaseInsensitive) != 0) return false;
    return url.toString().startsWith(QString::fromLatin1(kReleaseDownloadPrefix));
}

bool UpdateService::isTrustedReleasePageUrl(const QUrl &url) {
    if (!url.isValid()) return false;
    if (url.scheme() != "https") return false;
    if (url.host().compare("github.com", Qt::CaseInsensitive) != 0) return false;
    return url.path().startsWith("/cofedish/cofe-nekobox/releases");
}

QString UpdateService::normalizeVersion(const QString &version) {
    QString v = version.trimmed();
    if (v.startsWith('v', Qt::CaseInsensitive)) {
        v.remove(0, 1);
    }
    const int plusPos = v.indexOf('+');
    if (plusPos >= 0) {
        v = v.left(plusPos);
    }
    return v;
}

int UpdateService::compareVersions(const QString &lhs, const QString &rhs) {
    const auto splitVersion = [](const QString &input, QVersionNumber *core, QStringList *preParts) {
        QString cleaned = normalizeVersion(input);
        const auto dashPos = cleaned.indexOf('-');
        QString corePart = cleaned;
        QString prePart;
        if (dashPos >= 0) {
            corePart = cleaned.left(dashPos);
            prePart = cleaned.mid(dashPos + 1);
        }
        *core = QVersionNumber::fromString(corePart);
        *preParts = prePart.isEmpty() ? QStringList{} : prePart.split('.');
    };

    QVersionNumber leftCore;
    QVersionNumber rightCore;
    QStringList leftPre;
    QStringList rightPre;
    splitVersion(lhs, &leftCore, &leftPre);
    splitVersion(rhs, &rightCore, &rightPre);

    const int coreCmp = QVersionNumber::compare(leftCore, rightCore);
    if (coreCmp != 0) return coreCmp;

    if (leftPre.isEmpty() && rightPre.isEmpty()) return 0;
    if (leftPre.isEmpty()) return 1;
    if (rightPre.isEmpty()) return -1;

    const int n = qMin(leftPre.size(), rightPre.size());
    for (int i = 0; i < n; ++i) {
        bool leftNum = false;
        bool rightNum = false;
        const qlonglong l = leftPre.at(i).toLongLong(&leftNum);
        const qlonglong r = rightPre.at(i).toLongLong(&rightNum);
        if (leftNum && rightNum) {
            if (l < r) return -1;
            if (l > r) return 1;
            continue;
        }
        if (leftNum != rightNum) {
            return leftNum ? -1 : 1;
        }
        const int cmp = QString::compare(leftPre.at(i), rightPre.at(i), Qt::CaseInsensitive);
        if (cmp != 0) return cmp < 0 ? -1 : 1;
    }
    if (leftPre.size() == rightPre.size()) return 0;
    return leftPre.size() < rightPre.size() ? -1 : 1;
}

QString UpdateService::installModeName(InstallMode mode) {
    switch (mode) {
        case InstallMode::WindowsZip:
            return "windows-zip";
        case InstallMode::LinuxAppImage:
            return "linux-appimage";
        case InstallMode::LinuxDeb:
            return "linux-deb";
        case InstallMode::Unknown:
        default:
            return "unknown";
    }
}

void UpdateService::beginDebInstall() {
#ifndef Q_OS_LINUX
    setFailure(tr("Deb install is only supported on Linux."));
    return;
#else
    if (!Linux_HavePkexec()) {
        setFailure(tr("pkexec is required for .deb installation."),
                   tr("Install policykit-1 package and try again."));
        return;
    }
    if (downloadedAssetPath_.isEmpty()) {
        setFailure(tr("Downloaded .deb path is empty."));
        return;
    }

    debInstallProcess_ = new QProcess(this);
    debInstallProcess_->setProgram("pkexec");
    debInstallProcess_->setArguments({"apt", "install", "-y", downloadedAssetPath_});
    connect(debInstallProcess_, &QProcess::readyReadStandardOutput, this, [this] {
        appendLog(QString::fromUtf8(debInstallProcess_->readAllStandardOutput()).trimmed());
    });
    connect(debInstallProcess_, &QProcess::readyReadStandardError, this, [this] {
        appendLog(QString::fromUtf8(debInstallProcess_->readAllStandardError()).trimmed());
    });
    connect(debInstallProcess_,
            static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            this,
            [this](int code, QProcess::ExitStatus status) {
                finishDebInstall(code, status);
            });
    debInstallProcess_->start();
    if (!debInstallProcess_->waitForStarted(3000)) {
        const auto err = debInstallProcess_->errorString();
        debInstallProcess_->deleteLater();
        debInstallProcess_ = nullptr;
        setFailure(tr("Failed to start .deb installer."), err);
        return;
    }
    setState(State::Installing, tr("Installing .deb package (administrator access required)..."));
#endif
}

void UpdateService::finishDebInstall(int exitCode, QProcess::ExitStatus status) {
    QString detail;
    if (debInstallProcess_ != nullptr) {
        detail = QString::fromUtf8(debInstallProcess_->readAllStandardError()).trimmed();
        if (detail.isEmpty()) detail = QString::fromUtf8(debInstallProcess_->readAllStandardOutput()).trimmed();
        debInstallProcess_->deleteLater();
        debInstallProcess_ = nullptr;
    }
    if (status == QProcess::NormalExit && exitCode == 0) {
        appendLog("Deb install finished successfully.");
        setState(State::UpToDate, tr("Update installed successfully. Restart to apply changes."));
        emit restartSuggested();
        return;
    }
    setFailure(tr("Deb install failed."),
               joinDetail(QStringLiteral("Exit code: %1").arg(exitCode), detail));
}
