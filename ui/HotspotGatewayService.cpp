#include "ui/HotspotGatewayService.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHostAddress>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QTimer>

#ifdef Q_OS_WIN
#include "3rdparty/WinCommander.hpp"
#include "sys/windows/guihelper.h"
#endif

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace {

struct CommandResult {
    bool started = false;
    int exitCode = -1;
    QString out;
    QString err;
};

QString decodeBytes(const QByteArray &bytes) {
    if (bytes.isEmpty()) return {};
    const auto utf8 = QString::fromUtf8(bytes.constData(), bytes.size());
    if (!utf8.contains(QChar::ReplacementCharacter)) return utf8;
    const auto local = QString::fromLocal8Bit(bytes.constData(), bytes.size());
    if (!local.isEmpty()) return local;
    return utf8;
}

CommandResult runCommand(const QString &program, const QStringList &args, int timeoutMs = 10000) {
    QProcess p;
    p.setProgram(program);
    p.setArguments(args);
    CommandResult result;
    p.start();
    result.started = p.waitForStarted(3000);
    if (!result.started) {
        result.err = p.errorString();
        return result;
    }
    p.waitForFinished(timeoutMs);
    if (p.state() != QProcess::NotRunning) {
        p.kill();
        p.waitForFinished(1000);
    }
    result.exitCode = p.exitCode();
    result.out = decodeBytes(p.readAllStandardOutput()).trimmed();
    result.err = decodeBytes(p.readAllStandardError()).trimmed();
    return result;
}

bool looksLikeInterfaceName(const QString &value) {
#ifdef Q_OS_WIN
    if (value.trimmed().isEmpty() || value.size() > 128) return false;
    return !value.contains('\r') && !value.contains('\n');
#else
    static const QRegularExpression re("^[A-Za-z0-9_:\\.-]{1,64}$");
    return re.match(value).hasMatch();
#endif
}

QString appHelperPath() {
#ifdef Q_OS_WIN
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("cofebox-net-helper.exe");
#else
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("cofebox-net-helper");
#endif
}

QString maskPassword(const QString &password) {
    if (password.isEmpty()) return {};
    if (password.size() <= 2) return QString(password.size(), '*');
    return QString(password.size() - 2, '*') + password.right(2);
}

QString randomAlphaNum(int length) {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
    QString out;
    out.reserve(length);
    for (int i = 0; i < length; ++i) {
        const int idx = QRandomGenerator::global()->bounded(static_cast<int>(sizeof(chars) - 1));
        out.append(QChar(chars[idx]));
    }
    return out;
}

QString randomSsidSuffix() {
    const auto value = QRandomGenerator::global()->bounded(0x10000);
    return QString("%1").arg(value, 4, 16, QLatin1Char('0')).toUpper();
}

QString tunDefaultName() {
#ifdef Q_OS_WIN
    return QStringLiteral("cofebox-tun");
#else
    return QStringLiteral("cofebox-tun");
#endif
}

QString firstUsefulLine(const QString &text) {
    const auto lines = text.split('\n');
    for (const auto &raw : lines) {
        const auto line = raw.trimmed();
        if (line.isEmpty()) continue;
        if (line.startsWith('+') || line.startsWith("~")) continue;
        if (line.startsWith("CategoryInfo", Qt::CaseInsensitive)) continue;
        if (line.startsWith("FullyQualifiedErrorId", Qt::CaseInsensitive)) continue;
        return line;
    }
    return text.trimmed();
}

#ifdef Q_OS_LINUX
bool interfaceExistsLinux(const QString &ifName) {
    if (!looksLikeInterfaceName(ifName)) return false;
    const auto r = runCommand("ip", {"link", "show", "dev", ifName});
    return r.exitCode == 0;
}
#endif

// ----------------- Linux -----------------

#ifdef Q_OS_LINUX

bool parseIpv4OnInterface(const QString &ifName, QString *cidr, QString *ip) {
    const auto r = runCommand("ip", {"-4", "-o", "addr", "show", "dev", ifName});
    if (r.exitCode != 0) return false;
    QRegularExpression re("inet\\s+([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+/[0-9]+)");
    auto m = re.match(r.out);
    if (!m.hasMatch()) return false;
    const auto full = m.captured(1);
    if (cidr != nullptr) *cidr = full;
    if (ip != nullptr) *ip = full.section('/', 0, 0);
    return true;
}

QString detectUplinkLinux() {
    const auto r = runCommand("ip", {"route", "show", "default"});
    if (r.exitCode != 0) return {};
    QRegularExpression re1("default\\s+via\\s+\\S+\\s+dev\\s+(\\S+)");
    auto m1 = re1.match(r.out);
    if (m1.hasMatch()) return m1.captured(1).trimmed();
    QRegularExpression re2("default\\s+dev\\s+(\\S+)");
    auto m2 = re2.match(r.out);
    if (m2.hasMatch()) return m2.captured(1).trimmed();
    return {};
}

QString hotspotConnectionNameLinux() {
    return QStringLiteral("CofeBox Hotspot");
}

QStringList hotspotConnectionAliasesLinux() {
    return {
        hotspotConnectionNameLinux(),
        QStringLiteral("cofebox-hotspot") // legacy id
    };
}

struct NmDeviceInfo {
    QString dev;
    QString type;
    QString state;
};

QVector<NmDeviceInfo> listNmDevicesLinux() {
    QVector<NmDeviceInfo> devices;
    const auto r = runCommand("nmcli", {"-t", "-f", "DEVICE,TYPE,STATE", "device", "status"});
    if (r.exitCode != 0) return devices;
    for (const auto &line : r.out.split('\n')) {
        const auto parts = line.split(':');
        if (parts.size() < 3) continue;
        const auto dev = parts.at(0).trimmed();
        if (!looksLikeInterfaceName(dev)) continue;
        NmDeviceInfo info;
        info.dev = dev;
        info.type = parts.at(1).trimmed();
        info.state = parts.at(2).trimmed();
        devices.push_back(info);
    }
    return devices;
}

bool isWiFiInterfaceLinux(const QString &ifName) {
    if (ifName.trimmed().isEmpty()) return false;
    for (const auto &device : listNmDevicesLinux()) {
        if (device.dev == ifName && device.type == "wifi") return true;
    }
    return false;
}

QString selectHotspotInterfaceLinux(const QString &uplinkIf, QString *error) {
    QString fallbackDisconnected;
    QString fallbackAny;
    bool haveWifi = false;
    int connectedWifiCount = 0;
    for (const auto &device : listNmDevicesLinux()) {
        if (device.type != "wifi") continue;
        haveWifi = true;
        const bool isConnected = device.state.contains("connected", Qt::CaseInsensitive);
        if (isConnected) connectedWifiCount++;
        if (fallbackAny.isEmpty()) fallbackAny = device.dev;
        if (!isConnected && fallbackDisconnected.isEmpty()) fallbackDisconnected = device.dev;
        if (!uplinkIf.isEmpty() && device.dev == uplinkIf) continue;
        if (!isConnected) return device.dev;
    }

    if (!haveWifi) {
        if (error != nullptr) {
            *error = QObject::tr("Wi-Fi adapter does not support hotspot mode.");
        }
        return {};
    }

    if (!uplinkIf.isEmpty() && isWiFiInterfaceLinux(uplinkIf)) {
        if (error != nullptr) {
            *error = QObject::tr("Only one Wi-Fi adapter is available and it is already used for internet uplink (%1).\n"
                                 "Hotspot would disconnect your current Wi-Fi. Use Ethernet or a second Wi-Fi adapter.")
                         .arg(uplinkIf);
        }
        return {};
    }

    if (fallbackDisconnected.isEmpty() && connectedWifiCount > 0) {
        if (error != nullptr) {
            *error = QObject::tr("No free Wi-Fi adapter for hotspot. Your active Wi-Fi connection is in use.\n"
                                 "Use Ethernet or connect a second Wi-Fi adapter for hotspot mode.");
        }
        return {};
    }

    return fallbackDisconnected.isEmpty() ? fallbackAny : fallbackDisconnected;
}

QString detectWiFiInterfaceLinux() {
    for (const auto &device : listNmDevicesLinux()) {
        if (device.type != "wifi") continue;
        if (looksLikeInterfaceName(device.dev)) return device.dev;
    }
    return {};
}

class HotspotManagerLinux final : public HotspotManager {
public:
    bool start(const QString &ssid, const QString &password, HotspotRuntimeInfo *info, QString *error) override {
        const auto uplinkBefore = detectUplinkLinux();
        const auto ifName = selectHotspotInterfaceLinux(uplinkBefore, error);
        if (ifName.isEmpty()) {
            if (error != nullptr && error->trimmed().isEmpty()) {
                *error = QObject::tr("Wi-Fi adapter does not support hotspot mode.");
            }
            return false;
        }

        for (const auto &alias : hotspotConnectionAliasesLinux()) {
            runCommand("nmcli", {"connection", "delete", alias});
        }

        auto startRes = runCommand("nmcli",
                                   {"device", "wifi", "hotspot", "ifname", ifName, "con-name", hotspotConnectionNameLinux(),
                                    "ssid", ssid, "password", password},
                                   20000);
        if (startRes.exitCode != 0) {
            if (error != nullptr) {
                *error = QObject::tr("Failed to start hotspot via NetworkManager.\n%1")
                             .arg(startRes.err.isEmpty() ? startRes.out : startRes.err);
            }
            return false;
        }

        QString cidr;
        QString ip;
        if (!parseIpv4OnInterface(ifName, &cidr, &ip)) {
            // NetworkManager default shared subnet
            cidr = QStringLiteral("10.42.0.1/24");
            ip = QStringLiteral("10.42.0.1");
        }

        info->active = true;
        info->apIf = ifName;
        info->apCidr = cidr;
        info->gwIp = ip;
        info->uplinkIf = uplinkBefore.isEmpty() ? detectUplinkLinux() : uplinkBefore;
        return true;
    }

    bool stop(const HotspotRuntimeInfo &, QString *error) override {
        int lastExit = 0;
        QString lastErr;
        for (const auto &alias : hotspotConnectionAliasesLinux()) {
            auto down = runCommand("nmcli", {"connection", "down", alias});
            runCommand("nmcli", {"connection", "delete", alias});
            if (down.exitCode != 0) {
                lastExit = down.exitCode;
                if (!down.err.trimmed().isEmpty()) {
                    lastErr = down.err;
                }
            }
        }
        if (lastExit != 0 && !lastErr.contains("unknown", Qt::CaseInsensitive)) {
            if (error != nullptr) *error = lastErr;
        }
        return true;
    }

    bool status(HotspotRuntimeInfo *info, QString *) override {
        const auto r = runCommand("nmcli", {"-t", "-f", "NAME,DEVICE,TYPE", "connection", "show", "--active"});
        if (r.exitCode != 0) return false;
        const auto names = hotspotConnectionAliasesLinux();
        for (const auto &line : r.out.split('\n')) {
            const auto parts = line.split(':');
            if (parts.size() < 3) continue;
            if (!names.contains(parts.at(0).trimmed())) continue;
            const auto dev = parts.at(1).trimmed();
            if (!looksLikeInterfaceName(dev)) continue;
            info->active = true;
            info->apIf = dev;
            parseIpv4OnInterface(dev, &info->apCidr, &info->gwIp);
            info->uplinkIf = detectUplinkLinux();
            return true;
        }
        return false;
    }
};

class DeviceManagerLinux final : public DeviceManager {
public:
    void startWatch(const QString &apIf, const QString &apCidr) override {
        apIf_ = apIf;
        apCidr_ = apCidr;
    }

    void stopWatch() override {
        apIf_.clear();
        apCidr_.clear();
    }

    QVector<HotspotDeviceInfo> listDevices(QString *error) override {
        QVector<HotspotDeviceInfo> out;
        if (apIf_.isEmpty()) return out;
        const auto r = runCommand("ip", {"neigh", "show", "dev", apIf_});
        if (r.exitCode != 0) {
            if (error != nullptr) *error = r.err;
            return out;
        }
        QRegularExpression re("^([0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+)\\s+.*lladdr\\s+([0-9a-fA-F:]{17})\\s+(\\S+)");
        for (const auto &lineRaw : r.out.split('\n')) {
            const auto line = lineRaw.trimmed();
            if (line.isEmpty()) continue;
            const auto m = re.match(line);
            if (!m.hasMatch()) continue;
            HotspotDeviceInfo d;
            d.ip = m.captured(1).trimmed();
            d.mac = m.captured(2).trimmed().toLower();
            d.hostname.clear();
            d.lastSeen = QDateTime::currentDateTime();
            out.push_back(d);
        }
        return out;
    }

private:
    QString apIf_;
    QString apCidr_;
};

class TrafficRouterLinux final : public TrafficRouter {
public:
    bool applyFullTunnel(const HotspotRuntimeInfo &info, QString *error) override {
        const auto helper = appHelperPath();
        if (!QFileInfo::exists(helper)) {
            if (error != nullptr) *error = QObject::tr("Network helper is missing.");
            return false;
        }
        if (!looksLikeInterfaceName(info.apIf) || !looksLikeInterfaceName(info.tunIf) || info.apCidr.isEmpty()) {
            if (error != nullptr) *error = QObject::tr("Invalid hotspot interfaces for routing.");
            return false;
        }
        if (!interfaceExistsLinux(info.tunIf)) {
            if (error != nullptr) {
                *error = QObject::tr("TUN interface \"%1\" was not found. Start proxy in TUN mode first.").arg(info.tunIf);
            }
            return false;
        }
        if (!interfaceExistsLinux(info.apIf)) {
            if (error != nullptr) {
                *error = QObject::tr("Hotspot interface \"%1\" was not found.").arg(info.apIf);
            }
            return false;
        }

        QStringList args = {
            helper,
            "apply-linux",
            "--ap-if", info.apIf,
            "--ap-cidr", info.apCidr,
            "--tun-if", info.tunIf,
        };
        auto r = runCommand("pkexec", args, 20000);
        if (r.exitCode != 0) {
            if (error != nullptr) {
                *error = QObject::tr("Administrator rights are required to apply hotspot routing.\n%1")
                             .arg(r.err.isEmpty() ? r.out : r.err);
            }
            return false;
        }
        return true;
    }

    bool clear(const HotspotRuntimeInfo &info, QString *error) override {
        const auto helper = appHelperPath();
        if (!QFileInfo::exists(helper)) return true;

        QStringList args = {
            helper,
            "clear-linux",
            "--ap-if", info.apIf,
            "--ap-cidr", info.apCidr,
            "--tun-if", info.tunIf,
        };
        auto r = runCommand("pkexec", args, 15000);
        if (r.exitCode != 0 && error != nullptr) {
            *error = r.err.isEmpty() ? r.out : r.err;
        }
        return r.exitCode == 0;
    }
};

class DiagnosticsLinux final : public HotspotDiagnostics {
public:
    QString run(const HotspotRuntimeInfo &info, const QVector<HotspotDeviceInfo> &devices, bool *ok) override {
        QStringList lines;
        bool success = true;

        auto addCheck = [&](const QString &name, bool pass, const QString &detail) {
            lines << QStringLiteral("[%1] %2: %3").arg(pass ? "OK" : "FAIL", name, detail);
            if (!pass) success = false;
        };

        if (info.apIf.trimmed().isEmpty()) {
            if (ok != nullptr) *ok = false;
            return QObject::tr("Hotspot is not running. Start hotspot first.");
        }
        if (info.tunIf.trimmed().isEmpty()) {
            if (ok != nullptr) *ok = false;
            return QObject::tr("TUN interface is not set. Enable TUN mode first.");
        }

        const auto ap = runCommand("ip", {"-4", "addr", "show", "dev", info.apIf});
        addCheck(QObject::tr("Hotspot interface"),
                 ap.exitCode == 0 && ap.out.contains(info.gwIp),
                 ap.exitCode == 0 ? info.apIf + " " + info.gwIp : firstUsefulLine(ap.err));

        const auto tun = runCommand("ip", {"link", "show", info.tunIf});
        addCheck(QObject::tr("TUN interface"),
                 tun.exitCode == 0,
                 tun.exitCode == 0 ? info.tunIf : firstUsefulLine(tun.err));

        const auto rules = runCommand("ip", {"rule", "show"});
        addCheck(QObject::tr("Policy rule"),
                 rules.exitCode == 0 && rules.out.contains("fwmark 0x1") && rules.out.contains("lookup 100"),
                 rules.exitCode == 0 ? "fwmark 0x1 -> table 100" : firstUsefulLine(rules.err));

        const auto rt = runCommand("ip", {"route", "show", "table", "100"});
        addCheck(QObject::tr("Routing table 100"),
                 rt.exitCode == 0 && rt.out.contains("default") && rt.out.contains(info.tunIf),
                 rt.exitCode == 0 ? rt.out : firstUsefulLine(rt.err));

#ifdef Q_OS_LINUX
        if (geteuid() != 0) {
            lines << QStringLiteral("[OK] %1: %2")
                         .arg(QObject::tr("NAT"),
                              QObject::tr("NAT check requires admin rights (run diagnostics as root)."));
        } else {
            const auto nat = runCommand("iptables", {"-t", "nat", "-S", "POSTROUTING"});
            addCheck(QObject::tr("NAT"),
                     nat.exitCode == 0 && nat.out.contains("MASQUERADE") && nat.out.contains(info.tunIf),
                     nat.exitCode == 0 ? "MASQUERADE via " + info.tunIf : firstUsefulLine(nat.err));
        }
#endif

        lines << QObject::tr("Connected devices: %1").arg(devices.size());

        if (ok != nullptr) *ok = success;
        return lines.join('\n');
    }
};

#endif // Q_OS_LINUX

// ----------------- Windows -----------------

#ifdef Q_OS_WIN

CommandResult runPowerShell(QString script, int timeoutMs = 15000) {
    script.prepend("[Console]::OutputEncoding=[System.Text.Encoding]::UTF8; ");
    return runCommand("powershell", {"-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script}, timeoutMs);
}

bool interfaceExistsWindows(const QString &ifName) {
    if (!looksLikeInterfaceName(ifName)) return false;
    QString escaped = ifName;
    escaped.replace("'", "''");
    const auto r = runPowerShell(QStringLiteral(
        "$name='%1'; "
        "$a=Get-NetAdapter -IncludeHidden | Where-Object { $_.Name -eq $name -or $_.InterfaceAlias -eq $name } "
        "| Select-Object -First 1 -ExpandProperty Name; "
        "if($a){$a}")
                                     .arg(escaped));
    return r.exitCode == 0 && !r.out.trimmed().isEmpty();
}

bool parseWindowsApInfo(const QString &line, QString *ifName, QString *cidr, QString *ip) {
    const auto parts = line.split('|');
    if (parts.size() < 3) return false;
    const auto name = parts.at(0).trimmed();
    const auto c = parts.at(1).trimmed();
    const auto g = parts.at(2).trimmed();
    if (name.isEmpty() || c.isEmpty() || g.isEmpty()) return false;
    if (ifName != nullptr) *ifName = name;
    if (cidr != nullptr) *cidr = c;
    if (ip != nullptr) *ip = g;
    return true;
}

QString detectUplinkWindows() {
    const auto r = runPowerShell("(Get-NetRoute -DestinationPrefix '0.0.0.0/0' | Sort-Object RouteMetric,ifMetric | Select-Object -First 1 -ExpandProperty InterfaceAlias)");
    return r.exitCode == 0 ? r.out.trimmed() : QString{};
}

class HotspotManagerWindows final : public HotspotManager {
public:
    bool start(const QString &ssid, const QString &password, HotspotRuntimeInfo *info, QString *error) override {
        runCommand("netsh", {"wlan", "set", "hostednetwork", "mode=allow", "ssid=" + ssid, "key=" + password});
        runCommand("netsh", {"wlan", "start", "hostednetwork"});

        auto detect = runPowerShell(
            "$cfg = Get-NetIPConfiguration | Where-Object { $_.IPv4Address -and ("
            "$_.IPv4Address.IPAddress -like '192.168.137.*' -or "
            "$_.InterfaceAlias -like 'Local Area Connection*' -or "
            "$_.InterfaceDescription -match 'Wi-Fi Direct|Wireless|Hosted|Virtual') } "
            "| Sort-Object { if ($_.IPv4Address.IPAddress -like '192.168.137.*') { 0 } else { 1 } } "
            "| Select-Object -First 1;"
            "if ($cfg) { \"$($cfg.InterfaceAlias)|$($cfg.IPv4Address.IPAddress)/$($cfg.IPv4Address.PrefixLength)|$($cfg.IPv4Address.IPAddress)\" }");

        QString apIf;
        QString apCidr;
        QString gwIp;
        if (!parseWindowsApInfo(detect.out.trimmed(), &apIf, &apCidr, &gwIp)) {
            if (error != nullptr) {
                *error = QObject::tr("Mobile Hotspot is not active. Enable it in Windows settings, then click Start again.");
            }
            return false;
        }
        info->active = true;
        info->apIf = apIf;
        info->apCidr = apCidr;
        info->gwIp = gwIp;
        info->uplinkIf = detectUplinkWindows();
        return true;
    }

    bool stop(const HotspotRuntimeInfo &, QString *) override {
        runCommand("netsh", {"wlan", "stop", "hostednetwork"});
        return true;
    }

    bool status(HotspotRuntimeInfo *info, QString *) override {
        auto detect = runPowerShell(
            "$cfg = Get-NetIPConfiguration | Where-Object { $_.IPv4Address -and ("
            "$_.IPv4Address.IPAddress -like '192.168.137.*' -or "
            "$_.InterfaceAlias -like 'Local Area Connection*' -or "
            "$_.InterfaceDescription -match 'Wi-Fi Direct|Wireless|Hosted|Virtual') } "
            "| Sort-Object { if ($_.IPv4Address.IPAddress -like '192.168.137.*') { 0 } else { 1 } } "
            "| Select-Object -First 1;"
            "if ($cfg) { \"$($cfg.InterfaceAlias)|$($cfg.IPv4Address.IPAddress)/$($cfg.IPv4Address.PrefixLength)|$($cfg.IPv4Address.IPAddress)\" }");
        QString apIf;
        QString apCidr;
        QString gwIp;
        if (!parseWindowsApInfo(detect.out.trimmed(), &apIf, &apCidr, &gwIp)) return false;
        info->active = true;
        info->apIf = apIf;
        info->apCidr = apCidr;
        info->gwIp = gwIp;
        info->uplinkIf = detectUplinkWindows();
        return true;
    }
};

class DeviceManagerWindows final : public DeviceManager {
public:
    void startWatch(const QString &apIf, const QString &apCidr) override {
        apIf_ = apIf;
        apCidr_ = apCidr;
    }

    void stopWatch() override {
        apIf_.clear();
        apCidr_.clear();
    }

    QVector<HotspotDeviceInfo> listDevices(QString *error) override {
        QVector<HotspotDeviceInfo> out;
        if (apIf_.isEmpty()) return out;

        QString escapedApIf = apIf_;
        escapedApIf.replace("'", "''");
        const QString script = QStringLiteral(
            "$items = Get-NetNeighbor -InterfaceAlias '%1' -AddressFamily IPv4 | Where-Object { $_.LinkLayerAddress -and $_.State -ne 'Unreachable' };"
            "$items | ForEach-Object { \"$($_.IPAddress)|$($_.LinkLayerAddress)|$($_.State)\" }")
                                   .arg(escapedApIf);
        const auto r = runPowerShell(script);
        if (r.exitCode != 0) {
            if (error != nullptr) *error = r.err;
            return out;
        }
        for (const auto &lineRaw : r.out.split('\n')) {
            const auto line = lineRaw.trimmed();
            if (line.isEmpty()) continue;
            const auto parts = line.split('|');
            if (parts.size() < 2) continue;
            HotspotDeviceInfo d;
            d.ip = parts.at(0).trimmed();
            d.mac = parts.at(1).trimmed().toLower();
            d.hostname.clear();
            d.lastSeen = QDateTime::currentDateTime();
            out.push_back(d);
        }
        return out;
    }

private:
    QString apIf_;
    QString apCidr_;
};

class TrafficRouterWindows final : public TrafficRouter {
public:
    bool applyFullTunnel(const HotspotRuntimeInfo &info, QString *error) override {
        const auto helper = appHelperPath();
        if (!QFileInfo::exists(helper)) {
            if (error != nullptr) *error = QObject::tr("Network helper is missing.");
            return false;
        }
        if (!interfaceExistsWindows(info.tunIf)) {
            if (error != nullptr) {
                *error = QObject::tr("TUN interface \"%1\" was not found. Start proxy in TUN mode first.").arg(info.tunIf);
            }
            return false;
        }
        if (!interfaceExistsWindows(info.apIf)) {
            if (error != nullptr) {
                *error = QObject::tr("Hotspot interface \"%1\" was not found. Enable Mobile Hotspot first.").arg(info.apIf);
            }
            return false;
        }
        QStringList args = {"apply-windows-ics", "--public-if", info.tunIf, "--private-if", info.apIf};
        return runHelperWithElevation(helper, args, error);
    }

    bool clear(const HotspotRuntimeInfo &info, QString *error) override {
        const auto helper = appHelperPath();
        if (!QFileInfo::exists(helper)) return true;
        QStringList args = {"clear-windows-ics", "--public-if", info.tunIf, "--private-if", info.apIf};
        return runHelperWithElevation(helper, args, error);
    }

private:
    bool runHelperWithElevation(const QString &helper, const QStringList &args, QString *error) {
        if (Windows_IsInAdmin()) {
            const auto r = runCommand(helper, args, 20000);
            if (r.exitCode == 0) return true;
            if (error != nullptr) *error = r.err.isEmpty() ? r.out : r.err;
            return false;
        }

        const auto code = WinCommander::runProcessElevated(helper, args, QCoreApplication::applicationDirPath(), WinCommander::SW_HIDE, true);
        if (code != 0) {
            if (error != nullptr) {
                *error = QObject::tr("Administrator rights are required to configure ICS.");
            }
            return false;
        }
        return true;
    }
};

class DiagnosticsWindows final : public HotspotDiagnostics {
public:
    QString run(const HotspotRuntimeInfo &info, const QVector<HotspotDeviceInfo> &devices, bool *ok) override {
        QStringList lines;
        bool success = true;

        auto addCheck = [&](const QString &name, bool pass, const QString &detail) {
            lines << QStringLiteral("[%1] %2: %3").arg(pass ? "OK" : "FAIL", name, detail);
            if (!pass) success = false;
        };

        if (info.apIf.trimmed().isEmpty()) {
            if (ok != nullptr) *ok = false;
            return QObject::tr("Hotspot is not running. Start Mobile Hotspot first.");
        }
        if (info.tunIf.trimmed().isEmpty()) {
            if (ok != nullptr) *ok = false;
            return QObject::tr("TUN interface is not set. Enable TUN mode first.");
        }

        QString escapedApIf = info.apIf;
        escapedApIf.replace("'", "''");
        const auto ap = runPowerShell(QStringLiteral("Get-NetIPAddress -InterfaceAlias '%1' -AddressFamily IPv4 | Select-Object -First 1 -ExpandProperty IPAddress")
                                          .arg(escapedApIf));
        addCheck(QObject::tr("Hotspot interface"),
                 ap.exitCode == 0 && !ap.out.trimmed().isEmpty(),
                 ap.exitCode == 0 ? ap.out.trimmed() : firstUsefulLine(ap.err));

        QString escapedTunIf = info.tunIf;
        escapedTunIf.replace("'", "''");
        const auto tun = runPowerShell(QStringLiteral("Get-NetAdapter -Name '%1' | Select-Object -ExpandProperty Status")
                                           .arg(escapedTunIf));
        addCheck(QObject::tr("TUN interface"),
                 tun.exitCode == 0 && tun.out.contains("Up", Qt::CaseInsensitive),
                 tun.exitCode == 0 ? tun.out.trimmed() : firstUsefulLine(tun.err));

        const auto helper = appHelperPath();
        if (QFileInfo::exists(helper)) {
            QStringList args = {"diag-windows-ics", "--public-if", info.tunIf, "--private-if", info.apIf};
            CommandResult ics;
            if (Windows_IsInAdmin()) {
                ics = runCommand(helper, args, 15000);
            } else {
                const auto code = WinCommander::runProcessElevated(helper, args, QCoreApplication::applicationDirPath(), WinCommander::SW_HIDE, true);
                ics.started = true;
                ics.exitCode = static_cast<int>(code);
                if (code != 0) ics.err = QObject::tr("Failed to verify ICS in elevated helper.");
            }
            addCheck(QObject::tr("ICS sharing"), ics.exitCode == 0, ics.exitCode == 0 ? QStringLiteral("public=%1 private=%2").arg(info.tunIf, info.apIf)
                                                                                       : firstUsefulLine(ics.err.isEmpty() ? ics.out : ics.err));
        } else {
            addCheck(QObject::tr("ICS sharing"), false, QObject::tr("Network helper is missing."));
        }

        lines << QObject::tr("Connected devices: %1").arg(devices.size());
        if (ok != nullptr) *ok = success;
        return lines.join('\n');
    }
};

#endif // Q_OS_WIN

// ----------------- Null implementation -----------------

class HotspotManagerNull final : public HotspotManager {
public:
    bool start(const QString &, const QString &, HotspotRuntimeInfo *, QString *error) override {
        if (error != nullptr) *error = QObject::tr("Hotspot is not supported on this platform.");
        return false;
    }
    bool stop(const HotspotRuntimeInfo &, QString *) override { return true; }
    bool status(HotspotRuntimeInfo *, QString *) override { return false; }
};

class DeviceManagerNull final : public DeviceManager {
public:
    void startWatch(const QString &, const QString &) override {}
    void stopWatch() override {}
    QVector<HotspotDeviceInfo> listDevices(QString *) override { return {}; }
};

class TrafficRouterNull final : public TrafficRouter {
public:
    bool applyFullTunnel(const HotspotRuntimeInfo &, QString *error) override {
        if (error != nullptr) *error = QObject::tr("Hotspot router is not supported on this platform.");
        return false;
    }
    bool clear(const HotspotRuntimeInfo &, QString *) override { return true; }
};

class DiagnosticsNull final : public HotspotDiagnostics {
public:
    QString run(const HotspotRuntimeInfo &, const QVector<HotspotDeviceInfo> &, bool *ok) override {
        if (ok != nullptr) *ok = false;
        return QObject::tr("Hotspot diagnostics is not supported on this platform.");
    }
};

} // namespace

HotspotGatewayService::HotspotGatewayService(QObject *parent) : QObject(parent) {
#ifdef Q_OS_LINUX
    hotspotManager_ = std::make_unique<HotspotManagerLinux>();
    deviceManager_ = std::make_unique<DeviceManagerLinux>();
    trafficRouter_ = std::make_unique<TrafficRouterLinux>();
    diagnostics_ = std::make_unique<DiagnosticsLinux>();
#elif defined(Q_OS_WIN)
    hotspotManager_ = std::make_unique<HotspotManagerWindows>();
    deviceManager_ = std::make_unique<DeviceManagerWindows>();
    trafficRouter_ = std::make_unique<TrafficRouterWindows>();
    diagnostics_ = std::make_unique<DiagnosticsWindows>();
#else
    hotspotManager_ = std::make_unique<HotspotManagerNull>();
    deviceManager_ = std::make_unique<DeviceManagerNull>();
    trafficRouter_ = std::make_unique<TrafficRouterNull>();
    diagnostics_ = std::make_unique<DiagnosticsNull>();
#endif

    runtime_.tunIf = tunDefaultName();
    devicePollTimer_ = new QTimer(this);
    devicePollTimer_->setInterval(2500);
    connect(devicePollTimer_, &QTimer::timeout, this, &HotspotGatewayService::pollDevices);
}

HotspotGatewayService::~HotspotGatewayService() {
    stop();
}

void HotspotGatewayService::setCredentials(const QString &ssid, const QString &password) {
    runtime_.ssid = ssid.trimmed();
    runtime_.password = password.trimmed();
    ensureCredentials();
    emit credentialsChanged(runtime_.ssid, maskedPassword());
}

void HotspotGatewayService::regenerateCredentials() {
    runtime_.ssid = QStringLiteral("CofeBox-%1").arg(randomSsidSuffix());
    runtime_.password = randomAlphaNum(12);
    emit credentialsChanged(runtime_.ssid, maskedPassword());
}

void HotspotGatewayService::ensureCredentials() {
    if (runtime_.ssid.trimmed().isEmpty()) {
        runtime_.ssid = QStringLiteral("CofeBox-%1").arg(randomSsidSuffix());
    }
    if (runtime_.password.trimmed().size() < 8) {
        runtime_.password = randomAlphaNum(12);
    }
}

bool HotspotGatewayService::start(Mode mode) {
    if (state_ == State::Starting || state_ == State::Running) return true;
    ensureCredentials();
    mode_ = mode;
    runtime_.tunIf = tunDefaultName();
    setState(State::Starting, QObject::tr("Starting hotspot gateway..."));

    QString error;
    if (!hotspotManager_->start(runtime_.ssid, runtime_.password, &runtime_, &error)) {
        setState(State::Failed, error.isEmpty() ? QObject::tr("Failed to start hotspot.") : error);
        return false;
    }

    if (runtime_.tunIf.trimmed().isEmpty()) runtime_.tunIf = tunDefaultName();
    if (!trafficRouter_->applyFullTunnel(runtime_, &error)) {
        hotspotManager_->stop(runtime_, nullptr);
        runtime_.active = false;
        setState(State::Failed, error.isEmpty() ? QObject::tr("Failed to apply hotspot full-tunnel routing.") : error);
        return false;
    }

    deviceManager_->startWatch(runtime_.apIf, runtime_.apCidr);
    pollDevices();
    devicePollTimer_->start();
    runtime_.active = true;
    setState(State::Running, QObject::tr("Hotspot gateway is running."));
    return true;
}

void HotspotGatewayService::stop() {
    if (state_ == State::Idle || state_ == State::Stopping) return;
    setState(State::Stopping, QObject::tr("Stopping hotspot gateway..."));
    devicePollTimer_->stop();
    deviceManager_->stopWatch();

    QString errorClear;
    trafficRouter_->clear(runtime_, &errorClear);
    QString errorStop;
    hotspotManager_->stop(runtime_, &errorStop);

    runtime_.active = false;
    runtime_.apIf.clear();
    runtime_.apCidr.clear();
    runtime_.gwIp.clear();
    runtime_.uplinkIf.clear();
    devices_.clear();
    emit devicesChanged(devices_);

    const auto msg = (!errorClear.isEmpty() || !errorStop.isEmpty())
                         ? QObject::tr("Hotspot stopped with cleanup warnings.")
                         : QObject::tr("Hotspot gateway is stopped.");
    setState(State::Idle, msg);
}

void HotspotGatewayService::runDiagnostics() {
    if (state_ != State::Running) {
        emit diagReport(false, QObject::tr("Hotspot is not running. Start hotspot first."));
        return;
    }

    HotspotRuntimeInfo snapshot = runtime_;
    QString statusError;
    if (hotspotManager_ != nullptr) {
        hotspotManager_->status(&snapshot, &statusError);
    }
    if (snapshot.apIf.trimmed().isEmpty()) {
        emit diagReport(false, QObject::tr("Hotspot interface was not detected. Restart hotspot and try again."));
        return;
    }
    if (snapshot.tunIf.trimmed().isEmpty()) {
        emit diagReport(false, QObject::tr("TUN interface was not detected. Enable TUN mode first."));
        return;
    }

    bool ok = false;
    const auto report = diagnostics_->run(snapshot, devices_, &ok);
    emit diagReport(ok, report);
}

HotspotGatewayService::State HotspotGatewayService::state() const {
    return state_;
}

HotspotGatewayService::Mode HotspotGatewayService::mode() const {
    return mode_;
}

HotspotRuntimeInfo HotspotGatewayService::runtime() const {
    return runtime_;
}

QVector<HotspotDeviceInfo> HotspotGatewayService::devices() const {
    return devices_;
}

QString HotspotGatewayService::lastMessage() const {
    return lastMessage_;
}

QString HotspotGatewayService::maskedPassword() const {
    return maskPassword(runtime_.password);
}

QString HotspotGatewayService::wifiQrText() const {
    // WIFI:T:WPA;S:<ssid>;P:<pass>;H:false;;
    return QStringLiteral("WIFI:T:WPA;S:%1;P:%2;H:false;;")
        .arg(runtime_.ssid, runtime_.password);
}

void HotspotGatewayService::setState(State state, const QString &message) {
    state_ = state;
    lastMessage_ = message;
    emit stateChanged(state_, message);
}

void HotspotGatewayService::pollDevices() {
    QString error;
    auto list = deviceManager_->listDevices(&error);
    if (!error.isEmpty() && state_ == State::Running) {
        lastMessage_ = error;
    }
    devices_ = list;
    emit devicesChanged(devices_);
}
