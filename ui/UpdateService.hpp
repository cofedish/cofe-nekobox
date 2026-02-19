#pragma once

#include <QByteArray>
#include <QObject>
#include <QList>
#include <QPointer>
#include <QProcess>
#include <QString>
#include <QUrl>

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
        Failed
    };
    Q_ENUM(State)

    enum class InstallMode {
        WindowsZip,
        LinuxAppImage,
        LinuxDeb,
        Unknown
    };
    Q_ENUM(InstallMode)

    struct UpdateInfo {
        QString currentVersion;
        QString latestVersion;
        QString releaseTitle;
        QString releaseNotes;
        QUrl releaseUrl;
        QString assetName;
        QUrl assetUrl;
        QUrl checksumsUrl;
        qint64 assetSize = -1;
        InstallMode installMode = InstallMode::Unknown;
        bool available = false;
        bool autoInstallSupported = false;
        QString autoInstallReason;
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
    void updateInfoChanged(const UpdateService::UpdateInfo &info);
    void restartSuggested();
    void requestApplicationExitForInstall();

private:
    enum class DownloadStage {
        None,
        Checksums,
        Asset
    };

    struct ReleaseAsset {
        QString name;
        QUrl url;
        qint64 size = -1;
    };

    struct ReleaseInfo {
        QString versionTag;
        QString title;
        QString notes;
        QUrl htmlUrl;
        bool prerelease = false;
        QList<ReleaseAsset> assets;
        bool valid = false;
    };

    void setState(State state, const QString &message = {});
    void setProgress(double progress01);
    void setFailure(const QString &shortMessage, const QString &detailMessage = {});
    void appendLog(const QString &line);

    InstallMode detectInstallMode(QString *reason) const;
    bool isAppImage() const;
    bool isWritablePath(const QString &path, QString *reason) const;
    bool isWritableDirectory(const QString &dir, QString *reason) const;
    bool hasDpkgPackageInstalled(const QString &packageName) const;
    QString updaterBinaryPath() const;
    QString appImagePath() const;

    QNetworkReply *sendGet(const QUrl &url);
    void handleCheckReply(QNetworkReply *reply);
    void handleChecksumsReply(QNetworkReply *reply);
    void handleAssetReplyFinished(QNetworkReply *reply);
    void beginAssetDownload();
    void finalizeVerifiedDownload();
    bool verifySha256File(const QString &filePath, const QString &expectedHex, QString *actualHex) const;
    QString findChecksumForAsset(const QByteArray &checksumsData, const QString &assetName) const;

    ReleaseInfo parseLatestReleaseJson(const QByteArray &payload, bool includePrerelease) const;
    static ReleaseInfo releaseFromObject(const QJsonObject &obj);
    static bool isTrustedReleaseAssetUrl(const QUrl &url);
    static bool isTrustedReleasePageUrl(const QUrl &url);
    static int compareVersions(const QString &lhs, const QString &rhs);
    static QString normalizeVersion(const QString &version);
    static QString installModeName(InstallMode mode);

    void beginDebInstall();
    void finishDebInstall(int exitCode, QProcess::ExitStatus status);

    QNetworkAccessManager *network_ = nullptr;
    QPointer<QNetworkReply> activeReply_;
    QPointer<QProcess> debInstallProcess_;
    QFile *downloadFile_ = nullptr;
    QString downloadTempDir_;
    QString downloadedAssetPath_;
    QString expectedSha256_;
    DownloadStage downloadStage_ = DownloadStage::None;

    State state_ = State::Idle;
    QString message_;
    QString details_;
    double progress_ = 0.0;
    bool lastCheckManual_ = false;
    UpdateInfo info_;
};

Q_DECLARE_METATYPE(UpdateService::UpdateInfo)
