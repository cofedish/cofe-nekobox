#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

struct HotspotDeviceInfo {
    QString ip;
    QString mac;
    QString hostname;
    QDateTime lastSeen;
    quint64 rxBytes = 0;
    quint64 txBytes = 0;
};

struct HotspotRuntimeInfo {
    bool active = false;
    QString ssid;
    QString password;
    QString apIf;
    QString apCidr;
    QString gwIp;
    QString uplinkIf;
    QString tunIf;
};

class HotspotManager {
public:
    virtual ~HotspotManager() = default;
    virtual bool start(const QString &ssid, const QString &password, HotspotRuntimeInfo *info, QString *error) = 0;
    virtual bool stop(const HotspotRuntimeInfo &info, QString *error) = 0;
    virtual bool status(HotspotRuntimeInfo *info, QString *error) = 0;
};

class DeviceManager {
public:
    virtual ~DeviceManager() = default;
    virtual void startWatch(const QString &apIf, const QString &apCidr) = 0;
    virtual void stopWatch() = 0;
    virtual QVector<HotspotDeviceInfo> listDevices(QString *error) = 0;
};

class TrafficRouter {
public:
    virtual ~TrafficRouter() = default;
    virtual bool applyFullTunnel(const HotspotRuntimeInfo &info, QString *error) = 0;
    virtual bool clear(const HotspotRuntimeInfo &info, QString *error) = 0;
};

class HotspotDiagnostics {
public:
    virtual ~HotspotDiagnostics() = default;
    virtual QString run(const HotspotRuntimeInfo &info, const QVector<HotspotDeviceInfo> &devices, bool *ok) = 0;
};

class QTimer;

class HotspotGatewayService final : public QObject {
    Q_OBJECT
public:
    enum class State {
        Idle,
        Starting,
        Running,
        Stopping,
        Failed
    };
    Q_ENUM(State)

    enum class Mode {
        FullTunnelForHotspot
    };
    Q_ENUM(Mode)

    explicit HotspotGatewayService(QObject *parent = nullptr);
    ~HotspotGatewayService() override;

    void setCredentials(const QString &ssid, const QString &password);
    void regenerateCredentials();

    bool start(Mode mode = Mode::FullTunnelForHotspot);
    void stop();
    void runDiagnostics();

    [[nodiscard]] State state() const;
    [[nodiscard]] Mode mode() const;
    [[nodiscard]] HotspotRuntimeInfo runtime() const;
    [[nodiscard]] QVector<HotspotDeviceInfo> devices() const;
    [[nodiscard]] QString lastMessage() const;
    [[nodiscard]] QString maskedPassword() const;
    [[nodiscard]] QString wifiQrText() const;

signals:
    void stateChanged(HotspotGatewayService::State state, const QString &message);
    void devicesChanged(const QVector<HotspotDeviceInfo> &devices);
    void diagReport(bool ok, const QString &report);
    void credentialsChanged(const QString &ssid, const QString &passwordMasked);

private:
    void setState(State state, const QString &message);
    void pollDevices();
    void ensureCredentials();

    State state_ = State::Idle;
    Mode mode_ = Mode::FullTunnelForHotspot;
    QString lastMessage_;
    HotspotRuntimeInfo runtime_;
    QVector<HotspotDeviceInfo> devices_;

    std::unique_ptr<HotspotManager> hotspotManager_;
    std::unique_ptr<DeviceManager> deviceManager_;
    std::unique_ptr<TrafficRouter> trafficRouter_;
    std::unique_ptr<HotspotDiagnostics> diagnostics_;
    QTimer *devicePollTimer_ = nullptr;
};
