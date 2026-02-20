#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QProcess>

class QFile;
class QJsonObject;
class QNetworkAccessManager;
class QNetworkReply;

class UpdateService : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Checking,
        UpdateAvailable,
        UpToDate,
        Downloading,
        Verifying,
        Installing,
        Failed,
    };
    Q_ENUM(State)

    enum class InstallMode {
        WindowsZip,
        LinuxAppImage,
        LinuxDeb,
        Unknown,
    };
    Q_ENUM(InstallMode)

    struct UpdateInfo {
        QString currentVersion;
        QString latestVersion;
        QString releaseTitle;
        QString releaseNotes;
        QString assetName;
        QUrl releaseUrl;
        QUrl assetUrl;
        QUrl checksumsUrl;
        qint64 assetSize = -1;
        InstallMode installMode = InstallMode::Unknown;
        QString autoInstallReason;
        bool available = false;
        bool autoInstallSupported = false;
        bool isPrerelease = false;
    };

    explicit UpdateService(QObject *parent = nullptr);
    ~UpdateService() override;

    State state() const;
    UpdateInfo info() const;
    QString message() const;
    QString details() const;
    QString logFilePath() const;
    bool lastCheckWasManual() const;

public slots:
    void checkForUpdates(bool manual = false);
    void startUpdate();
    void cancelUpdate();

signals:
    void stateChanged(UpdateService::State state);
    void progressChanged(double progress01);
    void messageChanged(const QString &message);
    void updateInfoChanged();
    void requestApplicationExitForInstall();
    void restartSuggested();

private:
    struct ReleaseAsset {
        QString name;
        QUrl url;
        qint64 size = -1;
    };

    struct ReleaseInfo {
        bool valid = false;
        QString versionTag;
        QString title;
        QString notes;
        QUrl htmlUrl;
        bool prerelease = false;
        QVector<ReleaseAsset> assets;
    };

    enum class DownloadStage {
        None,
        Checksums,
        Asset,
    };

    void setState(State state, const QString &message = {});
    void setProgress(double progress01);
    void setFailure(const QString &shortMessage, const QString &detailMessage = {});
    void appendLog(const QString &line) const;

    InstallMode detectInstallMode(QString *reason = nullptr) const;
    bool isAppImage() const;
    bool isWritablePath(const QString &path, QString *reason = nullptr) const;
    bool isWritableDirectory(const QString &dirPath, QString *reason = nullptr) const;
    bool hasDpkgPackageInstalled(const QString &packageName) const;
    QString updaterBinaryPath() const;
    QString appImagePath() const;

    QNetworkReply *sendGet(const QUrl &url);
    void handleCheckReply(QNetworkReply *reply);
    void handleChecksumsReply(QNetworkReply *reply);
    void beginAssetDownload();
    void handleAssetReplyFinished(QNetworkReply *reply);
    void finalizeVerifiedDownload();
    bool verifySha256File(const QString &filePath, const QString &expectedHex, QString *actualHex = nullptr) const;
    QString findChecksumForAsset(const QByteArray &checksumsData, const QString &assetName) const;

    ReleaseInfo parseLatestReleaseJson(const QByteArray &payload, bool includePrerelease) const;
    static ReleaseInfo releaseFromObject(const QJsonObject &obj);
    static bool isTrustedReleaseAssetUrl(const QUrl &url);
    static bool isTrustedReleasePageUrl(const QUrl &url);
    static QString normalizeVersion(const QString &version);
    static int compareVersions(const QString &lhs, const QString &rhs);
    static QString installModeName(InstallMode mode);

    void beginDebInstall();
    void finishDebInstall(int exitCode, QProcess::ExitStatus status);

private:
    State state_ = State::Idle;
    double progress_ = 0.0;
    QString message_;
    QString details_;
    UpdateInfo info_;
    bool lastCheckManual_ = false;
    DownloadStage downloadStage_ = DownloadStage::None;

    QNetworkAccessManager *network_ = nullptr;
    QNetworkReply *activeReply_ = nullptr;
    QFile *downloadFile_ = nullptr;
    QProcess *debInstallProcess_ = nullptr;

    QString expectedSha256_;
    QString downloadTempDir_;
    QString downloadedAssetPath_;
};
